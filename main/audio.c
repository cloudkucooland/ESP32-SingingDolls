/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "sdkconfig.h"

#include "sd.h"

static i2s_chan_handle_t                tx_chan;        // I2S tx channel handler
static i2s_chan_handle_t                rx_chan;        // I2S rx channel handler

static void i2s_write_task(void *args) {
    int16_t *w_buf = (int16_t *)calloc(AUDIO_BUFFSIZE, sizeof(int16_t));
    assert(w_buf);
    size_t w_bytes = AUDIO_BUFFSIZE;

    /* Enable the TX channel */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    while (1) {
	synthTick(w_buf, AUDIO_BUFFSIZE);
        if (i2s_channel_write(tx_chan, w_buf, AUDIO_BUFFSIZE, &w_bytes, 1000) != ESP_OK) {
            printf("Write Task: i2s write failed\n");
            break;
        }
    }
    free(w_buf);
    vTaskDelete(NULL);
}

void start_audio(void) {
    // look into i2s_channel_register_event_callback()

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,    // some codecs may require mclk signal, this example doesn't need it
            .bclk = I2S_BCLK_IO,
            .ws   = I2S_WS_IO,
            .dout = I2S_DOUT_IO,
            .din  = I2S_DOUT_IO, // duplex
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    // ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));

    xTaskCreate(i2s_write_task, "i2s_write_task", 4096, NULL, 5, NULL);
}

void stop_audio(void) {
  i2s_channel_disable(tx_chan);
  // i2s_channel_disable(rx_chan);

  i2s_del_channel(tx_chan);
  i2s_del_channel(rx_chan);
}
