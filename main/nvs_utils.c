#include "nvs_utils.h"

#define STORAGE_NAMESPACE "storage"

static const char *TAG = "nvs";

esp_err_t saveData() {
    nvs_handle_t my_handle;
    esp_err_t err;

    ESP_LOGI(TAG, "start saveSave to nvs");

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

    required_size = sizeof(Image);

    SemaphoreHandle_t showFrameSemaphore = getShowFrameSemaphore();
    ESP_LOGI(TAG, "saveToNVS image_to_show required_size: %i", required_size);
    if (showFrameSemaphore != NULL && xSemaphoreTake(showFrameSemaphore, portMAX_DELAY) == pdTRUE) {
        err = nvs_set_blob(my_handle, "image_to_show", getImageToShowCustom(), required_size);
        if (err != ESP_OK)
            return err;
        xSemaphoreGive(showFrameSemaphore);
    }

    ESP_LOGI(TAG, "end saveSave to nvs");
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        return err;

    nvs_close(my_handle);
    return ESP_OK;
}

void saveToNVS(void *pvParameters) {
    while (1) {
        vTaskDelay(15000 / portTICK_PERIOD_MS);
        // TOdo: add semaphore locks on written data
        ESP_ERROR_CHECK(saveData());
    }
}

esp_err_t loadFromNVS() {
    nvs_handle_t my_handle;
    esp_err_t err;

    ESP_LOGI(TAG, "start loadFromNVS to nvs");

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
        // Image *img = malloc(sizeof(Image));
        ESP_LOGI(TAG, "loadFromNVS image_to_show required_size: %i", required_size);
        // Image *img = getImageToShowCustom();
        err = nvs_get_blob(my_handle, "image_to_show", getImageToShowCustom(), &required_size);
        if (err != ESP_OK) {
            // free(img);
            return err;
        }
        ESP_LOGI(TAG, "loadFromNVS image_to_show loaded framesCount: %i", getImageToShowCustom()->framesCount);
        // restoreImageToShow(img);
        // free(img);
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

    ESP_LOGI(TAG, "end  loadFromNVS to nvs");
    nvs_close(my_handle);
    return ESP_OK;
}
