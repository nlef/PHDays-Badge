#include "led_plate.h"
#include "esp_log.h"
#include "led_plate.inc"

static const char* TAG = "led plate";

uint8_t currentImage = 0;
uint8_t imageToShow[300] = { 0 };
uint8_t imageDisplayed[300] = { 0 };

bool showCustom = false;
bool settedCustom = false;
bool ledsOn = true;
float currentBrightness = 0.10f;
float lastBrightness = 0.0;
float beforeFadeBrightness = 0.0;
const float minBrightness = 0.01f;
const float maxBrightness = 0.15f;
const float turboBrightness = 0.15f;
const float fadeBrightness = 0.04f;
const float brightnessStep = 0.01f;

unsigned long lastActivity = 0;
bool autoFadeLeds = true;
bool autoFaded = false;

uint8_t ledShifterState = 0;

int lastActivitySecondsDelta = 600;
uint32_t oneImageShowMilles = 6000;

SemaphoreHandle_t xSemaphore = NULL;

// Todo: some gamma corrections needed
uint32_t createRGBbr(int r, int g, int b, float bright) { return ((((int)(g * bright) & 0xff) << 16) | (((int)(r * bright) & 0xff) << 8) | ((int)(b * bright) & 0xff)) & 0xffffff; }

uint32_t createRGB(int r, int g, int b) { return createRGBbr(r, g, b, 0.1f); }

void showImage(uint8_t pixels[])
{
    if (xSemaphore != NULL && xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
        struct led_state new_state;
        // fix some pixels colors on low brightness
        for (int leds = 0; leds < NUM_LEDS; leds++) {
            new_state.leds[leds] = 0;
        }
        ws2812_write_leds(new_state);
        ws2812_write_leds(new_state);

        for (int leds = 0; leds < NUM_LEDS; leds++) {
            new_state.leds[NUM_LEDS - 1 - leds] = createRGBbr(pixels[leds * 3], pixels[leds * 3 + 1], pixels[leds * 3 + 2], currentBrightness);
        }
        ws2812_write_leds(new_state);
        xSemaphoreGive(xSemaphore);
    }
}

void showImageBr(uint8_t pixels[])
{
    struct led_state new_state;
    for (float br = 0.01f; br < 0.15f; br += 0.01f) {
        for (int leds = 0; leds < NUM_LEDS; leds++) {
            new_state.leds[NUM_LEDS - 1 - leds] = createRGBbr(pixels[leds * 3], pixels[leds * 3 + 1], pixels[leds * 3 + 2], br);
        }
        ws2812_write_leds(new_state);
        vTaskDelay(150 / portTICK_PERIOD_MS);
    }
    for (float br = 0.15f; br > 0.10f; br -= 0.01f) {
        for (int leds = 0; leds < NUM_LEDS; leds++) {
            new_state.leds[NUM_LEDS - 1 - leds] = createRGBbr(pixels[leds * 3], pixels[leds * 3 + 1], pixels[leds * 3 + 2], br);
        }
        ws2812_write_leds(new_state);
        vTaskDelay(150 / portTICK_PERIOD_MS);
    }
}

