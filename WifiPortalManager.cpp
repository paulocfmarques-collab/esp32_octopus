#include "WifiPortalManager.h"
#include "NTPService.h"
#include "defines.h"

// Vincula a instância global 'ntp' declarada no arquivo principal (main.cpp)
extern NTPService ntp;

// Interface web unificada (Dashboard + Configurações) armazenada em Flash (PROGMEM)
const char* WifiPortalManager::_htmlPage PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>🐙 OCTOPUS - Painel de Controle</title>
  <style>
    :root {
      --bg-main: #0b0c10;
      --bg-card: #1f2833;
      --accent: #45f3ff;
      --accent-hover: #1f9c9c;
      --text-main: #c5c6c7;
      --text-white: #ffffff;
      --success: #00ffa3;
      --danger: #ff4a5a;
      --border: #2c3540;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', system-ui, sans-serif; }
    body { background-color: var(--bg-main); color: var(--text-main); padding: 20px; display: flex; justify-content: center; }
    .wrapper { width: 100%; max-width: 800px; display: flex; flex-direction: column; gap: 20px; }
    header { background: var(--bg-card); padding: 20px; border-radius: 12px; border: 1px solid var(--border); display: flex; align-items: center; justify-content: space-between; box-shadow: 0 4px 15px rgba(0,0,0,0.5); }
    header h1 { font-size: 20px; color: var(--text-white); display: flex; align-items: center; gap: 10px; text-transform: uppercase; letter-spacing: 1px; }
    header h1 span { color: var(--accent); }
    .status-badge { background: rgba(0,255,163,0.1); color: var(--success); padding: 6px 12px; border-radius: 20px; font-size: 12px; font-weight: bold; border: 1px solid var(--success); display: flex; align-items: center; gap: 6px; }
    .tabs { display: flex; gap: 10px; border-bottom: 2px solid var(--border); padding-bottom: 2px; }
    .tab-btn { background: none; border: none; color: var(--text-main); padding: 10px 20px; font-size: 15px; font-weight: 600; cursor: pointer; transition: 0.3s; border-radius: 6px 6px 0 0; }
    .tab-btn.active { color: var(--accent); background: var(--bg-card); border: 1px solid var(--border); border-bottom: 2px solid var(--bg-card); }
    .tab-content { display: none; background: var(--bg-card); padding: 25px; border-radius: 0 0 12px 12px; border: 1px solid var(--border); box-shadow: 0 8px 20px rgba(0,0,0,0.4); }
    .tab-content.active { display: block; animation: fadeIn 0.4s ease-out; }
    @keyframes fadeIn { from { opacity: 0; transform: translateY(5px); } to { opacity: 1; transform: translateY(0); } }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 15px; }
    .card { background: rgba(11,12,16,0.5); padding: 15px 20px; border-radius: 8px; border: 1px solid var(--border); position: relative; overflow: hidden; }
    .card .label { font-size: 11px; text-transform: uppercase; color: #858b94; font-weight: 600; letter-spacing: 0.5px; }
    .card .value { font-size: 22px; font-weight: bold; color: var(--text-white); margin-top: 5px; }
    .card-full { grid-column: 1 / -1; }
    .progress-bar-container { background: var(--border); height: 6px; border-radius: 3px; margin-top: 10px; overflow: hidden; }
    .progress-bar { background: var(--accent); height: 100%; width: 0%; transition: width 0.5s ease-out; }
    .progress-bar.success { background: var(--success); }
    .form-group { display: flex; flex-direction: column; gap: 5px; margin-bottom: 15px; text-align: left; }
    .form-group label { font-size: 13px; font-weight: 600; color: var(--text-white); }
    .form-group input[type="text"], .form-group input[type="password"], .form-group input[type="number"] { background: var(--bg-main); border: 1px solid var(--border); padding: 10px 14px; border-radius: 6px; color: var(--text-white); font-size: 14px; transition: 0.3s; width: 100%; }
    .form-group input:focus { border-color: var(--accent); outline: none; box-shadow: 0 0 5px rgba(69,243,255,0.3); }
    .form-row { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
    .checkbox-group { display: flex; align-items: center; gap: 10px; margin: 15px 0; }
    .checkbox-group input { width: 18px; height: 18px; cursor: pointer; accent-color: var(--accent); }
    .btn-submit { background: var(--accent); color: var(--bg-main); font-weight: bold; font-size: 15px; padding: 12px; border: none; border-radius: 6px; cursor: pointer; width: 100%; transition: 0.3s; text-transform: uppercase; letter-spacing: 0.5px; }
    .btn-submit:hover { background: var(--accent-hover); color: var(--text-white); }
    .console { background: #050608; border-radius: 6px; padding: 15px; font-family: 'Consolas', monospace; font-size: 13px; color: #a1b0be; text-align: left; border: 1px solid var(--border); line-height: 1.6; }
    .console span { color: var(--accent); }
    
    table { width: 100%; border-collapse: collapse; margin-top: 10px; text-align: left; font-size: 14px; }
    th, td { padding: 10px 12px; border-bottom: 1px solid var(--border); }
    th { background: rgba(69,243,255,0.1); color: var(--accent); font-weight: 600; text-transform: uppercase; font-size: 11px; letter-spacing: 0.5px; }
    .badge { padding: 3px 8px; border-radius: 4px; font-size: 11px; font-weight: bold; }
    .badge.active { background: rgba(0,255,163,0.1); color: var(--success); border: 1px solid var(--success); }
    .badge.empty { background: rgba(255,74,90,0.1); color: var(--danger); border: 1px solid var(--danger); }
  </style>
</head>
<body>
  <div class="wrapper">
    <header>
      <h1>🐙 Octopus <span>Node</span></h1>
      <div class="status-badge"><div style="width:8px; height:8px; background:var(--success); border-radius:50%;"></div>ONLINE</div>
    </header>

    <div class="tabs">
      <button class="tab-btn active" onclick="switchTab('dash')">📊 Dashboard</button>
      <button class="tab-btn" onclick="switchTab('config')">⚙️ Configurações</button>
    </div>

    <!-- ABA 1: DASHBOARD DINÂMICO -->
    <div id="dash" class="tab-content active">
      <div class="grid">
        <div class="card">
          <div class="label">📅 Data do Sistema</div>
          <div class="value" id="val-data">--/--/----</div>
        </div>
        <div class="card">
          <div class="label">⏰ Hora Atual (NTP)</div>
          <div class="value" id="val-hora" style="color: var(--success);">--:--:--</div>
        </div>
        <div class="card">
          <div class="label">🧠 RAM Dinâmica Disponível</div>
          <div class="value"><span id="val-ram">0</span> KB</div>
          <div class="progress-bar-container"><div id="bar-ram" class="progress-bar success"></div></div>
        </div>
        <div class="card">
          <div class="label">📶 Intensidade Wi-Fi</div>
          <div class="value"><span id="val-rssi">0</span> dBm</div>
          <div class="progress-bar-container"><div id="bar-rssi" class="progress-bar"></div></div>
        </div>

        <div class="card card-full">
          <div class="label">🗂️ Fila Circular de Credenciais Wi-Fi (Máx 5)</div>
          <table>
            <thead>
              <tr>
                <th>Slot</th>
                <th>SSID da Rede</th>
                <th>Status do Slot</th>
              </tr>
            </thead>
            <tbody id="table-slots"></tbody>
          </table>
        </div>

        <div class="card card-full">
          <div class="label">🌐 Informações Físicas e Conexão Estável</div>
          <div class="console" style="margin-top: 10px;">
            <div><span>SSID Conectado:</span> <span id="val-ssid" style="color:var(--text-white);">--</span></div>
            <div><span>Endereço IP:</span> <span id="val-ip" style="color:var(--text-white);">--</span></div>
            <div><span>Endereço MAC:</span> <span id="val-mac" style="color:var(--text-white);">--</span></div>
            <div><span>Tempo de Atividade (Uptime):</span> <span id="val-uptime" style="color:var(--text-white);">0</span> s</div>
          </div>
        </div>
      </div>
    </div>

    <div id="config" class="tab-content">
      <form action="/salvar" method="POST">
        <div class="form-group">
          <label>SSID da Rede Wi-Fi</label>
          <input type="text" name="ssid" placeholder="Ex: MinhaRede_2G" required>
        </div>
        <div class="form-group">
          <label>Senha de Acesso</label>
          <input type="password" name="senha" placeholder="••••••••">
        </div>
        <div class="form-row">
          <div class="form-group">
            <label>Porta de Escuta UDP</label>
            <input type="number" name="udpport" value="4210" required>
          </div>
          <div class="form-group">
            <label>Pisca Padrão (ms)</label>
            <input type="number" name="blink" value="500" required>
          </div>
        </div>
        <div class="form-group">
          <label>Fuso Horário (GMT)</label>
          <input type="number" name="fuso" value="-3" min="-12" max="14" required>
        </div>
        <div class="checkbox-group">
          <input type="checkbox" id="dst" name="dst" value="1">
          <label style="cursor:pointer;" for="dst">Ativar Horário de Verão Automático</label>
        </div>
        <button type="submit" class="btn-submit">Gravar Parâmetros e Reiniciar</button>
      </form>
    </div>
  </div>

  <script>
    function switchTab(tabId) {
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.getElementById(tabId).classList.add('active');
      event.currentTarget.classList.add('active');
    }

    async function updateMetrics() {
      try {
        const response = await fetch('/status');
        if (!response.ok) return;
        const data = await response.json();
        
        document.getElementById('val-data').innerText = data.data;
        document.getElementById('val-hora').innerText = data.hora;
        document.getElementById('val-ram').innerText = data.ram;
        document.getElementById('val-rssi').innerText = data.rssi;
        document.getElementById('val-ssid').innerText = data.ssid;
        document.getElementById('val-ip').innerText = data.ip;
        document.getElementById('val-mac').innerText = data.mac;
        document.getElementById('val-uptime').innerText = data.uptime;

        const ramPct = Math.min(100, Math.max(0, (data.ram / 220) * 100));
        document.getElementById('bar-ram').style.width = ramPct + '%';

        let rssiPct = 100 + parseInt(data.rssi);
        rssiPct = Math.min(100, Math.max(0, rssiPct * 1.4));
        const barRssi = document.getElementById('bar-rssi');
        barRssi.style.width = rssiPct + '%';
        barRssi.className = 'progress-bar ' + (rssiPct > 70 ? 'success' : rssiPct > 40 ? '' : 'danger');

        const tbody = document.getElementById('table-slots');
        tbody.innerHTML = "";
        data.redes.forEach(r => {
          const tr = document.createElement('tr');
          const badgeClass = r.ocupado ? 'badge active' : 'badge empty';
          const badgeText = r.ocupado ? (r.is_head ? 'OCUPADO (HEAD)' : 'OCUPADO') : 'VAZIO';
          
          tr.innerHTML = `
            <td>Slot ${r.slot}</td>
            <td style="font-family:monospace; color:${r.ocupado ? '#fff' : '#555'};">${r.ssid}</td>
            <td><span class="${badgeClass}">${badgeText}</span></td>
          `;
          tbody.appendChild(tr);
        });

      } catch (err) { console.error("Erro na leitura de métricas assíncronas", err); }
    }

    updateMetrics();
    setInterval(updateMetrics, 2000);
  </script>
</body>
</html>
)rawliteral";

WifiPortalManager::WifiPortalManager(uint8_t ledStatus) : _server(80), _modoAP(false), _ledStatus(ledStatus) {}

// Varre ciclicamente as 5 posições da fila circular tentando conectar
bool WifiPortalManager::conectar() {
    _prefs.begin("wifi_circular", true);
    int head = _prefs.getInt("head", -1);
    int total = _prefs.getInt("total", 0);
    _prefs.end();

    if (total == 0) {
        Serial.println(F("[WIFI] Nenhuma rede armazenada na fila circular."));
        return false;
    }

    WiFi.mode(WIFI_STA);
    
    // Varre a fila a partir do slot mais recente (head) até o mais antigo
    for (int i = 0; i < total; i++) {
        int index = (head - i + 5) % 5;
        
        _prefs.begin("wifi_circular", true);
        String keySsid = "ssid" + String(index);
        String keyPass = "pass" + String(index);
        String ssid = _prefs.getString(keySsid.c_str(), "");
        String senha = _prefs.getString(keyPass.c_str(), "");
        _prefs.end();

        if (ssid == "" || ssid.length() == 0) continue;

        WiFi.begin(ssid.c_str(), senha.c_str());
        Serial.println(F("--------------------------------------------------"));
        Serial.printf("[WIFI] Slot %d - Tentando conectar ao SSID: %s\n", index, ssid.c_str());

        int tentativas = 0; 
        while (WiFi.status() != WL_CONNECTED && tentativas < 15) {
            delay(500);
            Serial.print(".");
            digitalWrite(_ledStatus, !digitalRead(_ledStatus));
            tentativas++;
        }

        digitalWrite(_ledStatus, LOW);
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            _modoAP = false;
            return true;
        }
        
        Serial.printf("[WIFI] Falha de conexao no Slot %d.\n", index);
    }

    _modoAP = true;
    return false;
}

// Inicializa o Access Point (AP) e injeta os callbacks de roteamento HTTP
void WifiPortalManager::iniciarPortal() {
    _modoAP = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("OCTOPUS_CONFIG_PORTAL");
    
    _server.on("/", HTTP_GET, std::bind(&WifiPortalManager::handleRoot, this));
    _server.on("/salvar", HTTP_POST, std::bind(&WifiPortalManager::handleSalvar, this));
    _server.on("/status", HTTP_GET, std::bind(&WifiPortalManager::handleStatus, this)); 
    _server.begin();
}

// Inicializa o servidor HTTP interno no modo Station
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

void WifiPortalManager::handleStatus() {
    uint32_t freeHeap = ESP.getFreeHeap() / 1024;
    float flashLivre = (float)ESP.getFreeSketchSpace() / (1024.0 * 1024.0);

    _prefs.begin("wifi_circular", true);
    int head = _prefs.getInt("head", -1);

    String json;
    json.reserve(768); 
    json = "{";
    json += "\"data\":\"" + ntp.obterApenasData() + "\",";
    json += "\"hora\":\"" + ntp.obterApenasHora() + "\",";
    json += "\"ram\":" + String(freeHeap) + ",";
    json += "\"flash\":\"" + String(flashLivre, 1) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ssid\":\"" + WiFi.SSID() + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"mac\":\"" + WiFi.macAddress() + "\",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    
    json += "\"redes\":[";
    for (int i = 0; i < 5; i++) {
        String keySsid = "ssid" + String(i);
        String ssidSalvo = _prefs.getString(keySsid.c_str(), "");
        bool ocupado = (ssidSalvo != "" && ssidSalvo.length() > 0);
        bool isHead = (head == i && ocupado);

        json += "{";
        json += "\"slot\":" + String(i) + ",";
        json += "\"ssid\":\"" + (ocupado ? ssidSalvo : "---") + "\",";
        json += "\"ocupado\":" + String(ocupado ? "true" : "false") + ",";
        json += "\"is_head\":" + String(isHead ? "true" : "false");
        json += "}";
        if (i < 4) json += ",";
    }
    _prefs.end();
    
    json += "]}";
    _server.send(200, "application/json", json);
}

void WifiPortalManager::handleSalvar() {
    String novoSSID = _server.arg("ssid"); 
    String novaSenha = _server.arg("senha");
    uint16_t udpPort = _server.arg("udpport").length() > 0 ? _server.arg("udpport").toInt() : 4210; 
    unsigned long blink = _server.arg("blink").length() > 0 ? _server.arg("blink").toInt() : 500;
    int fuso = _server.arg("fuso").length() > 0 ? _server.arg("fuso").toInt() : -3; 
    bool dst = _server.hasArg("dst");

    _prefs.begin("wifi", false);
    _prefs.putUInt("udpport", udpPort); 
    _prefs.putULong("blink", blink);
    _prefs.putInt("fuso", fuso); 
    _prefs.putBool("dst", dst);
    _prefs.end(); 

    _prefs.begin("relogio", false);
    _prefs.putInt("gmt", fuso); 
    _prefs.putBool("dst", dst);
    _prefs.end();

    adicionarRedeFila(novoSSID, novaSenha);

    _server.send(200, "text/html", "<h2>Configuracao salva com sucesso! O OCTOPUS esta reiniciando...</h2>");
    delay(2000);
    ESP.restart();
}

void WifiPortalManager::adicionarRedeFila(const String& ssid, const String& senha) {
    _prefs.begin("wifi_circular", false);
    int head = _prefs.getInt("head", -1);
    int total = _prefs.getInt("total", 0);

    head = (head + 1) % 5;
    if (total < 5) total++;

    String keySsid = "ssid" + String(head);
    String keyPass = "pass" + String(head);

    _prefs.putString(keySsid.c_str(), ssid);
    _prefs.putString(keyPass.c_str(), senha);
    _prefs.putInt("head", head);
    _prefs.putInt("total", total);
    _prefs.end();
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
        int len = _udp.read((uint8_t*)buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0';
            msgOut = String(buffer);
            return true;
        }
    }
    return false;
}

