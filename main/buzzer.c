#include "buzzer.h"
#include "esp_log.h"
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOTE_BUFFER_SIZE 1024
#define MAX_NOTES 100

static bool is_buzzer = true;

static const char *kTag = "buzzer";

static const uint32_t kQueueSize = 100;
static const uint32_t kStackSize = 2048;

Buzzer buzzer_handler;

typedef struct {
    uint8_t duration;
    uint8_t octave;
    bool dotted;
    bool sharp;
    char note;
} Note;

Note notes[MAX_NOTES];
char notes_buf[NOTE_BUFFER_SIZE];

static void buzzer_stop(Buzzer *buzzer) {
    BaseType_t err = ledc_set_duty(buzzer->speed_mode, buzzer->channel, 0);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to set duty cycle to zero");
    }
    err = ledc_update_duty(buzzer->speed_mode, buzzer->channel);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to update duty cycle");
    }
    err = ledc_stop(buzzer->speed_mode, buzzer->channel, buzzer->idle_level);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to stop buzzer");
    }
}

static void buzzer_start(Buzzer *buzzer, uint32_t frequency) {
    uint32_t duty = (1 << buzzer->timer_bit) / 2;

    esp_err_t err = ledc_set_duty(buzzer->speed_mode, buzzer->channel, duty);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to set duty cycle");
        return;
    }
    err = ledc_update_duty(buzzer->speed_mode, buzzer->channel);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to update duty cycle");
        return;
    }
    err = ledc_set_freq(buzzer->speed_mode, buzzer->timer_num, frequency);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to set frequency");
        return;
    }
}

void buzzer_task(void *param) {
    Buzzer *buzzer = (Buzzer *)param;
    message_t message;
    TickType_t waitTime = portMAX_DELAY;

    while (1) {
        BaseType_t res = xQueueReceive(buzzer->queue_handle, &message, waitTime);
        if (res != pdPASS) {
            ESP_LOGD(kTag, "No new message, Stop beeping");
            buzzer_stop(buzzer);
            waitTime = portMAX_DELAY;
        } else {
            ESP_LOGD(kTag, "Beep at %" PRIu32 " Hz for %" PRIu32 " ms", message.frequency, message.duration_ms);
            waitTime = pdMS_TO_TICKS(message.duration_ms);
            if (message.frequency == 0) {
                buzzer_stop(buzzer);
                vTaskDelay(waitTime);
            } else {
                buzzer_start(buzzer, message.frequency);
                vTaskDelay(waitTime);
            }
        }
    }
}

