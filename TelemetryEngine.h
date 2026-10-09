#ifndef TELEMETRY_ENGINE_H
#define TELEMETRY_ENGINE_H

#include <Arduino.h>
#include "StorageManager.h"
#include "WifiPortalManager.h"
#include "LedManager.h"

class TelemetryEngine {
public:
    TelemetryEngine(StorageManager& storage, WifiPortalManager& network, LedManager& leds);
    void executarComando(String cmd);
    void streamFileUDP(const char* filename);

    // Variáveis estáticas primitivas para respostas de chamadas assíncronas
    static char ultimoClienteIP[16];
    static uint16_t ultimoClientePorta;

private:
    StorageManager& _storage;
    WifiPortalManager& _network;
    LedManager& _leds;
};

#endif
