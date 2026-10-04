#ifndef WIFI_PORTAL_MANAGER_H
#define WIFI_PORTAL_MANAGER_H

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>

class WifiPortalManager {
private:
    WebServer _server;
    WiFiUDP _udp;
    Preferences _prefs;
    bool _modoAP;
    uint8_t _ledStatus;
    static const char* _htmlPage;

    void handleRoot();
    void handleSalvar();
    void handleStatus();

public:
    WifiPortalManager(uint8_t ledStatus);
    bool conectar();
    void iniciarPortal();
    void iniciarWebServerLocal();
    void iniciarUDP();
    void processarPortal();
    void responderUDP(const String& resposta);
    bool checarMensagensUDP(String& msgOut);
    void clearConfig();
    bool isConnected();
    
    uint16_t getUdpPort();
    unsigned long getDefaultBlinkInterval();
    int getFusoHorario();
    bool getHorarioVerao();
    void setNtpConfig(int fuso, bool dst);

    IPAddress getUdpRemoteIP() { return _udp.remoteIP(); }
    uint16_t getUdpRemotePort() { return _udp.remotePort(); }
};

#endif
