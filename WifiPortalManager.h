#ifndef WIFI_PORTAL_MANAGER_H
#define WIFI_PORTAL_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUDP.h>
#include <Preferences.h>

class WifiPortalManager {
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
    
    // Pontes para o Octopus.ino obter os dados do remetente do pacote
    IPAddress getUdpRemoteIP();
    uint16_t getUdpRemotePort();
    
    // Exportador de listagem de redes para o interpretador de comandos UDP
    String obterListaRedesTXT();

private:
    WebServer _server;
    WiFiUDP _udp;
    Preferences _prefs;
    bool _modoAP;
    uint8_t _ledStatus;

    static const char* _htmlPage PROGMEM;

    void handleRoot();
    void handleSalvar();
    void handleStatus();
    
    // Método auxiliar interno para a fila circular de tamanho 5
    void adicionarRedeFila(const String& ssid, const String& senha);
};

#endif
