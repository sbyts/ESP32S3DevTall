#include <Arduino.h>
#include "config/config.h"
#include "network/net_manager.h"

uint32_t touchBaseline = 0;
bool isTouched = false;
bool isDimming = false;
unsigned long touchStartTime = 0;

// Спільний стан світла (синхронізований із Web UI)
LampState lamp = { false, 1, BRIGHTNESS_DEFAULT };
int8_t dimDirection = -1;

float currentWhite = 0.0f;
float currentYellow = 0.0f;
unsigned long lastLoopTime = 0;

void calibrateTouch() {
    uint64_t sum = 0;
    for (int i = 0; i < TOUCH_CALIB_SAMPLES; i++) {
        sum += touchRead(PIN_TOUCH);
        delay(20);
    }
    touchBaseline = sum / TOUCH_CALIB_SAMPLES;
    Serial.printf("[TOUCH] Baseline calibrated: %u\n", touchBaseline);
}

void handleTouch(float dt) {
    uint32_t touchVal = 0;
    for (int i = 0; i < TOUCH_FILTER_SAMPLES; i++) {
        touchVal += touchRead(PIN_TOUCH);
    }
    touchVal /= TOUCH_FILTER_SAMPLES;

    int32_t diff = (int32_t)touchVal - (int32_t)touchBaseline;
    unsigned long now = millis();

    if (!isTouched && diff > (int32_t)TOUCH_DELTA_ON) {
        isTouched = true;
        isDimming = false;
        touchStartTime = now;
    }

    if (isTouched && (now - touchStartTime > TOUCH_LONG_PRESS_MS)) {
        if (!isDimming) {
            isDimming = true;
            if (!lamp.isLampOn) {
                lamp.isLampOn = true;
                lamp.brightness = BRIGHTNESS_MIN;
                dimDirection = 1;
            }
        }
        lamp.brightness += dimDirection * DIM_SPEED_PER_SEC * dt;
        if (lamp.brightness >= BRIGHTNESS_MAX) {
            lamp.brightness = BRIGHTNESS_MAX;
            dimDirection = -1;
        } else if (lamp.brightness <= BRIGHTNESS_MIN) {
            lamp.brightness = BRIGHTNESS_MIN;
            dimDirection = 1;
        }
    }

    if (isTouched && diff < (int32_t)TOUCH_DELTA_OFF) {
        unsigned long duration = now - touchStartTime;
        isTouched = false;

        if (isDimming) {
            dimDirection = -dimDirection;
        } else if (duration >= TOUCH_DEBOUNCE_MS) {
            if (!lamp.isLampOn) {
                lamp.isLampOn = true;
                lamp.colorMode = 1;
            } else {
                lamp.colorMode++;
                if (lamp.colorMode > 3) {
                    lamp.isLampOn = false;
                    lamp.colorMode = 1;
                }
            }
        }
    }
}

void updateLightSmooth(float dt) {
    float targetWhite = 0.0f;
    float targetYellow = 0.0f;

    if (lamp.isLampOn) {
        float gammaBright = lamp.brightness * lamp.brightness;
        switch (lamp.colorMode) {
            case 1:
                targetWhite = 0.0f;
                targetYellow = gammaBright * PWM_MAX;
                break;
            case 2:
                targetWhite = gammaBright * PWM_MAX;
                targetYellow = 0.0f;
                break;
            case 3:
                targetWhite  = gammaBright * (PWM_MAX * MIX_CHANNEL_LIMIT);
                targetYellow = gammaBright * (PWM_MAX * MIX_CHANNEL_LIMIT);
                break;
        }
    }

    float fadeSpeed = isDimming ? FADE_SPEED_DIMMING : FADE_SPEED_SWITCH;
    float step = min(1.0f, fadeSpeed * dt);

    currentWhite  += (targetWhite  - currentWhite)  * step;
    currentYellow += (targetYellow - currentYellow) * step;

    ledcWrite(CH_WHITE,  (currentWhite  < 1.5f) ? 0 : (uint16_t)roundf(currentWhite));
    ledcWrite(CH_YELLOW, (currentYellow < 1.5f) ? 0 : (uint16_t)roundf(currentYellow));
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    touchSetCycles(TOUCH_MEAS_CYCLES, TOUCH_SLEEP_CYCLES);

    ledcSetup(CH_WHITE, PWM_FREQ, PWM_RES);
    ledcSetup(CH_YELLOW, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_LED_WHITE, CH_WHITE);
    ledcAttachPin(PIN_LED_YELLOW, CH_YELLOW);
    ledcWrite(CH_WHITE, 0);
    ledcWrite(CH_YELLOW, 0);

    calibrateTouch();
    netManager.begin(&lamp);

    lastLoopTime = millis();
}

void loop() {
    netManager.loop();

    unsigned long now = millis();
    float dt = (now - lastLoopTime) / 1000.0f;
    lastLoopTime = now;

    handleTouch(dt);
    updateLightSmooth(dt);
}