#include <Arduino.h>
#include "system/sys_logger.h"
#include "lamp/lamp_controller.h"
#include "network/net_manager.h"

unsigned long lastLoopTime = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    LOG_I("=== Ridy-10-Lite Core OS Boot ===");

    // Initialize PWM driver, restore NVS state, and calibrate touch sensor before Wi-Fi radio startup
    lampController.begin();

    // Start Multi-AP Wi-Fi roaming, embedded Web UI, and ArduinoOTA service
    netManager.begin(lampController.getState());

    lastLoopTime = millis();
}

void loop() {
    unsigned long now = millis();
    float dt = (now - lastLoopTime) / 1000.0f;
    lastLoopTime = now;

    netManager.loop();
    lampController.update(dt);
}