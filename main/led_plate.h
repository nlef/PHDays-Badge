#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ws2812_control.h"

void showTestColor(int r, int g, int b);

void plateUpdateTask(void* pvParameters);

void updateImageToShow(uint8_t pixels[]);

void shiftBrightness();
void setTurboBrightness();

void switchLeds();

void switchCustom();

void showInt(uint32_t pass, int r, int g, int b);

uint8_t getSettedCustom();
void restoreSettedCustom(uint8_t state);

uint8_t getShowCustom();
void restoreShowCustom(uint8_t state);

uint8_t* getImageToShow();
void restoreImageToShow(uint8_t pixels[]);

void updateLastActivity();
void switchAutoFade();

void switchLedsShiter();