#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <mbedtls/md.h>
#include <stdio.h>

#include "adc_utils.h"
#include "buttons.h"
#include "buzzer.h"
#include "dns_server.h"
#include "led_plate.h"
#include "nvs_utils.h"
#include "ota.h"
#include "rest_server.h"
#include "wifi_utilss.h"

static const char *TAG = "badge main";

void app_main(void) {
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is %s chip with %d CPU core(s), %s%s%s%s, ", CONFIG_IDF_TARGET, chip_info.cores, (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
             (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "", (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
             (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    ESP_LOGI(TAG, "silicon revision v%d.%d, ", major_rev, minor_rev);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Get flash size failed");
        return;
    }

    ESP_LOGI(TAG, "%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024), (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE("alalal", "eraing nvs in main");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize file storage */
    const char *base_path = "/data";
    ESP_ERROR_CHECK(mount_storage(base_path));

    ret = loadFromNVS();
    ESP_ERROR_CHECK(ret);

    TaskHandle_t xHandle = NULL;
    xTaskCreatePinnedToCore(plateUpdateTask, "ledPlate", 4096, NULL, 2, &xHandle, 1);

    if (xHandle == NULL) {
        ESP_LOGE("TASK1", "Failed to task create");
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    initWiFi();

    /* Start the file server */
    ESP_ERROR_CHECK(start_file_server(base_path));
    ESP_LOGI(TAG, "File server started");

    initButtons();

    xTaskCreate(saveToNVS, "saveToNVS", 4096, NULL, 5, &xHandle);

    if (xHandle == NULL) {
        ESP_LOGE("TASK2", "Failed to create saveToNVS task");
    }

    // Start the DNS server that will redirect all queries to the softAP IP
    // dns_server_config_t config = { .num_of_entries = 1, .item = { { .name = "my-badge.local", .ip = { .addr = ESP_IP4TOADDR(192, 168, 4, 1) } } } };
    dns_server_config_t config = {.num_of_entries = 1, .item = {{.name = "*", .ip = {.addr = ESP_IP4TOADDR(192, 168, 4, 1)}}}};
    start_dns_server(&config);
    updateLastActivity();

    xTaskCreate(ota_verify_after_update, "ota_verify_after_update", 4096, NULL, 5, &xHandle);

    if (xHandle == NULL) {
        ESP_LOGE("TASK4", "Failed to create ota_verify_after_update task");
    }

    // TOdo: Fix ota prority!
    xTaskCreate(ota_periodic_updates, "ota_periodic_updates", 4096, NULL, 5, &xHandle);

    if (xHandle == NULL) {
        ESP_LOGE("TASK5", "Failed to create ota_periodic_updates task");
    }

    init_buzzer();

    init_adc();
    xTaskCreate(sample_adc, "sample_adc", 4096, NULL, 6, &xHandle);

    if (xHandle == NULL) {
        ESP_LOGE("TASK5", "Failed to create sample_adc task");
    }
}
