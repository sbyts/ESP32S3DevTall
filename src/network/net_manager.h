#pragma once
#include <Arduino.h>

struct LampState {
    bool isLampOn;
    uint8_t colorMode;
    float brightness;
};

class NetManager {
public:
    void begin(LampState* statePtr);
    void loop();
    bool isConnected() const;

private:
    LampState* _lampState = nullptr;
    bool _apMode = false;
    String _connectedSsid = "None";
    String _adminUser = "admin";
    String _adminPass = "admin";
    String _hostname  = "ridy-lamp";

    void connectMultiAP();
    void startFallbackAP();
    void setupWebServer();
    void setupOTA();
};

extern NetManager netManager;