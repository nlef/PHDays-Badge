#pragma once

#include <stdint.h>

extern uint8_t wifiClientConnectCounter;

extern uint8_t ESP_WIFI_SSID_bytes[4];
extern uint32_t ESP_WIFI_SSID_INT;
extern uint64_t ESP_WIFI_PASS_LONG;

void initWiFi();