String WifiPortalManager::obterListaRedesTXT() {
    _prefs.begin("wifi_circular", true);
    int head = _prefs.getInt("head", -1);
    int total = _prefs.getInt("total", 0);

    String txt = "--- FILA CIRCULAR WI-FI ---\n";
    txt += "Total de redes: " + String(total) + " | Ponteiro Head: Slot " + String(head) + "\n";
    
    for (int i = 0; i < 5; i++) {
        String keySsid = "ssid" + String(i);
        String ssidSalvo = _prefs.getString(keySsid.c_str(), "");
        txt += "Slot [" + String(i) + "]: ";
        if (ssidSalvo != "" && ssidSalvo.length() > 0) {
            txt += ssidSalvo;
            if (head == i) txt += " <- (Atual/Head)";
        } else {
            txt += "[VAZIO]";
        }
        txt += "\n";
    }
    _prefs.end();
    txt += "---------------------------\n";
    return txt;
}

void WifiPortalManager::clearConfig() {
    _prefs.begin("wifi", false); _prefs.clear(); _prefs.end();
    _prefs.begin("relogio", false); _prefs.clear(); _prefs.end();
    _prefs.begin("wifi_circular", false); _prefs.clear(); _prefs.end();
}

bool WifiPortalManager::isConnected() { return WiFi.status() == WL_CONNECTED; }
uint16_t WifiPortalManager::getUdpPort() { _prefs.begin("wifi", true); uint16_t p = _prefs.getUInt("udpport", 4210); _prefs.end(); return p; }
unsigned long WifiPortalManager::getDefaultBlinkInterval() { _prefs.begin("wifi", true); unsigned long b = _prefs.getULong("blink", 500); _prefs.end(); return b; }
int WifiPortalManager::getFusoHorario() { _prefs.begin("wifi", true); int f = _prefs.getInt("fuso", -3); _prefs.end(); return f; }
bool WifiPortalManager::getHorarioVerao() { _prefs.begin("wifi", true); bool d = _prefs.getBool("dst", false); _prefs.end(); return d; }
void WifiPortalManager::setNtpConfig(int fuso, bool dst) { _prefs.begin("wifi", false); _prefs.putInt("fuso", fuso); _prefs.putBool("dst", dst); _prefs.end(); }

IPAddress WifiPortalManager::getUdpRemoteIP() { return _udp.remoteIP(); }
uint16_t WifiPortalManager::getUdpRemotePort() { return _udp.remotePort(); }
