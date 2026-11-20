#ifndef LAMP_CONTROLLER_H
#define LAMP_CONTROLLER_H

#include <Arduino.h>
#include "../config/config.h"
#include "../network/net_manager.h"

class LampController {
public:
    void begin();
    void update(float dt);
    LampState* getState();

private:
    LampState _state = { false, 1, BRIGHTNESS_DEFAULT, 0 };

    // Capacitive touch button state
    uint32_t _touchBaseline = 0;
    bool _isTouched = false;
    bool _isDimming = false;
    unsigned long _touchStartTime = 0;
    int8_t _dimDirection = -1;

    // Smooth PWM output values (Soft-Fade)
    float _currentWhite = 0.0f;
    float _currentYellow = 0.0f;

    // Debounced NVS state persistence
    uint8_t _savedMode = 1;
    float _savedBrightness = BRIGHTNESS_DEFAULT;
    bool _stateDirty = false;
    unsigned long _lastStateChangeMs = 0;

    void initPWM();
    void loadStateFromNVS();
    void calibrateTouch();
    void handleTouch(float dt);
    void updateLightSmooth(float dt);
    void handleNvsPersistence();
};

extern LampController lampController;

#endif // LAMP_CONTROLLER_H