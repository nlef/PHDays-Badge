#pragma once

#include "esp_err.h"
#include "sdkconfig.h"

extern uint8_t webOpenedCounter;
extern uint8_t imageSetCounter;
extern uint8_t projectSaveCounter;

esp_err_t start_file_server(const char *base_path);
esp_err_t mount_storage(const char *base_path);
