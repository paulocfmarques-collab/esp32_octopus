#ifndef WIFI_PORTAL_MANAGER_H
#define WIFI_PORTAL_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUDP.h>
#include <Preferences.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class WifiPortalManager {
public:
    WifiPortalManager(uint8_t ledStatus);
    bool conectar();
    void iniciarPortal();
    void iniciarWebServerLocal();
    void iniciarUDP();
    void processarPortal();
    void responderUDP(const String& resposta);
    void definirDestinoResposta(IPAddress ip, uint16_t porta);
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
    SemaphoreHandle_t _udpMutex = nullptr;
    IPAddress _ultimoRemetenteIP;
    uint16_t _ultimoRemetentePorta = 0;
    IPAddress _respostaIP;
    uint16_t _respostaPorta = 0;
    WebServer _server;
    WiFiUDP _udp;
    Preferences _prefs;
    bool _modoAP;
    uint8_t _ledStatus;

    static const char* _htmlHead PROGMEM;
    static const char* _htmlStatus PROGMEM;
    static const char* _htmlConfig PROGMEM;
    static const char* _htmlFooter PROGMEM;

    void handleRoot();
    void handleConfig();
    void enviarPagina(bool configuracao);
    void handleSalvar();
    void handleStatus();
    void handleDownload();
    void handleUpload();
    void handleUploadFim();
    File _uploadFile;
    String _uploadDestino;
    String _uploadErro;
    size_t _uploadBytes = 0;
    bool _uploadPronto = false;
    uint32_t _uploadPartes = 0;
    
    // Método auxiliar interno para a fila circular de tamanho 5
    void adicionarRedeFila(const String& ssid, const String& senha);
};

#endif
