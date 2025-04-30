#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "button_gpio.h"
#include "iot_button.h"

#include "adc_utils.h"
#include "buttons.h"
#include "led_plate.h"
#include "wifi_utilss.h"

#define BUTTON_ACTIVE_LEVEL 0

uint8_t brightnessSwitchCounter = 0;
uint8_t screenOffCounter = 0;
uint8_t modeSwitchCounter = 0;

static const char *TAG = "buttons";

static void button_click_bright_cb(void *arg, void *usr_data) {
    button_event_t event = iot_button_get_event(arg);
    if ((event == BUTTON_SINGLE_CLICK || event == BUTTON_MULTIPLE_CLICK) && brightnessSwitchCounter < 255)
        brightnessSwitchCounter++;

    if (event == BUTTON_SINGLE_CLICK)
        shiftBrightness();

    if (event == BUTTON_MULTIPLE_CLICK && (int)usr_data == 3)
        switchAutoFade();

    if (event == BUTTON_MULTIPLE_CLICK && (int)usr_data == 4)
        setTurboBrightness();
}

static void button_single_click_leds_cb(void *arg, void *usr_data) {
    if (screenOffCounter < 255)
        screenOffCounter++;
    switchLeds();
}

static void button_press_leds_cb(void *arg, void *usr_data) {
    if (modeSwitchCounter < 255)
        modeSwitchCounter++;
    switchCustom();
}
static void button_triple_click_leds_cb(void *arg, void *usr_data) { switchLedsShiter(); }

void showSsidTask(void *pvParameters) {
    showInt(ESP_WIFI_SSID_INT, 255, 255, 255);
    vTaskDelete(NULL);
}
void showPswdTask(void *pvParameters) {
    showInt(ESP_WIFI_PASS_LONG, 255, 0, 0);
    vTaskDelete(NULL);
}

static void button_single_click_ssid_cb(void *arg, void *usr_data) {
    ESP_LOGI(TAG, "button_single_click_ssid_cb");
    TaskHandle_t xHandle = NULL;
    xTaskCreate(showSsidTask, "showSsid", 4096, NULL, 5, &xHandle);
    if (xHandle == NULL) {
        ESP_LOGE(TAG, "Failed to create task `showSsid`");
    };
}
static void button_press_pass_cb(void *arg, void *usr_data) {
    ESP_LOGI(TAG, "button_press_pass_cb");
    TaskHandle_t xHandle = NULL;
    xTaskCreate(showPswdTask, "showPswd", 4096, NULL, 5, &xHandle);
    if (xHandle == NULL) {
        ESP_LOGE(TAG, "Failed to create task `showPswd`");
    };
}

static void button_double_click_pass_cb(void *arg, void *usr_data) {
    uint8_t percent = get_battery_level_percent();
    ESP_LOGI(TAG, "button_double_click_pass_cb PERCENT: %d", percent);
    bool showCustomTmp = getShowCustom();
    if (showCustomTmp) {
        save_img_custom();
    }
    set_power_display_image(percent);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    reset_showImage(showCustomTmp);
}

void initButtons() {
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t gpio_btn_bright_cfg = {
        .gpio_num = CONFIG_BRIGHT_BUTTON,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };

    button_handle_t gpio_btn_bright = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &gpio_btn_bright_cfg, &gpio_btn_bright);
    ESP_ERROR_CHECK(ret);

    button_event_args_t args = {
        .multiple_clicks.clicks = 3,
    };
    iot_button_register_cb(gpio_btn_bright, BUTTON_SINGLE_CLICK, NULL, button_click_bright_cb, NULL);
    iot_button_register_cb(gpio_btn_bright, BUTTON_MULTIPLE_CLICK, &args, button_click_bright_cb, (void *)3);
    args.multiple_clicks.clicks = 4;
    iot_button_register_cb(gpio_btn_bright, BUTTON_MULTIPLE_CLICK, &args, button_click_bright_cb, (void *)4);

    const button_gpio_config_t gpio_btn_pass_cfg = {
        .gpio_num = CONFIG_PASS_BUTTON,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };

    button_handle_t gpio_btn_pass = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &gpio_btn_pass_cfg, &gpio_btn_pass);
    ESP_ERROR_CHECK(ret);

    button_event_args_t args_pass = {
        .multiple_clicks.clicks = 2,
    };

    iot_button_register_cb(gpio_btn_pass, BUTTON_SINGLE_CLICK, NULL, button_single_click_ssid_cb, NULL);
    iot_button_register_cb(gpio_btn_pass, BUTTON_LONG_PRESS_START, NULL, button_press_pass_cb, NULL);
    iot_button_register_cb(gpio_btn_pass, BUTTON_MULTIPLE_CLICK, &args_pass, button_double_click_pass_cb, NULL);

    const button_gpio_config_t gpio_btn_leds_cfg = {
        .gpio_num = CONFIG_LEDS_BUTTON,
        .active_level = BUTTON_ACTIVE_LEVEL,
    };

    button_handle_t gpio_btn_leds = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &gpio_btn_leds_cfg, &gpio_btn_leds);
    ESP_ERROR_CHECK(ret);

    button_event_args_t args_leds = {
        .multiple_clicks.clicks = 3,
    };
    iot_button_register_cb(gpio_btn_leds, BUTTON_SINGLE_CLICK, NULL, button_single_click_leds_cb, NULL);
    iot_button_register_cb(gpio_btn_leds, BUTTON_LONG_PRESS_START, NULL, button_press_leds_cb, NULL);
    iot_button_register_cb(gpio_btn_leds, BUTTON_MULTIPLE_CLICK, &args_leds, button_triple_click_leds_cb, (void *)3);
}
