#include "WifiPortalManager.h"
#include "NTPService.h"
#include "defines.h"

const char* WifiPortalManager::_htmlPage PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><title>OCTOPUS Config</title>
<style>body{font-family:Arial;background:#f4f4f9;text-align:center;}
.box{background:white;max-width:320px;margin:30px auto;padding:20px;border-radius:8px;box-shadow:0 4px 8px rgba(0,0,0,0.1);}</style>
</head><body><div class="box"><h2>Configurações do OCTOPUS</h2>
<form action="/salvar" method="POST">
  SSID:<br><input type="text" name="ssid" style="width:90%" required><br><br>
  Senha:<br><input type="password" name="senha" style="width:90%"><br><br>
  Porta UDP:<br><input type="number" name="udpport" value="4210" style="width:90%" required><br><br>
  Pisca Padrão (ms):<br><input type="number" name="blink" value="500" style="width:90%" required><br><br>
  Fuso Horário:<br><input type="number" name="fuso" value="-3" min="-12" max="14" style="width:90%" required><br><br>
  <label><input type="checkbox" name="dst" value="1"> Ativar Horário de Verão</label><br><br>
  <input type="submit" value="Salvar Configurações" style="padding:10px; background:#007bff; color:white; border:none; border-radius:4px; cursor:pointer; width:95%;">
</form><br><a href="/status" style="text-decoration:none; color:#007bff; font-size:14px; font-weight:bold;">→ Acessar Dashboard de Diagnósticos ←</a>
</div></body></html>)rawliteral";

WifiPortalManager::WifiPortalManager(uint8_t ledStatus) : _server(80), _modoAP(false), _ledStatus(ledStatus) {}

bool WifiPortalManager::conectar() {
    _prefs.begin("wifi", true);
    String ssid = _prefs.getString("ssid", "");
    String senha = _prefs.getString("senha", "");
    _prefs.end();

    if (ssid == "" || ssid.length() == 0) return false;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), senha.c_str());
    
    Serial.println(F("--------------------------------------------------"));
    Serial.printf("[WIFI] Tentando conectar ao SSID: %s\n", ssid.c_str());

    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
        delay(500);
        Serial.print(".");
        digitalWrite(_ledStatus, !digitalRead(_ledStatus));
        tentativas++;
    }
    digitalWrite(_ledStatus, LOW);
    Serial.println();

    _modoAP = (WiFi.status() != WL_CONNECTED);
    return !_modoAP;
}

void WifiPortalManager::iniciarPortal() {
    _modoAP = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("OCTOPUS_CONFIG_PORTAL");
    
    _server.on("/", HTTP_GET, std::bind(&WifiPortalManager::handleRoot, this));
    _server.on("/salvar", HTTP_POST, std::bind(&WifiPortalManager::handleSalvar, this));
    _server.on("/status", HTTP_GET, std::bind(&WifiPortalManager::handleStatus, this));
    _server.begin();
}

void WifiPortalManager::iniciarWebServerLocal() {
    _modoAP = false;
    _server.on("/", HTTP_GET, std::bind(&WifiPortalManager::handleRoot, this));
    _server.on("/salvar", HTTP_POST, std::bind(&WifiPortalManager::handleSalvar, this));
    _server.on("/status", HTTP_GET, std::bind(&WifiPortalManager::handleStatus, this));
    _server.begin();
}

void WifiPortalManager::iniciarUDP() {
    _udp.begin(getUdpPort());
}

void WifiPortalManager::processarPortal() {
    _server.handleClient();
}

void WifiPortalManager::handleRoot() { 
    _server.send(200, "text/html", _htmlPage); 
}