void setAutoFade()
{
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

void updateLastActivity()
{
    ESP_LOGI(TAG, "Update last activity %lu", lastActivity);
    lastActivity = (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000;

    ESP_LOGI(TAG, "beforefade bight  `%lf`  curr bigth `%lf`", beforeFadeBrightness, currentBrightness);
}

void tickNextBaseImage()
{
    if (showCustom)
        return;

    if (currentImage < imagesSize - 1) {
        currentImage++;
    } else {
        currentImage = 0;
    }

    for (int i = 0; i < 300; i++) {
        imageDisplayed[i] = images[currentImage][i];
    }
}

void showTestColorOld(int r, int g, int b)
{
    struct led_state new_state;
    for (float br = 0.01f; br < 0.25f; br += 0.01f) {
        for (int leds = 0; leds < NUM_LEDS; leds++) {
            new_state.leds[NUM_LEDS - 1 - leds] = createRGBbr(r, g, b, br);
        }
        ws2812_write_leds(new_state);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    for (float br = 0.25f; br > 0.05f; br -= 0.01f) {
        for (int leds = 0; leds < NUM_LEDS; leds++) {
            new_state.leds[NUM_LEDS - 1 - leds] = createRGBbr(r, g, b, br);
        }
        ws2812_write_leds(new_state);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void showTestColor(int r, int g, int b)
{
    showCustom = true;
    struct led_state new_state;
    for (int leds = 0; leds < NUM_LEDS; leds++) {
        new_state.leds[NUM_LEDS - 1 - leds] = createRGBbr(r, g, b, 0.15f);
        imageDisplayed[leds * 3] = r;
        imageDisplayed[leds * 3 + 1] = g;
        imageDisplayed[leds * 3 + 2] = b;
    }
    ws2812_write_leds(new_state);
}

void initPlate()
{
    ws2812_control_init();

    if (showCustom) {
        for (int i = 0; i < 300; i++) {
            imageDisplayed[i] = imageToShow[i];
        }
    } else {
        for (int i = 0; i < 300; i++) {
            imageDisplayed[i] = images[currentImage][i];
        }
    }
}

void showPlate(bool force)
{
    setAutoFade();
    if (ledsOn || force)
        showImage(imageDisplayed);
}

void shifLedsDown()
{
    uint8_t imgBuffer[300] = { 0 };

    for (int i = 0; i < 300; i++) {
        if (i < 270) {
            imgBuffer[i + 30] = imageDisplayed[i];
        } else {
            imgBuffer[i - 270] = imageDisplayed[i];
        }
    }

    for (int i = 0; i < 300; i++) {
        imageDisplayed[i] = imgBuffer[i];
    }
}

void shiftLedsRight()
{
    uint8_t imgBuffer[300] = { 0 };

    for (int i = 0; i < 300; i++) {
        if (i % 30 == 27 || i % 30 == 28 || i % 30 == 29) {
            imgBuffer[i - 27] = imageDisplayed[i];
        } else {
            imgBuffer[i + 3] = imageDisplayed[i];
        }
    }

    for (int i = 0; i < 300; i++) {
        imageDisplayed[i] = imgBuffer[i];
    }
}

void shiftLeds()
{
    if (!ledsOn)
        return;

    if (ledShifterState == 1)
        shiftLedsRight();
    else if (ledShifterState == 2)
        shifLedsDown();
}

void plateUpdateTask(void* pvParameters)
{
    ESP_ERROR_CHECK(gpio_set_direction(26, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(26, 1));
    initPlate();
    vSemaphoreCreateBinary(xSemaphore);
    while (1) {
        ESP_LOGI(TAG, "Show plate");
        ESP_LOGI(TAG, "Current brightness -s %f", currentBrightness);

        if (ledShifterState == 0) {
            showPlate(false);
            vTaskDelay(oneImageShowMilles / portTICK_PERIOD_MS);
        } else {
            for (int i = 0; i < 20; i++) {
                showPlate(false);
                vTaskDelay((oneImageShowMilles / 20) / portTICK_PERIOD_MS);
                shiftLeds();
            }
        }

        ESP_LOGI(TAG, "Tick next base image");
        tickNextBaseImage();
    }
}

void updateImageToShow(uint8_t pixels[])
{
    for (size_t i = 0; i < 300; i++) {
        imageToShow[i] = pixels[i];
    }
    showCustom = true;
    settedCustom = true;

    for (int i = 0; i < 300; i++) {
        imageDisplayed[i] = imageToShow[i];
    }

    showPlate(false);
}

void shiftBrightness()
{
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
        showPlate(false);
    }
}

void setTurboBrightness()
{
    updateLastActivity();
    if (ledsOn) {
        ESP_LOGI(TAG, "Shifting led Brightness to Turbo");
        currentBrightness = turboBrightness;
        showPlate(false);
    }
}

void switchLeds()
{
    updateLastActivity();
    if (ledsOn) {
        ledsOn = false;
        lastBrightness = currentBrightness;
        currentBrightness = 0.0f;
    } else {
        currentBrightness = lastBrightness;
        lastBrightness = 0.0f;
        ledsOn = true;
    }
    showPlate(true);
}

void switchCustom()
{
    updateLastActivity();
    if (settedCustom) {
        showCustom = !showCustom;
    }
    if (showCustom) {
        for (int i = 0; i < 300; i++) {
            imageDisplayed[i] = imageToShow[i];
        }
    }
}

void showInt(uint32_t pass, int r, int g, int b)
{
    updateLastActivity();
    ESP_LOGI(TAG, "pass  %lu", pass);
    int arr[10];
    int i = 0;
    int j, rr;

    while (pass != 0) {
        rr = pass % 10;
        arr[i] = rr;
        i++;
        pass = pass / 10;
    }

    for (size_t iii = 0; iii < 10; iii++) {
        ESP_LOGI(TAG, " %d", arr[iii]);
    }

    uint8_t imagePixels[300] = { 0 };
    for (j = i; j > 0; j--) {
        for (int ii = 0; ii < arr[j - 1]; ii++) {
            imagePixels[(i - j) * 30 + ii * 3] = r;
            imagePixels[(i - j) * 30 + ii * 3 + 1] = g;
            imagePixels[(i - j) * 30 + ii * 3 + 2] = b;
        }
    }
    ledsOn = false;
    showImage(imagePixels);
    vTaskDelay(15000 / portTICK_PERIOD_MS);
    ledsOn = true;
}

uint8_t getSettedCustom()
{
    if (settedCustom)
        return 1;
    else
        return 0;
}

void restoreSettedCustom(uint8_t state)
{
    if (state == 0)
        settedCustom = false;
    else
        settedCustom = true;
}

uint8_t getShowCustom()
{
    if (showCustom)
        return 1;
    else
        return 0;
}

/* !!! Call after restoreImageToShow !!!*/
void restoreShowCustom(uint8_t state)
{
    if (state == 0)
        showCustom = false;
    else
        showCustom = true;

    if (showCustom) {
        for (int i = 0; i < 300; i++) {
            imageDisplayed[i] = imageToShow[i];
        }
    }
}

uint8_t* getImageToShow() { return imageToShow; }

void restoreImageToShow(uint8_t pixels[])
{
    for (size_t i = 0; i < 300; i++) {
        imageToShow[i] = pixels[i];
    }
}

void switchAutoFade() { autoFadeLeds = !autoFadeLeds; }

void switchLedsShiter()
{
    if (ledShifterState >= 2)
        ledShifterState = 0;
    else
        ledShifterState++;

    if (showCustom) {
        for (int i = 0; i < 300; i++) {
            imageDisplayed[i] = imageToShow[i];
        }
    } else {
        for (int i = 0; i < 300; i++) {
            imageDisplayed[i] = images[currentImage][i];
        }
    }
}
