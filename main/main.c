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
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <mbedtls/md.h>
#include <stdio.h>

#include "bluetooth.h"
#include "buttons.h"
#include "dns_server.h"
#include "led_plate.h"
#include "rest_server.h"
#include "wifi_utilss.h"

#define STORAGE_NAMESPACE "storage"

static const char* TAG = "badge main";

esp_err_t saveData()
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    uint8_t settedCustom = 0;
    err = nvs_get_u8(my_handle, "setted_custom", &settedCustom);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_u8(my_handle, "setted_custom", settedCustom);
        if (err != ESP_OK)
            return err;
    } else {
        if (settedCustom != getSettedCustom()) {
            settedCustom = getSettedCustom();
            err = nvs_set_u8(my_handle, "setted_custom", settedCustom);
            if (err != ESP_OK)
                return err;
        }
    }

    uint8_t showCustom = 0;
    err = nvs_get_u8(my_handle, "show_custom", &showCustom);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_u8(my_handle, "show_custom", showCustom);
        if (err != ESP_OK)
            return err;
    } else {
        if (showCustom != getShowCustom()) {
            showCustom = getShowCustom();
            err = nvs_set_u8(my_handle, "show_custom", showCustom);
            if (err != ESP_OK)
                return err;
        }
    }

    uint8_t swebOpened = 0;
    err = nvs_get_u8(my_handle, "web_openned", &swebOpened);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_u8(my_handle, "web_openned", webOpenedCounter);
        if (err != ESP_OK)
            return err;
    } else {
        if (swebOpened != webOpenedCounter) {
            err = nvs_set_u8(my_handle, "web_openned", webOpenedCounter);
            if (err != ESP_OK)
                return err;
        }
    }

    uint8_t simageSetted = 0;
    err = nvs_get_u8(my_handle, "image_setted", &simageSetted);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_u8(my_handle, "image_setted", imageSetCounter);
        if (err != ESP_OK)
            return err;
    } else {
        if (simageSetted != imageSetCounter) {
            err = nvs_set_u8(my_handle, "image_setted", imageSetCounter);
            if (err != ESP_OK)
                return err;
        }
    }

    uint8_t sprojectSaved = 0;
    err = nvs_get_u8(my_handle, "project_saved", &sprojectSaved);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_u8(my_handle, "project_saved", projectSaveCounter);
        if (err != ESP_OK)
            return err;
    } else {
        if (sprojectSaved != projectSaveCounter) {
            err = nvs_set_u8(my_handle, "project_saved", projectSaveCounter);
            if (err != ESP_OK)
                return err;
        }
    }

    uint8_t sbrightnessSwitched = 0;
    err = nvs_get_u8(my_handle, "brightness_sw", &sbrightnessSwitched);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_u8(my_handle, "brightness_sw", brightnessSwitchCounter);
        if (err != ESP_OK)
            return err;
    } else {
        if (sbrightnessSwitched != brightnessSwitchCounter) {
            err = nvs_set_u8(my_handle, "brightness_sw", brightnessSwitchCounter);
            if (err != ESP_OK)
                return err;
        }
    }

    uint8_t sscreenOff = 0;
    err = nvs_get_u8(my_handle, "screen_off", &sscreenOff);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_u8(my_handle, "screen_off", screenOffCounter);
        if (err != ESP_OK)
            return err;
    } else {
        if (sscreenOff != screenOffCounter) {
            err = nvs_set_u8(my_handle, "screen_off", screenOffCounter);
            if (err != ESP_OK)
                return err;
        }
    }

    uint8_t smodeSwitched = 0;
    err = nvs_get_u8(my_handle, "mode_switched", &smodeSwitched);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_u8(my_handle, "mode_switched", modeSwitchCounter);
        if (err != ESP_OK)
            return err;
    } else {
        if (smodeSwitched != modeSwitchCounter) {
            err = nvs_set_u8(my_handle, "mode_switched", modeSwitchCounter);
            if (err != ESP_OK)
                return err;
        }
    }

    uint8_t swifiClientConnected = 0;
    err = nvs_get_u8(my_handle, "wifi_client_con", &swifiClientConnected);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_set_u8(my_handle, "wifi_client_con", wifiClientConnectCounter);
        if (err != ESP_OK)
            return err;
    } else {
        if (swifiClientConnected != wifiClientConnectCounter) {
            err = nvs_set_u8(my_handle, "wifi_client_con", wifiClientConnectCounter);
            if (err != ESP_OK)
                return err;
        }
    }

    size_t required_size = 0; // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "image_to_show", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;

    required_size = sizeof(uint8_t[300]);
    err = nvs_set_blob(my_handle, "image_to_show", getImageToShow(), required_size);
    if (err != ESP_OK)
        return err;

    ESP_LOGI(TAG, "end saveSave to nvs");
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        return err;

    nvs_close(my_handle);
    return ESP_OK;
}

