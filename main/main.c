#include <string.h>
#include <esp_system.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_ble_conn_mgr.h>
#include <esp_ble_midi.h>
#include <esp_ble_midi_svc.h>

#include "sd.h"

static const char *TAG = "SingingDoll";

void midi_evt_cb(uint16_t, esp_ble_midi_event_type_t, const unsigned char*, uint16_t);
void app_ble_conn_event_handler(void*, esp_event_base_t, int32_t , void*);

void app_main(void) {
    esp_err_t ret;
    initTables();

    static const uint8_t midi_service_uuid128[] = BLE_MIDI_SERVICE_UUID128;
    esp_ble_conn_config_t config = {
        .device_name = "SD",
        .broadcast_data = "1",
        .include_service_uuid = 1,                    /* Include service UUID in advertising */
        .adv_uuid_type = BLE_CONN_UUID_TYPE_128,      /* Use 128-bit UUID */
    };
    memcpy(config.adv_uuid128, midi_service_uuid128, sizeof(config.adv_uuid128));

    // required for BT to start
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler, NULL));

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    /* Prefer a larger ATT MTU for better BLE-MIDI throughput */
    ESP_ERROR_CHECK(esp_ble_conn_set_mtu(256));
    ESP_ERROR_CHECK(esp_ble_midi_svc_init()); /* Register BLE-MIDI service and IO characteristic */
    ESP_ERROR_CHECK(esp_ble_midi_profile_init()); /* Initialize BLE MIDI profile (must be called before registering callbacks) */
    // ESP_ERROR_CHECK(esp_ble_midi_register_rx_cb(midi_rx_cb, NULL)); /* Register RX callback with NULL user context */
    ESP_ERROR_CHECK(esp_ble_midi_register_event_cb(midi_evt_cb));

    if (esp_ble_conn_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager");
        esp_ble_midi_profile_deinit();
        esp_ble_midi_svc_deinit();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        return;
    }

    ESP_LOGI(TAG, "running...");
}
