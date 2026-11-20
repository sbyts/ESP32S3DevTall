#include "lamp_controller.h"
#include "../system/sys_logger.h"
#include <Preferences.h>

LampController lampController;
static Preferences lampPrefs;

void LampController::begin() {
    initPWM();
    loadStateFromNVS();
    calibrateTouch();
}

LampState* LampController::getState() {
    return &_state;
}

void LampController::initPWM() {
    ledcSetup(CH_WHITE, PWM_FREQ, PWM_RES);
    ledcSetup(CH_YELLOW, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_LED_WHITE, CH_WHITE);
    ledcAttachPin(PIN_LED_YELLOW, CH_YELLOW);
    ledcWrite(CH_WHITE, 0);
    ledcWrite(CH_YELLOW, 0);
}

void LampController::loadStateFromNVS() {
    lampPrefs.begin("ridy_state", false);
    _savedMode       = constrain(lampPrefs.getUChar("mode", 1), 1, 3);
    _savedBrightness = constrain(lampPrefs.getFloat("bri", BRIGHTNESS_DEFAULT), BRIGHTNESS_MIN, BRIGHTNESS_MAX);

    _state.isLampOn   = false; // safe start after power
    _state.colorMode  = _savedMode;
    _state.brightness = _savedBrightness;

    LOG_I("NVS restored -> Mode: %u | Bri: %.0f%%", _state.colorMode, _state.brightness * 100.0f);
}

void LampController::calibrateTouch() {
    touchSetCycles(TOUCH_MEAS_CYCLES, TOUCH_SLEEP_CYCLES);

    uint64_t sum = 0;
    for (int i = 0; i < TOUCH_CALIB_SAMPLES; i++) {
        sum += touchRead(PIN_TOUCH);
        delay(20);
    }
    _touchBaseline = sum / TOUCH_CALIB_SAMPLES;
    LOG_I("Touch baseline calibrated: %u", _touchBaseline);
}

void LampController::update(float dt) {
    handleTouch(dt);
    updateLightSmooth(dt);
    handleNvsPersistence();
}

void LampController::handleTouch(float dt) {
    uint32_t touchVal = 0;
    for (int i = 0; i < TOUCH_FILTER_SAMPLES; i++) {
        touchVal += touchRead(PIN_TOUCH);
    }
    touchVal /= TOUCH_FILTER_SAMPLES;

    int32_t diff = (int32_t)touchVal - (int32_t)_touchBaseline;
    _state.touchDelta = diff;
    unsigned long now = millis();

    // 1. Початок дотику
    if (!_isTouched && diff > (int32_t)TOUCH_DELTA_ON) {
        _isTouched = true;
        _isDimming = false;
        _touchStartTime = now;
    }

    // 2. Утримання пальця (плавне димування)
    if (_isTouched && (now - _touchStartTime > TOUCH_LONG_PRESS_MS)) {
        if (!_isDimming) {
            _isDimming = true;
            if (!_state.isLampOn) {
                _state.isLampOn = true;
                _state.brightness = BRIGHTNESS_MIN;
                _dimDirection = 1;
            }
            LOG_I("Touch hold -> dimming started (Δ +%d)", diff);
        }

        _state.brightness += _dimDirection * DIM_SPEED_PER_SEC * dt;
        if (_state.brightness >= BRIGHTNESS_MAX) {
            _state.brightness = BRIGHTNESS_MAX;
            _dimDirection = -1;
        } else if (_state.brightness <= BRIGHTNESS_MIN) {
            _state.brightness = BRIGHTNESS_MIN;
            _dimDirection = 1;
        }
    }

    // 3. Відпускання пальця
    if (_isTouched && diff < (int32_t)TOUCH_DELTA_OFF) {
        unsigned long duration = now - _touchStartTime;
        _isTouched = false;

        if (_isDimming) {
            _dimDirection = -_dimDirection;
            LOG_I("Dimming set to %.0f%%", _state.brightness * 100.0f);
        } else if (duration >= TOUCH_DEBOUNCE_MS) {
            if (!_state.isLampOn) {
                _state.isLampOn = true;
            } else {
                _state.colorMode++;
                if (_state.colorMode > 3) {
                    _state.isLampOn = false;
                    _state.colorMode = 1;
                }
            }
            LOG_I("Touch tap -> %s | Mode %u | %.0f%%",
                  _state.isLampOn ? "ON" : "OFF", _state.colorMode, _state.brightness * 100.0f);
        }
    }
}

void LampController::updateLightSmooth(float dt) {
    float targetWhite = 0.0f;
    float targetYellow = 0.0f;

    if (_state.isLampOn) {
        float gammaBright = _state.brightness * _state.brightness;
        switch (_state.colorMode) {
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

    float fadeSpeed = _isDimming ? FADE_SPEED_DIMMING : FADE_SPEED_SWITCH;
    float step = min(1.0f, fadeSpeed * dt);

    _currentWhite  += (targetWhite  - _currentWhite)  * step;
    _currentYellow += (targetYellow - _currentYellow) * step;

    ledcWrite(CH_WHITE,  (_currentWhite  < 1.5f) ? 0 : (uint16_t)roundf(_currentWhite));
    ledcWrite(CH_YELLOW, (_currentYellow < 1.5f) ? 0 : (uint16_t)roundf(_currentYellow));
}

void LampController::handleNvsPersistence() {
    if (_state.colorMode != _savedMode || fabsf(_state.brightness - _savedBrightness) > 0.005f) {
        _savedMode         = _state.colorMode;
        _savedBrightness   = _state.brightness;
        _stateDirty        = true;
        _lastStateChangeMs = millis();
    }

    if (_stateDirty && !_isTouched && (millis() - _lastStateChangeMs >= NVS_SAVE_DELAY_MS)) {
        lampPrefs.putUChar("mode", _savedMode);
        lampPrefs.putFloat("bri", _savedBrightness);
        _stateDirty = false;
        LOG_I("NVS saved -> Mode: %u | Bri: %.0f%%", _savedMode, _savedBrightness * 100.0f);
    }
}