void saveToNVS(void* pvParameters)
{
    while (1) {
        vTaskDelay(15000 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(saveData());
    }
}

esp_err_t loadFromNVS()
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    uint8_t swebOpened = 0;
    err = nvs_get_u8(my_handle, "web_openned", &swebOpened);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    webOpenedCounter = swebOpened;

    uint8_t simageSetted = 0;
    err = nvs_get_u8(my_handle, "image_setted", &simageSetted);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    imageSetCounter = simageSetted;

    uint8_t sprojectSaved = 0;
    err = nvs_get_u8(my_handle, "project_saved", &sprojectSaved);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    projectSaveCounter = sprojectSaved;

    uint8_t sbrightnessSwitched = 0;
    err = nvs_get_u8(my_handle, "brightness_sw", &sbrightnessSwitched);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    brightnessSwitchCounter = sbrightnessSwitched;

    uint8_t sscreenOff = 0;
    err = nvs_get_u8(my_handle, "screen_off", &sscreenOff);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    screenOffCounter = sscreenOff;

    uint8_t smodeSwitched = 0;
    err = nvs_get_u8(my_handle, "mode_switched", &smodeSwitched);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    modeSwitchCounter = smodeSwitched;

    uint8_t swifiClientConnected = 0;
    err = nvs_get_u8(my_handle, "wifi_client_con", &swifiClientConnected);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;
    wifiClientConnectCounter = swifiClientConnected;

    size_t required_size = 0; // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "image_to_show", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;

    if (required_size > 0) {
        // maybe remove malloc and use array?
        uint8_t* pixels = malloc(sizeof(uint8_t[300]));
        err = nvs_get_blob(my_handle, "image_to_show", pixels, &required_size);
        if (err != ESP_OK) {
            free(pixels);
            return err;
        }
        restoreImageToShow(pixels);
        free(pixels);
    }

    uint8_t settedCustom = 0;
    err = nvs_get_u8(my_handle, "setted_custom", &settedCustom);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;

    restoreSettedCustom(settedCustom);

    uint8_t showCustom = 0;
    err = nvs_get_u8(my_handle, "show_custom", &showCustom);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;

    restoreShowCustom(showCustom);

    nvs_close(my_handle);
    return ESP_OK;
}

void app_main(void)
{
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

    ret = loadFromNVS();
    ESP_ERROR_CHECK(ret);

    TaskHandle_t xHandle = NULL;
    xTaskCreatePinnedToCore(plateUpdateTask, "ledPlate", 4096, NULL, 2, &xHandle, 1);

    if (xHandle == NULL) {
        ESP_LOGE("TASK1", "Failed to task create");
    }

    initWiFi();

    /* Initialize file storage */
    const char* base_path = "/data";
    ESP_ERROR_CHECK(mount_storage(base_path));

    /* Start the file server */
    ESP_ERROR_CHECK(start_file_server(base_path));
    ESP_LOGI(TAG, "File server started");

    initButtons();

    fflush(stdout);

    xTaskCreate(saveToNVS, "saveToNVS", 4096, NULL, 5, &xHandle);

    if (xHandle == NULL) {
        ESP_LOGE("TASK2", "Failed to create saveToNVS task");
    }

    init_bluetooth();

    xTaskCreate(updateAdvData, "updateAdvData", 4096, NULL, 5, &xHandle);

    if (xHandle == NULL) {
        ESP_LOGE("TASK3", "Failed to create saveToNVS task");
    }

    dns_server_config_t config = { .num_of_entries = 1, .item = { { .name = "*", .ip = { .addr = ESP_IP4TOADDR(192, 168, 4, 1) } } } };
    start_dns_server(&config);
    updateLastActivity();
}