esp_err_t buzzer_init(Buzzer *buzzer, gpio_num_t gpio_num, ledc_clk_cfg_t clk_cfg, ledc_mode_t speed_mode, ledc_timer_bit_t timer_bit, ledc_timer_t timer_num, ledc_channel_t channel,
                      uint32_t idle_level) {
    buzzer->speed_mode = speed_mode;
    buzzer->timer_bit = timer_bit;
    buzzer->timer_num = timer_num;
    buzzer->channel = channel;
    buzzer->idle_level = idle_level;

    // Configure Timer
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = buzzer->speed_mode;
    ledc_timer.timer_num = buzzer->timer_num;
    ledc_timer.duty_resolution = buzzer->timer_bit;
    ledc_timer.freq_hz = 4000; // Can be any value, but put something valid
    ledc_timer.clk_cfg = clk_cfg;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configure Channel
    ledc_channel_config_t ledc_channel = {};
    ledc_channel.speed_mode = buzzer->speed_mode;
    ledc_channel.channel = buzzer->channel;
    ledc_channel.timer_sel = buzzer->timer_num;
    ledc_channel.gpio_num = gpio_num;
    ledc_channel.duty = 0;
    ledc_channel.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_ERROR_CHECK(ledc_stop(buzzer->speed_mode, buzzer->channel, buzzer->idle_level));

    buzzer->queue_handle = xQueueCreate(kQueueSize, sizeof(message_t));
    BaseType_t res = xTaskCreatePinnedToCore(buzzer_task, "buzzer_task", kStackSize, buzzer, uxTaskPriorityGet(NULL), &buzzer->task_handle, 1);

    if (res != pdPASS) {
        ESP_LOGE(kTag, "Failed to create task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void buzzer_deinit(Buzzer *buzzer) {
    vQueueDelete(buzzer->queue_handle);
    vTaskDelete(buzzer->task_handle);
}

BaseType_t buzzer_beep(Buzzer *buzzer, uint32_t frequency, uint32_t duration_ms) {
    message_t message = {
        .frequency = frequency,
        .duration_ms = duration_ms,
    };
    return xQueueSend(buzzer->queue_handle, &message, 0);
}

void init_buzzer() {
    if (buzzer_init(&buzzer_handler, CONFIG_BUZZER, LEDC_AUTO_CLK, LEDC_LOW_SPEED_MODE, LEDC_TIMER_13_BIT, LEDC_TIMER_0, LEDC_CHANNEL_0, 0) != ESP_OK) {
        ESP_LOGE(kTag, "error init buzzer");
    }
    return;
}

uint32_t get_note_frequency(char note, uint8_t sharp, uint8_t octave) {
    note = toupper((unsigned char)note);

    uint8_t base_note_index = 0;

    switch (note) {
    case 'C':
        base_note_index = 0;
        break;
    case 'D':
        base_note_index = 2;
        break;
    case 'E':
        base_note_index = 4;
        break;
    case 'F':
        base_note_index = 5;
        break;
    case 'G':
        base_note_index = 7;
        break;
    case 'A':
        base_note_index = 9;
        break;
    case 'B':
        base_note_index = 11;
        break;
    default:
        return 0;
    }

    base_note_index += sharp;

    const double middleC = 261.63;
    return (middleC * pow(2, (octave - 4 + base_note_index / 12.0)));
}

double get_note_duration(uint16_t bpm, uint8_t note_length, uint8_t has_dot) {

    uint32_t base_duration = (60000 / bpm / note_length * 4) * (has_dot ? 1.5 : 1);
    return base_duration;
}

bool is_valid_string(const char *rtttl, uint16_t len) {
    uint16_t count = 0;
    for (size_t i = 0; i < len; i++) {
        if (rtttl[i] == ':') {
            count++;
        }
    }
    return (count == 2);
}

void skip_name(char **ptr, const char *rtttl) {
    *ptr = strchr(rtttl, ':');
    return;
}

void find_default_val(char **ptr, uint8_t *default_duration, uint8_t *default_octave, uint16_t *bpm) {
    char *d_pos = strstr(*ptr, "d=");
    char *o_pos = strstr(*ptr, "o=");
    char *b_pos = strstr(*ptr, "b=");

    if (d_pos) {
        sscanf(d_pos, "d=%hhu", default_duration);
    }
    if (o_pos) {
        sscanf(o_pos, "o=%hhu", default_octave);
    }
    if (b_pos) {
        sscanf(b_pos, "b=%hu", bpm);
    }
    return;
}

bool parse_note(const char *token, Note *n) {
    int i = 0;
    // fill note
    if (isdigit((unsigned char)token[i])) {
        n->duration = atoi(&token[i]);
        i++;
        if (isdigit((unsigned char)token[i]))
            i++;
    }

    if (isalpha((unsigned char)token[i])) {
        n->note = token[i];
        i++;
    }
    if (token[i] == '#') {
        n->sharp = 1;
        i++;
    }
    if (isdigit((unsigned char)token[i])) {
        n->octave = token[i] - '0';
        i++;
    }
    if (token[i] == '.') {
        n->dotted = 1;
        i++;
    }

    // validate note

    if (n->note == '\0')
        return 0;
    if (n->duration > 32)
        return 0;
    if (n->octave > 32)
        return 0;
    return 1;
}

int parse_rtttl(const char *rtttl, uint16_t len) {
    if (is_buzzer == false) {
        return -1;
    }
    char *ptr = NULL; // ptr in string
    uint8_t default_duration = 4, default_octave = 6;
    uint16_t bpm = 120;

    // check valid string
    if (!is_valid_string(rtttl, len))
        return -1;

    skip_name(&ptr, rtttl);

    ptr++; // skip first ":" symbol

    find_default_val(&ptr, &default_duration, &default_octave, &bpm);

    ptr = strchr(ptr, ':'); // find second ":" symbol

    ptr++; // skip second ":" symbol

    strcpy(notes_buf, ptr);
    char *token = strtok(notes_buf, ",");
    // Note notes[MAX_NOTES];
    uint8_t count_notes = 0;
    memset(&notes, 0, sizeof(notes));

    // validate notes and collect
    while (token != NULL && count_notes < MAX_NOTES) {
        notes[count_notes].duration = default_duration;
        notes[count_notes].octave = default_octave;
        notes[count_notes].note = '\0';
        if (!parse_note(token, &notes[count_notes]))
            return -1;
        token = strtok(NULL, ",");
        count_notes++;
    }

    for (size_t i = 0; i < count_notes; i++) {
        uint32_t freq = get_note_frequency(notes[i].note, notes[i].sharp, notes[i].octave);
        uint32_t duration = get_note_duration(bpm, notes[i].duration, notes[i].dotted);
        buzzer_beep(&buzzer_handler, freq, duration);
    }
    return 0;
}

void disable_buzzer() { is_buzzer = false; }

void enable_buzzer() { is_buzzer = true; }
