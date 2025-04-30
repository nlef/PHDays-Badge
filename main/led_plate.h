#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include <string.h>

#define IMAGE_MAX_FRAMES 8

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Pixel;

typedef struct {
    Pixel pixels[100];
    uint16_t duration;
} Frame;

typedef struct {
    Frame frames[IMAGE_MAX_FRAMES];
    uint8_t shiftMode;
    uint8_t framesCount;
} Image;

void showColor(uint8_t r, uint8_t g, uint8_t b);

void plateUpdateTask(void *pvParameters);

// void updateImageToShow(const Image image);
void updateImageToShowCustom();

void shiftBrightness();
void setTurboBrightness();

void switchLeds();

void switchCustom();

void showInt(uint64_t pass, uint8_t r, uint8_t g, uint8_t b);

uint8_t getSettedCustom();
void restoreSettedCustom(uint8_t state);

uint8_t getShowCustom();
void restoreShowCustom(uint8_t state);

// uint8_t *getImageToShow();
// void restoreImageToShow(const uint8_t pixels[]);

void updateLastActivity();
void switchAutoFade();

void switchLedsShiter();

Image *getImageToShowCustom();
SemaphoreHandle_t getShowFrameSemaphore();
void set_ota_display_image(uint8_t state);
void reset_showImage(bool tmp);
void set_power_display_image(uint8_t percent);
void save_img_custom();
