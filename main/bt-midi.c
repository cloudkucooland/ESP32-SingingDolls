#include <string.h>
#include <math.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ble_conn_mgr.h"
#include "esp_ble_midi.h"
#include "esp_ble_midi_svc.h"

#include "sd.h"

static const char *TAG = "SingingDoll";

static float attackTable[128];
static float decayTable[128];
static float releaseTable[128];
static float freqTable[128];
static float resoTable[128];

static void conn_param_update_task(void *arg) {
    uint16_t conn_handle = *(uint16_t *)arg;
    free(arg);

    /* Wait a bit for service discovery to complete */
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_ble_conn_params_t params = {
        .itvl_min = 0x0006,
        .itvl_max = 0x0006,
        .latency  = 0x0000,
        .supervision_timeout = 400,
        .min_ce_len = 0,
        .max_ce_len = 0,
    };
    esp_err_t rc = esp_ble_conn_update_params(conn_handle, &params);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "esp_ble_conn_update_params rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Connection parameters update requested");
    }

    vTaskDelete(NULL);
}

void midi_evt_cb(uint16_t ts_ms, esp_ble_midi_event_type_t event_type, const uint8_t *msg, uint16_t msg_len) {
    if (event_type == ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW) {
        ESP_LOGW(TAG, "MIDI EVT ts=%u: SysEx buffer overflow detected", ts_ms);
        return;
    }

    if (msg_len < 1) return;
    switch(msg[0]) {
    case 0x90:
	if (msg_len < 3) break;
        if (msg[2] == 0) noteOff(msg[1]);
        // ESP_LOGI(TAG, "Note On: %02X %02X\n", msg[1], msg[2]);
	noteOn(msg[1], msg[2]);
        break;
    case 0x80:
	if (msg_len < 2) break;
        // ESP_LOGI(TAG, "Note Off: %02X\n", msg[1]);
	noteOff(msg[1]);
        break;
    case 0xB0: // CC
	if (msg_len < 3) break;
    	switch(msg[1]) {
	case 0x47: // filter resonance 
		setFilterResonance(resoTable[msg[2]]);
		break;
	case 0x48: // filter release
		setFilterRelease(releaseTable[msg[2]]);
		break;
	case 0x49: // filter attack
		setFilterAttack(attackTable[msg[2]]);
		break;
	case 0x4A: // filter freq
		setFilterFreq(freqTable[msg[2]]);
		break;
	case 0x4B: // filter decay
		setFilterDecay(decayTable[msg[2]]);
		break;
	default:
        	ESP_LOGI(TAG, "Other CC: %02X\n", msg[1]);
	}
	break;
    default:
        // ESP_LOGI(TAG, "Something Else: %02X\n", msg[0]);
        char line[96] = {0};
        size_t pos = 0;
        for (int i = 0; i < msg_len; i++) {
            int written = snprintf(&line[pos], sizeof(line) - pos, "%02X ", msg[i]);
            if (written > 0) {
                pos += written;
                if (pos >= sizeof(line)) {
                    pos = sizeof(line) - 1;
                }
            }
            if (pos > (sizeof(line) - 4)) {
                ESP_LOGI(TAG, "%s", line);
                pos = 0;
                line[0] = 0;
            }
        }
        if (pos) {
            ESP_LOGI(TAG, "%s", line);
        }
        break;
    }
}

void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data) {
    if (base != BLE_CONN_MGR_EVENTS) {
    	ESP_LOGI(TAG, "Ignored MGR_EVENT base=%d", base);
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED");

        esp_ble_midi_set_notify_enabled(false);
        /* Ask for low-latency params via conn_mgr: 7.5 ms, latency 0, timeout 4 s.
         * The central has the final say and may refuse or adjust. */
        {
            uint16_t conn_handle = 0;
            if (esp_ble_conn_get_conn_handle(&conn_handle) == ESP_OK && conn_handle != 0) {
                /* Create a task to delay the parameter update request */
                uint16_t *handle_ptr = (uint16_t *)malloc(sizeof(uint16_t));
                if (handle_ptr) {
                    *handle_ptr = conn_handle;
                    BaseType_t task_result = xTaskCreate(conn_param_update_task, "conn_param_update", 4096, handle_ptr, 5, NULL);
                    if (task_result != pdPASS) {
                        ESP_LOGW(TAG, "Failed to create conn_param_update task");
                        free(handle_ptr);
                    }
		}
            }
        }

        ESP_LOGI(TAG, "starting leaf");
        start_leaf();

        ESP_LOGI(TAG, "starting i2s audio");
        start_audio();
        break;
    case ESP_BLE_CONN_EVENT_CCCD_UPDATE: {
        esp_ble_conn_cccd_update_t *cccd_update = (esp_ble_conn_cccd_update_t *)event_data;
        ESP_LOGI(TAG, "CCCD updated: notify=%d, indicate=%d for char handle=%d",
                 cccd_update->notify_enable, cccd_update->indicate_enable, cccd_update->char_handle);

        /* Check if this is the MIDI characteristic CCCD */
        static const uint8_t midi_char_uuid128[] = BLE_MIDI_CHAR_UUID128;
        bool is_midi_char = false;

        if (cccd_update->uuid_type == BLE_CONN_UUID_TYPE_128) {
            is_midi_char = (memcmp(cccd_update->uuid.uuid128, midi_char_uuid128, sizeof(midi_char_uuid128)) == 0);
        }

        if (is_midi_char) {
            if (cccd_update->notify_enable) {
                esp_ble_midi_set_notify_enabled(true);
            } else if (!cccd_update->notify_enable) {
                esp_ble_midi_set_notify_enabled(false);
            }
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED");
        /* Reset notification and indication state on disconnect */
        esp_ble_midi_set_notify_enabled(false);
        stop_audio();
        stop_leaf();
        break;
    default:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_UNKNOWN");
        break;
    }
}

void initTables() {
	const float minRelease = 0.01f;
	const float maxRelease = 5.0f;
	const float minAttack = 0.01f;
	const float maxAttack = 3.0f;
	const float minFreq = 60.0f; // no need for super-low on small speakers
	const float maxFreq = 10000.0f; // no need for highs, we are running at 24kHz
	const float minDecay = 0.01f;
	const float maxDecay = 3.0f;
	const float minReso = 0.01f;
	const float maxReso = 0.9f;

	for (int i = 0; i < 128; i++) {
		float ratio = i / 127.0f; // only called at startup, doesn't need micro-optimization

		attackTable[i] = minAttack * powf(maxAttack / minAttack, ratio);
		decayTable[i] = minDecay * powf(maxDecay / minDecay, ratio);
		releaseTable[i] = minRelease * powf(maxRelease / minRelease, ratio);
		freqTable[i] = minFreq * powf(maxFreq / minFreq, ratio);
		resoTable[i] = minReso + (ratio * ratio) * (maxReso - minReso);
	}
}
