#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_pdm.h"
#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "driver/gpio.h"

#include "sd.h"

static i2s_chan_handle_t tx_chan;

static void i2s_write_task(void *args) {
    int32_t *w_buf = (int32_t *)heap_caps_malloc(AUDIO_BUFFSIZE * 2 * sizeof(int32_t), MALLOC_CAP_DMA);
    assert(w_buf);
    size_t bytes_to_write = AUDIO_BUFFSIZE * 2 * sizeof(int32_t);
    unsigned int check;

    /* Enable the TX channel */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    while (1) {
	synth_tick(w_buf);
        if (i2s_channel_write(tx_chan, w_buf, bytes_to_write, &check, portMAX_DELAY) != ESP_OK) {
            printf("Write Task: i2s write failed\n");
            break;
        }
        if (check != bytes_to_write) {
            printf("write wrong number of bytes: %d/%d\n", check, bytes_to_write);
        }
	taskYIELD();
    }

    free(w_buf);
    vTaskDelete(NULL);
}

void start_audio(void) {
	i2s_chan_config_t chan_cfg = {
		.id = I2S_NUM_AUTO,
		.role = I2S_ROLE_MASTER,
		.dma_desc_num = 12, // increased
		.dma_frame_num = 511, // 511 is hardware's max
		.auto_clear = true
	};
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCLK_IO,
            .bclk = I2S_BCLK_IO,
            .ws   = I2S_WS_IO,
            .dout = I2S_DOUT_IO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    xTaskCreate(i2s_write_task, "i2s_write_task", 4096, NULL, 5, NULL);
}

void stop_audio(void) {
  ESP_ERROR_CHECK(i2s_channel_disable(tx_chan));
  ESP_ERROR_CHECK(i2s_del_channel(tx_chan));
}