// ─── PAINEL VISUAL DE STATUS AVANÇADO (DASHBOARD INDUSTRIAL) ───
void WifiPortalManager::handleStatus() {
    uint32_t freeHeap = ESP.getFreeHeap() / 1024;
    uint32_t minFreeHeap = ESP.getMinFreeHeap() / 1024;
    float flashLivre = (float)ESP.getFreeSketchSpace() / (1024.0 * 1024.0);

    String htmlStatus = R"rawliteral(
    <!DOCTYPE html><html lang="pt-BR"><head><meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-repeat="5" content="5"> <!-- Auto-Refresh de 5 segundos -->
    <title>OCTOPUS - Painel de Controle</title>
    <style>
      body { font-family: 'Segoe UI', Arial, sans-serif; background: #121214; color: #e1e1e6; margin: 0; padding: 20px; text-align: center; }
      .container { max-width: 600px; margin: auto; }
      h2 { color: #04d361; margin-bottom: 25px; letter-spacing: 1px; text-transform: uppercase; font-size: 22px; }
      .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin-bottom: 25px; }
      .card { background: #202024; padding: 20px; border-radius: 8px; border: 1px solid #29292e; text-align: left; box-shadow: 0 4px 10px rgba(0,0,0,0.3); }
      .card-full { grid-column: span 2; }
      .label { font-size: 12px; text-transform: uppercase; color: #8d8d99; letter-spacing: 0.5px; }
      .value { font-size: 18px; font-weight: bold; color: #ffffff; margin-top: 5px; }
      .btn { display: inline-block; padding: 12px 24px; background: #007bff; color: white; text-decoration: none; border-radius: 4px; font-weight: bold; margin-top: 10px; transition: 0.2s; width: 85%; }
      .btn:hover { background: #0056b3; }
    </style></head><body><div class="container">
      <h2>🐙 OCTOPUS - Painel Sistema</h2>
      <div class="grid">
        <div class="card"><div class="label">📅 Data do Sistema</div><div class="value">)rawliteral" + ntp.obterApenasData() + R"rawliteral(</div></div>
        <div class="card"><div class="label">⏰ Hora Atual (NTP)</div><div class="value" style="color:#04d361;">)rawliteral" + ntp.obterApenasHora() + R"rawliteral(</div></div>
        <div class="card"><div class="label">🧠 RAM Livre Actual</div><div class="value">)rawliteral" + String(freeHeap) + R"rawliteral( KB</div></div>
        <div class="card"><div class="label">📉 Menor RAM Registrada</div><div class="value">)rawliteral" + String(minFreeHeap) + R"rawliteral( KB</div></div>
        <div class="card"><div class="label">💾 Espaço Flash Livre</div><div class="value">)rawliteral" + String(flashLivre, 1) + R"rawliteral( MB</div></div>
        <div class="card"><div class="label">📶 Sinal Wi-Fi (RSSI)</div><div class="value">)rawliteral" + String(WiFi.RSSI()) + R"rawliteral( dBm</div></div>
        <div class="card card-full"><div class="label">🌐 Informações de Rede Estável</div><div class="value" style="font-size:14px; font-family:monospace; color:#8d8d99; margin-top:8px;">
          SSID: )rawliteral" + WiFi.SSID() + R"rawliteral(<br>IP: )rawliteral" + WiFi.localIP().toString() + R"rawliteral(<br>MAC: )rawliteral" + WiFi.macAddress() + R"rawliteral(<br>Uptime: )rawliteral" + String(millis() / 1000) + R"rawliteral( s
        </div></div>
      </div>
      <a href="/" class="btn">Voltar para Configurações Wi-Fi</a>
    </div></body></html>)rawliteral";

    _server.send(200, "text/html", htmlStatus);
}

void WifiPortalManager::handleSalvar() {
    String novoSSID = _server.arg("ssid"); String novaSenha = _server.arg("senha");
    uint16_t udpPort = _server.arg("udpport").length() > 0 ? _server.arg("udpport").toInt() : 4210; 
    unsigned long blink = _server.arg("blink").length() > 0 ? _server.arg("blink").toInt() : 500;
    int fuso = _server.arg("fuso").length() > 0 ? _server.arg("fuso").toInt() : -3; 
    bool dst = _server.hasArg("dst");

    _prefs.begin("wifi", false);
    _prefs.putString("ssid", novoSSID); _prefs.putString("senha", novaSenha);
    _prefs.putUInt("udpport", udpPort); _prefs.putULong("blink", blink);
    _prefs.putInt("fuso", fuso); _prefs.putBool("dst", dst);
    _prefs.end(); 

    _prefs.begin("relogio", false);
    _prefs.putInt("gmt", fuso); _prefs.putBool("dst", dst);
    _prefs.end();

    _server.send(200, "text/html", "<h2>Configuracao salva com sucesso! O OCTOPUS esta reiniciando...</h2>");
    delay(2000);
    ESP.restart();
}

void WifiPortalManager::responderUDP(const String& resposta) {
    _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
    _udp.print(resposta);
    _udp.endPacket();
}

bool WifiPortalManager::checarMensagensUDP(String& msgOut) {
    if (_modoAP) return false;
    int packetSize = _udp.parsePacket();
    if (packetSize) {
        char buffer[256]; 
        int len = _udp.read(buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0';
            msgOut = String(buffer);
            return true;
        }
    }
    return false;
}

void WifiPortalManager::clearConfig() {
    _prefs.begin("wifi", false); _prefs.clear(); _prefs.end();
    _prefs.begin("relogio", false); _prefs.clear(); _prefs.end();
}

bool WifiPortalManager::isConnected() { return WiFi.status() == WL_CONNECTED; }
uint16_t WifiPortalManager::getUdpPort() { _prefs.begin("wifi", true); uint16_t p = _prefs.getUInt("udpport", 4210); _prefs.end(); return p; }
unsigned long WifiPortalManager::getDefaultBlinkInterval() { _prefs.begin("wifi", true); unsigned long b = _prefs.getULong("blink", 500); _prefs.end(); return b; }
int WifiPortalManager::getFusoHorario() { _prefs.begin("wifi", true); int f = _prefs.getInt("fuso", -3); _prefs.end(); return f; }
bool WifiPortalManager::getHorarioVerao() { _prefs.begin("wifi", true); bool d = _prefs.getBool("dst", false); _prefs.end(); return d; }
void WifiPortalManager::setNtpConfig(int fuso, bool dst) { _prefs.begin("wifi", false); _prefs.putInt("fuso", fuso); _prefs.putBool("dst", dst); _prefs.end(); }
