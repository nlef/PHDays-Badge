#pragma once

#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef struct Buzzer {
    ledc_mode_t speed_mode;
    ledc_timer_bit_t timer_bit;
    ledc_timer_t timer_num;
    ledc_channel_t channel;
    uint32_t idle_level;
    TaskHandle_t task_handle;
    QueueHandle_t queue_handle;
} Buzzer;

typedef struct message_t {
    uint32_t frequency;
    uint32_t duration_ms;
} message_t;

void init_buzzer();
int parse_rtttl(const char *rtttl, uint16_t len);
void disable_buzzer();
void enable_buzzer();
