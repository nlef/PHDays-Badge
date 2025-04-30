#pragma once

#include "buttons.h"
#include "esp_log.h"
#include "led_plate.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "rest_server.h"
#include "wifi_utilss.h"

esp_err_t loadFromNVS();
void saveToNVS(void *pvParameters);
esp_err_t saveData();
