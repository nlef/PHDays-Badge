#include "led_plate.h"
#include "esp_log.h"
#include "led_plate.inc"

#define LED_STRIP_GPIO_PIN CONFIG_LEDS_STRIP
#define LED_STRIP_LED_COUNT 100
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

static const char *TAG = "led plate";

uint8_t currentImage = 0;
// uint8_t currentFrame = 0;

// Pixel frameToShow[100] = {0};
// Pixel frameDisplayed[100] = {0};

Pixel frameBuffer[LED_STRIP_LED_COUNT];
Pixel frameBufferDisplayed[LED_STRIP_LED_COUNT];

Image imageToShowBase;
Image imageToShow;
Image imageToTmp;

bool showCustom = false;
bool settedCustom = false;
bool ledsOn = true;

uint8_t currentBrightness = 10;
uint8_t lastBrightness = 0;
uint8_t beforeFadeBrightness = 0;
const uint8_t minBrightness = 1;
const uint8_t maxBrightness = 15;
const uint8_t turboBrightness = 15;
const uint8_t fadeBrightness = 4;
const uint8_t brightnessStep = 1;

unsigned long lastActivity = 0;
bool autoFadeLeds = true;
bool autoFaded = false;

static uint8_t step = 0;
const uint8_t steps = 20;

// Todo: maybe add global shifter state for galery cicle?
//  uint8_t ledShifterState = 0;

int lastActivitySecondsDelta = 600;
// Todo: move to images array!
// uint32_t oneImageShowMilles = 6000;

static SemaphoreHandle_t xSemaphore = NULL;

static led_strip_handle_t led_strip;

led_strip_handle_t configure_led(void) {

    led_strip_config_t strip_config = {.strip_gpio_num = LED_STRIP_GPIO_PIN,
                                       .max_leds = LED_STRIP_LED_COUNT,
                                       .led_model = LED_MODEL_WS2812,
                                       .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
                                       .flags = {
                                           .invert_out = false,
                                       }};

    led_strip_rmt_config_t rmt_config = {.clk_src = RMT_CLK_SRC_DEFAULT,
                                         .resolution_hz = LED_STRIP_RMT_RES_HZ,
                                         .mem_block_symbols = 64,
                                         .flags = {
                                             .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
                                         }};

    led_strip_handle_t led_strip_;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    ESP_ERROR_CHECK(led_strip_clear(led_strip_));
    return led_strip_;
}

void showFrame(const Pixel pixels[]) {
    if (xSemaphore != NULL && xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
        for (size_t leds = 0; leds < LED_STRIP_LED_COUNT; leds++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, LED_STRIP_LED_COUNT - 1 - leds, pixels[leds].r * currentBrightness / 100, pixels[leds].g * currentBrightness / 100,
                                                pixels[leds].b * currentBrightness / 100));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        xSemaphoreGive(xSemaphore);
    }
}

void showBlackFrame() {
    if (xSemaphore != NULL && xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
        for (size_t leds = 0; leds < LED_STRIP_LED_COUNT; leds++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, leds, 0, 0, 0));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        xSemaphoreGive(xSemaphore);
    }
}

void showFrameBr(const Pixel pixels[]) {
    for (uint8_t br = 1; br < 15; br++) {
        for (size_t leds = 0; leds < LED_STRIP_LED_COUNT; leds++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, LED_STRIP_LED_COUNT - 1 - leds, pixels[leds].r * br / 100, pixels[leds].g * br / 100, pixels[leds].b * br / 100));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(150 / portTICK_PERIOD_MS);
    }
    for (uint8_t br = 15; br > 10; br -= 1) {
        for (size_t leds = 0; leds < LED_STRIP_LED_COUNT; leds++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, LED_STRIP_LED_COUNT - 1 - leds, pixels[leds].r * br / 100, pixels[leds].g * br / 100, pixels[leds].b * br / 100));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(150 / portTICK_PERIOD_MS);
    }
}

