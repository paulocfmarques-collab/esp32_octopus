#ifndef TELEMETRY_ENGINE_H
#define TELEMETRY_ENGINE_H

#include "StorageManager.h"
#include "WifiPortalManager.h"
#include "LedManager.h"

class TelemetryEngine {
private:
    StorageManager& _storage;
    WifiPortalManager& _network;
    LedManager& _leds;
    
    void streamFileUDP(const char* filename);

public:
    TelemetryEngine(StorageManager& storage, WifiPortalManager& network, LedManager& leds);
    void executarComando(String cmd);
};

#endif
