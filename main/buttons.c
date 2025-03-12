#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"
#include "led_plate.h"

#include "buttons.h"
#include "wifi_utilss.h"

uint8_t brightnessSwitchCounter = 0;
uint8_t screenOffCounter = 0;
uint8_t modeSwitchCounter = 0;

static const char* TAG = "buttons";

static void button_single_click_bright_cb(void* arg, void* usr_data)
{
    if (brightnessSwitchCounter < 255)
        brightnessSwitchCounter++;
    shiftBrightness();
}

static void button_triple_click_bright_cb(void* arg, void* usr_data) { switchAutoFade(); }
static void button_quad_click_bright_cb(void* arg, void* usr_data) { setTurboBrightness(); }

static void button_single_click_leds_cb(void* arg, void* usr_data)
{
    if (screenOffCounter < 255)
        screenOffCounter++;
    switchLeds();
}

static void button_press_leds_cb(void* arg, void* usr_data)
{
    if (modeSwitchCounter < 255)
        modeSwitchCounter++;
    switchCustom();
}
static void button_triple_click_leds_cb(void* arg, void* usr_data) { switchLedsShiter(); }

void showSsidTask(void* pvParameters)
{
    showInt(ESP_WIFI_SSID_INT, 255, 255, 255);
    vTaskDelete(NULL);
}
void showPswdTask(void* pvParameters)
{
    showInt(ESP_WIFI_PASS_INT, 255, 0, 0);
    vTaskDelete(NULL);
}

static void button_single_click_ssid_cb(void* arg, void* usr_data)
{
    TaskHandle_t xHandle = NULL;
    xTaskCreate(showSsidTask, "showSsid", 4096, NULL, 5, &xHandle);
    if (xHandle == NULL) {
        ESP_LOGE("showSsid", "Failed to task create");
    };
}
static void button_press_pass_cb(void* arg, void* usr_data)
{
    TaskHandle_t xHandle = NULL;
    xTaskCreate(showPswdTask, "showPswd", 4096, NULL, 5, &xHandle);
    if (xHandle == NULL) {
        ESP_LOGE("showPswd", "Failed to task create");
    };
}

void initButtons()
{
    button_config_t gpio_btn_bright_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 17,
            .active_level = 0,
        },
    };
    button_handle_t gpio_btn_bright = iot_button_create(&gpio_btn_bright_cfg);
    if (NULL == gpio_btn_bright) {
        ESP_LOGE(TAG, "Button create failed");
    }

    iot_button_register_cb(gpio_btn_bright, BUTTON_SINGLE_CLICK, button_single_click_bright_cb, NULL);
    button_event_config_t quad_cfg_bright = { .event = BUTTON_MULTIPLE_CLICK, .event_data.multiple_clicks.clicks = 4 };
    iot_button_register_event_cb(gpio_btn_bright, quad_cfg_bright, button_quad_click_bright_cb, NULL);
    button_event_config_t triple_cfg_bright = { .event = BUTTON_MULTIPLE_CLICK, .event_data.multiple_clicks.clicks = 3 };
    iot_button_register_event_cb(gpio_btn_bright, triple_cfg_bright, button_triple_click_bright_cb, NULL);

    button_config_t gpio_btn_pass_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 2,
            .active_level = 0,
        },
    };
    button_handle_t gpio_btn_pass = iot_button_create(&gpio_btn_pass_cfg);
    if (NULL == gpio_btn_pass) {
        ESP_LOGE(TAG, "Button create failed");
    }

    iot_button_register_cb(gpio_btn_pass, BUTTON_SINGLE_CLICK, button_single_click_ssid_cb, NULL);
    iot_button_register_cb(gpio_btn_pass, BUTTON_LONG_PRESS_START, button_press_pass_cb, NULL);

    button_config_t gpio_btn_leds_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 16,
            .active_level = 0,
        },
    };
    button_handle_t gpio_btn_leds = iot_button_create(&gpio_btn_leds_cfg);
    if (NULL == gpio_btn_leds) {
        ESP_LOGE(TAG, "Button create failed");
    }

    iot_button_register_cb(gpio_btn_leds, BUTTON_SINGLE_CLICK, button_single_click_leds_cb, NULL);
    iot_button_register_cb(gpio_btn_leds, BUTTON_LONG_PRESS_START, button_press_leds_cb, NULL);

    button_event_config_t triple_cfg_leds = { .event = BUTTON_MULTIPLE_CLICK, .event_data.multiple_clicks.clicks = 3 };
    iot_button_register_event_cb(gpio_btn_leds, triple_cfg_leds, button_triple_click_leds_cb, NULL);
}