void setAutoFade() {
    if (autoFadeLeds && lastActivity > 0 && ledsOn) {
        ESP_LOGI(TAG, "Fade last activity check");
        if (lastActivity + lastActivitySecondsDelta < (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000) {
            ESP_LOGW(TAG, "Fade last activity in past %lu for %lu", lastActivity, (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000);
            if (!autoFaded) {
                beforeFadeBrightness = currentBrightness;
                currentBrightness = fadeBrightness;
                autoFaded = true;
            }
        } else {
            ESP_LOGW(TAG, "Fade last activity not in past %lu for %lu", lastActivity, (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000);
            if (autoFaded) {
                currentBrightness = beforeFadeBrightness;
                autoFaded = false;
            }
        }
    }
}

void updateLastActivity() {
    // ESP_LOGI(TAG, "Update last activity %lu", lastActivity);
    lastActivity = (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000;
    // ESP_LOGI(TAG, "beforefade bight  `%u`  curr bigth `%u`", beforeFadeBrightness, currentBrightness);
}

void showTestColorOld(int r, int g, int b) {
    for (uint8_t br = 1; br < 25; br++) {
        for (size_t leds = 0; leds < LED_STRIP_LED_COUNT; leds++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, LED_STRIP_LED_COUNT - 1 - leds, r * br / 100, g * br / 100, b * br / 100));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    for (uint8_t br = 25; br > 5; br -= 1) {
        for (size_t leds = 0; leds < LED_STRIP_LED_COUNT; leds++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, LED_STRIP_LED_COUNT - 1 - leds, r * br / 100, g * br / 100, b * br / 100));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void showColor(uint8_t r, uint8_t g, uint8_t b) {
    ledsOn = false;
    if (xSemaphore != NULL && xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
        for (size_t leds = 0; leds < LED_STRIP_LED_COUNT; leds++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, LED_STRIP_LED_COUNT - 1 - leds, r * maxBrightness / 100, g * maxBrightness / 100, b * maxBrightness / 100));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        xSemaphoreGive(xSemaphore);
    }
}

void setframeBufferDisplayed(const Pixel pixels[]) {
    for (size_t i = 0; i < LED_STRIP_LED_COUNT; i++) {
        frameBufferDisplayed[i] = pixels[i];
    }
}

void copyBuffers() {
    for (size_t i = 0; i < LED_STRIP_LED_COUNT; i++) {
        frameBufferDisplayed[i] = frameBuffer[i];
    }
}

void shiftLedsDown() {
    for (size_t i = 0; i < LED_STRIP_LED_COUNT; i++) {
        if (i < 90) {
            frameBuffer[i + 10] = frameBufferDisplayed[i];
        } else {
            frameBuffer[i - 90] = frameBufferDisplayed[i];
        }
    }
}

void shiftLedsUp() {
    for (size_t i = 0; i < LED_STRIP_LED_COUNT; i++) {
        if (i < 10) {
            frameBuffer[i + 90] = frameBufferDisplayed[i];
        } else {
            frameBuffer[i - 10] = frameBufferDisplayed[i];
        }
    }
}

void shiftLedsRight() {
    for (size_t i = 0; i < LED_STRIP_LED_COUNT; i++) {
        if (i % 10 == 9) {
            frameBuffer[i - 9] = frameBufferDisplayed[i];
        } else {
            frameBuffer[i + 1] = frameBufferDisplayed[i];
        }
    }
}

void shiftLedsLeft() {
    for (size_t i = 0; i < LED_STRIP_LED_COUNT; i++) {
        if (i % 10 == 0) {
            frameBuffer[i + 9] = frameBufferDisplayed[i];
        } else {
            frameBuffer[i - 1] = frameBufferDisplayed[i];
        }
    }
}

void breathEffect() {
    if (step / 10 == 0)
        currentBrightness--;
    else if (step != 19)
        currentBrightness++;

    for (size_t i = 0; i < LED_STRIP_LED_COUNT; i++) {
        frameBuffer[i] = frameBufferDisplayed[i];
    }
}

Image *getImage() {
    if (showCustom)
        return &imageToShow;
    else
        return &imageToShowBase;
}

void shiftBaseLoopStep(void shift()) {
    if (step == 0) {
        setframeBufferDisplayed((*getImage()).frames[0].pixels);
    } else {
        shift();
        copyBuffers();
    }
    showFrame(frameBufferDisplayed);
}

void initPlate() {
    ESP_LOGI(TAG, "Init Plate");
    led_strip = configure_led();
    imageToShowBase = baseImages[currentImage];
    ESP_LOGI(TAG, "Finish Init Plate");
}

void tickNextBaseImage() {
    if (showCustom)
        return;

    ESP_LOGI(TAG, "Tick next base image");
    if (currentImage < baseImagesSize - 1) {
        currentImage++;
    } else {
        currentImage = 0;
    }

    imageToShowBase = baseImages[currentImage];
}

void processImage() {
    ESP_LOGI(TAG, "processImage step: %i", step);
    step = 0;
    if ((*getImage()).framesCount == 1) {
        while (step < steps) {
            if (!ledsOn) {
                step++;
                vTaskDelay((*getImage()).frames[0].duration / steps / portTICK_PERIOD_MS);
                continue;
            }

            switch ((*getImage()).shiftMode) {
            case 0:
                showFrame((*getImage()).frames[0].pixels);
                break;
            case 1:
                shiftBaseLoopStep(shiftLedsRight);
                break;
            case 2:
                shiftBaseLoopStep(shiftLedsLeft);
                break;
            case 3:
                shiftBaseLoopStep(shiftLedsUp);
                break;
            case 4:
                shiftBaseLoopStep(shiftLedsDown);
                break;
            case 5:
                shiftBaseLoopStep(breathEffect);
            case 6:
                // Todo: wave
                break;
            case 7:
                // Todo: rotation right
                break;
            case 8:
                // Todo: rotation left
                break;
            default:
                showFrame((*getImage()).frames[0].pixels);
                break;
            }

            step++;
            vTaskDelay((*getImage()).frames[0].duration / steps / portTICK_PERIOD_MS);
        }

    } else if ((*getImage()).framesCount > 1) {
        for (size_t i = 0; i < (*getImage()).framesCount; i++) {
            if (ledsOn) {
                showFrame((*getImage()).frames[i].pixels);
            }
            vTaskDelay((*getImage()).frames[i].duration / portTICK_PERIOD_MS);
        }
    }
}

void plateUpdateTask(void *pvParameters) {
    ESP_ERROR_CHECK(gpio_set_direction(26, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(26, 1));
    initPlate();
    vSemaphoreCreateBinary(xSemaphore);
    ESP_LOGI(TAG, "Show plate");
    while (1) {
        tickNextBaseImage();
        processImage();
    }
}

void updateImageToShowCustom() {
    showCustom = true;
    settedCustom = true;
}

Image *getImageToShowCustom() { return &imageToShow; }

SemaphoreHandle_t getShowFrameSemaphore() { return xSemaphore; }

void shiftBrightness() {
    updateLastActivity();
    if (ledsOn) {
        ESP_LOGI(TAG, "Shifting led Brightness");
        if (autoFaded) {
            currentBrightness = beforeFadeBrightness;
            autoFaded = false;
        }

        if (currentBrightness >= maxBrightness) {
            currentBrightness = minBrightness;
        } else {
            currentBrightness += brightnessStep;
        }
    }
}

void setTurboBrightness() {
    updateLastActivity();
    if (ledsOn) {
        ESP_LOGI(TAG, "Shifting led Brightness to Turbo");
        currentBrightness = turboBrightness;
    }
}

void switchLeds() {
    updateLastActivity();
    if (ledsOn) {
        ledsOn = false;
        showBlackFrame();
    } else {
        ledsOn = true;
    }
}

void reset_showImage(bool tmp) {
    if (tmp) {
        imageToShow = imageToTmp;
        settedCustom = true;
        return;
    }
    showCustom = false;
    settedCustom = false;
}

void switchCustom() {
    updateLastActivity();
    if (settedCustom) {
        showCustom = !showCustom;
    }
}

void showInt(uint64_t pass, uint8_t r, uint8_t g, uint8_t b) {
    updateLastActivity();
    ledsOn = false;

    ESP_LOGI(TAG, "pass  %llu", pass);
    int arr[10];
    int i = 0;
    int rr;

    while (pass != 0) {
        rr = pass % 10;
        arr[i] = rr;
        i++;
        pass = pass / 10;
    }

    for (size_t iii = 0; iii < 10; iii++) {
        ESP_LOGI(TAG, " %d", arr[iii]);
    }

    for (size_t itt = 0; itt < LED_STRIP_LED_COUNT; itt++) {
        frameBuffer[itt] = (Pixel){0, 0, 0};
    }

    for (int j = i; j > 0; j--) {
        for (int ii = 0; ii < arr[j - 1]; ii++) {
            frameBuffer[(i - j) * 10 + ii] = (Pixel){r, g, b};
        }
    }

    showFrame(frameBuffer);
    vTaskDelay(15000 / portTICK_PERIOD_MS);
    ledsOn = true;
    step = 0;
}

void set_ota_display_image(uint8_t state) {
    switch (state) {
    case 0:
        imageToShow = loadImage;
        break;
    case 1:
        imageToShow = successImage;
        break;
    case 2:
        imageToShow = errorImage;
        break;
    default:
        return;
    }
    showCustom = true;
    ledsOn = true;
    step = 0;
}

uint8_t getSettedCustom() {
    if (settedCustom)
        return 1;
    else
        return 0;
}

void restoreSettedCustom(uint8_t state) {
    if (state == 0)
        settedCustom = false;
    else
        settedCustom = true;
}

uint8_t getShowCustom() {
    if (showCustom)
        return 1;
    else
        return 0;
}

/* !!! Call after restoreImageToShow !!!*/
void restoreShowCustom(uint8_t state) {
    if (state == 0)
        showCustom = false;
    else
        showCustom = true;

    step = 0;
}

void switchAutoFade() { autoFadeLeds = !autoFadeLeds; }

void switchLedsShiter() {
    if (imageToShow.shiftMode >= 6)
        imageToShow.shiftMode = 0;
    else
        imageToShow.shiftMode++;

    step = 0;
    ESP_LOGI(TAG, "shiftMode: %i", imageToShow.shiftMode);
}

void set_power_display_image(uint8_t percent) {
    if (percent <= 30) {
        imageToShow = lowBattery;
    } else if (percent > 30 && percent <= 60) {
        imageToShow = mediumBattery;
    } else if (percent > 60 && percent <= 90) {
        imageToShow = greenBattery;
    } else {
        imageToShow = fullBattery;
    }
    showCustom = true;
    ledsOn = true;
    step = 0;
}

void save_img_custom() { imageToTmp = imageToShow; }
