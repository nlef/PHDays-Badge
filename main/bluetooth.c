#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bluetooth.h"
#include "buttons.h"
#include "rest_server.h"
#include "wifi_utilss.h"

static const char* TAG = "bluetooth";

#define SAMPLE_DEVICE_NAME "phd2 badge"

static uint8_t raw_scan_rsp_data[] = {
    /* flags */
    0x02, 0x01, 0x06,
    /* tx power */
    0x02, 0x0a, 0xeb,
    /* service uuid */
    0x03, 0x03, 0xFF, 0x00
};

uint8_t raw_adv_data[30] = {

    0x02,
    0x01,
    0x06,

    0x02,
    0x0a,
    0xeb,
    0x16,
    0x09,

};

static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min = 0x0250,
    .adv_int_max = 0x0320,
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

void updateAdvData(void* pvParameters)
{
    while (1) {
        raw_adv_data[8] = webOpenedCounter;
        raw_adv_data[9] = imageSetCounter;
        raw_adv_data[10] = projectSaveCounter;
        raw_adv_data[11] = brightnessSwitchCounter;
        raw_adv_data[12] = screenOffCounter;
        raw_adv_data[13] = modeSwitchCounter;
        raw_adv_data[14] = wifiClientConnectCounter;

        uint32_t run_time = (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000;
        raw_adv_data[15] = (run_time & 0x000000ff);
        raw_adv_data[16] = (run_time & 0x0000ff00) >> 8;
        raw_adv_data[17] = (run_time & 0x00ff0000) >> 16;
        raw_adv_data[18] = (run_time & 0xff000000) >> 24;

        esp_err_t ret;
        uint8_t mac_base[6] = { 0 };
        ret = esp_efuse_mac_get_default(mac_base);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get base MAC address from EFUSE BLK3. (%s)", esp_err_to_name(ret));
        }

        raw_adv_data[19] = mac_base[0];
        raw_adv_data[20] = mac_base[1];
        raw_adv_data[21] = mac_base[2];
        raw_adv_data[22] = mac_base[3];
        raw_adv_data[23] = mac_base[4];
        raw_adv_data[24] = mac_base[5];

        // SSID!
        raw_adv_data[25] = ESP_WIFI_SSID_bytes[0];
        raw_adv_data[26] = ESP_WIFI_SSID_bytes[1];
        raw_adv_data[27] = ESP_WIFI_SSID_bytes[2];
        raw_adv_data[28] = ESP_WIFI_SSID_bytes[3];

        ret = esp_ble_gap_config_adv_data_raw((uint8_t*)&raw_adv_data, sizeof(raw_adv_data));
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "Set adv_raw failed : %s", esp_err_to_name(ret));
        }

        ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "Set scan_raw failed : %s", esp_err_to_name(ret));
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
        esp_ble_gap_start_advertising(&ble_adv_params);
        ESP_LOGI(TAG, "start ble adv after data update");
        break;
    }
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&ble_adv_params);
        break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {

        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        // scan start complete event to indicate scan start successfully or failed
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(err));
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // adv start complete event to indicate adv start successfully or failed
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Adv start failed: %s", esp_err_to_name(err));
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if ((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scan stop failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Stop scan successfully");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Adv stop failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Stop adv successfully");
        }
        break;

    default:
        break;
    }
}

void ble_ibeacon_appRegister(void)
{
    esp_err_t status;

    ESP_LOGI(TAG, "register callback");

    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(TAG, "gap register error: %s", esp_err_to_name(status));
        return;
    }
}

void ble_ibeacon_init(void)
{
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    esp_bluedroid_enable();
    ble_ibeacon_appRegister();
    esp_ble_gap_set_device_name("phd2 badge");
}

void init_bluetooth()
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    ble_ibeacon_init();
}