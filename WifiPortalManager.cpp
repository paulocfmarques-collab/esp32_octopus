#include "WifiPortalManager.h"
#include "NTPService.h"
#include "defines.h"
#include "CrashDiagnostics.h"
#include "StorageManager.h"
#include "PrometheusMetrics.h"
#include <math.h>
extern StorageManager storage;
extern PrometheusMetrics prometheus;

// SSIDs may contain quotes, backslashes and control characters.
static String jsonString(const String& value) {
    String out = "\"";
    for (size_t i = 0; i < value.length(); ++i) {
        unsigned char c = value[i];
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) { char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped; }
        else out += char(c);
    }
    return out + "\"";
}

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
    .wrapper { width: 100%; max-width: 1100px; display: flex; flex-direction: column; gap: 20px; }
    .console { overflow-wrap: anywhere; white-space: pre-wrap; }
    .progress-bar.danger { background: var(--danger); }
    h2 { font-size: 16px; color: var(--accent); margin: 20px 0 12px; }
    @media(max-width:600px) { body { padding:10px; } header { flex-wrap:wrap; gap:12px; } .tab-content { padding:12px; } .form-row { grid-template-columns:1fr; } th,td { padding:8px 4px; overflow-wrap:anywhere; } }
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
      <div class="status-badge" id="connection" role="status">CONECTANDO...</div>
    </header>

    <div class="tabs">
      <button class="tab-btn active" onclick="switchTab('dash', this)">📊 Dashboard</button>
      <button class="tab-btn" onclick="switchTab('config', this)">⚙️ Configurações</button>
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

    <div id="diagnostics" class="card">
      <h2>Hardware e firmware</h2><div id="hardware" class="console">Aguardando dados...</div>
      <h2>Memória e armazenamento</h2><div id="memory" class="console">Aguardando dados...</div>
      <h2>Rede e serviços</h2><div id="services" class="console">Aguardando dados...</div>
      <h2>Reinicialização e diagnóstico de falhas</h2><div id="reset" class="console">Aguardando dados...</div>
      <p style="margin:12px 0">Envie <code>crash_info</code> por UDP para consultar o panic, tarefa e backtrace. O relatório é arquivado em /crash_*.txt no SD; o registro interno só é apagado após verificar a cópia. Use sd_list e sd_read:/arquivo para recuperar relatórios.</p>
      <div class="console">Consultas UDP: help · info · status · reason · crash_info · cpu · temp · ram · heap · flash · net_info · wifi_list · sd_status · uptime · time · date · age</div>
      <p style="margin-top:12px">Histórico UDP: /udp_log.txt (RX e TX com data, hora, cliente e resultado de envio). Rotação de 512 KB para /udp_log_old.txt. Consulte com sd_read:/udp_log.txt. Sem SD, não há gravação.</p>
      <p id="updated" style="margin-top:12px" aria-live="polite">Atualização automática a cada 2 segundos.</p>
    </div>

    <div id="config" class="tab-content">
      <h2>Prometheus</h2>
      <p style="margin-bottom:15px">O Prometheus coleta as métricas em uma porta exclusiva. O painel continua na porta 80.</p>
      <form id="prom-form" action="/prometheus/salvar" method="POST">
        <div class="checkbox-group">
          <input type="checkbox" id="prom-enabled" name="prom_enabled" value="1">
          <label for="prom-enabled">Ativar endpoint de métricas</label>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label for="prom-host">Endereço do servidor Prometheus autorizado (opcional)</label>
            <input type="text" id="prom-host" name="prom_host" maxlength="253" placeholder="Ex: 192.168.1.20 ou prometheus.local">
          </div>
          <div class="form-group">
            <label for="prom-port">Porta de métricas no Octopus</label>
            <input type="number" id="prom-port" name="prom_port" min="1" max="65535" value="9120" required>
          </div>
        </div>
        <p style="margin-bottom:12px">Informe apenas IP ou hostname do coletor, sem http:// nem porta. Em branco, permite a coleta por qualquer cliente da rede. A porta acima é do Octopus; não é a porta 9090 do servidor Prometheus. Portas 80 e 3232 são reservadas.</p>
        <button id="prom-save" type="submit" class="btn-submit">Salvar Prometheus sem reiniciar</button>
        <p id="prom-result" role="status" style="margin-top:12px"></p>
      </form>
      <h2>Estado da coleta</h2>
      <div id="prom-state" class="console">Aguardando configuração...</div>
      <h2>Exemplo para prometheus.yml</h2>
      <pre id="prom-yaml" class="console"></pre>
      <p style="margin:12px 0">Padrão ESP32C6: métricas de sensores, energia, relés, ADC e MQTT sem integração são omitidas. Uso de CPU aparece após amostras válidas do FreeRTOS. SD informa se as estatísticas estão disponíveis.</p>
      <h2>Wi-Fi e relógio</h2>
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
    function switchTab(tabId, button) {
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.getElementById(tabId).classList.add('active');
      button.classList.add('active');
    }

    let configLoaded = false;
    document.getElementById('prom-form').addEventListener('submit', async event => {
      event.preventDefault();
      const button = document.getElementById('prom-save');
      const result = document.getElementById('prom-result');
      const port = Number(document.getElementById('prom-port').value);
      if (!Number.isInteger(port) || port < 1 || port > 65535 || port === 80 || port === 3232) {
        result.textContent = 'Porta inválida: use 1 a 65535, exceto 80 e 3232.';
        return;
      }
      button.disabled = true;
      result.textContent = 'Salvando...';
      try {
        const response = await fetch('/prometheus/salvar', {method:'POST', body:new URLSearchParams(new FormData(event.currentTarget)), signal:AbortSignal.timeout(5000)});
        result.textContent = await response.text();
        if (!response.ok) result.style.color = 'var(--danger)';
        else result.style.color = 'var(--success)';
      } catch (err) { result.textContent = 'Não foi possível confirmar a gravação. Confira a conexão e tente novamente.'; }
      finally { button.disabled = false; }
    });
    async function updateMetrics() {
      try {
        const response = await fetch('/status', {cache:'no-store', signal:AbortSignal.timeout(5000)});
        if (!response.ok) throw new Error('HTTP ' + response.status);
        const data = await response.json();
        document.getElementById('connection').textContent = data.modo === 'AP' ? 'PORTAL AP' : data.conectado ? 'ONLINE' : 'WI-FI DESCONECTADO';
        document.getElementById('connection').style.color = data.conectado || data.modo === 'AP' ? 'var(--success)' : 'var(--danger)';
        document.getElementById('hardware').textContent = `Modelo: ${data.chip} · revisão ${data.revisao} · ${data.nucleos} núcleos\nCPU: ${data.cpu_mhz} MHz · temperatura: ${data.temperatura ?? 'indisponível'} °C\nFirmware: ${data.firmware} · build: ${data.build}\nESP-IDF: ${data.sdk}`;
        document.getElementById('memory').textContent = `Heap livre / total: ${data.heap_livre} / ${data.heap_total} bytes\nMenor heap livre: ${data.heap_min} bytes · maior bloco: ${data.heap_bloco} bytes\nPSRAM livre / total: ${data.psram_livre} / ${data.psram_total} bytes\nFlash física: ${data.flash_total} bytes · frequência: ${data.flash_mhz} MHz\nFirmware: ${data.sketch} bytes · espaço disponível para sketch: ${data.sketch_livre} bytes\nCartão SD: ${data.sd ? 'inicializado' : 'indisponível'}`;
        document.getElementById('services').textContent = `Modo: ${data.modo} · canal: ${data.canal}\nGateway: ${data.gateway} · máscara: ${data.mascara} · DNS: ${data.dns}\nHTTP: porta 80 · UDP: ${data.udp_port}\nOTA: ${data.ota ? 'inicializado (OCTOPUS_NODE)' : 'não inicializado'}\nRelógio: ${data.relogio_valido ? 'data/hora válidas' : 'aguardando sincronização'} · GMT ${data.fuso} · horário de verão: ${data.dst ? 'ativado' : 'desativado'}\nPisca padrão: ${data.blink} ms`;
        document.getElementById('reset').textContent = `Último reset: ${data.reset}\nCore dump em flash: ${data.coredump ? 'habilitado (ELF)' : 'não habilitado neste firmware'}\nTempo ativo: ${Math.floor(data.uptime/86400)}d ${Math.floor(data.uptime/3600)%24}h ${Math.floor(data.uptime/60)%60}m ${data.uptime%60}s`;
        document.getElementById('updated').textContent = 'Última leitura: ' + new Date().toLocaleTimeString('pt-BR');
        document.getElementById('prom-state').textContent = `${data.prom_status}\nEndpoint: http://${data.ip}:${data.prom_port}/metrics\nServidor autorizado: ${data.prom_host || 'qualquer cliente da rede'}\nSD: ${data.sd ? 'inicializado' : 'indisponível'} · total: ${data.sd_total_bytes} B · usado: ${data.sd_used_bytes} B\nLogs gravados: ${data.sd_log_writes} · erros: ${data.sd_write_errors}`;
        document.getElementById('prom-yaml').textContent = `scrape_configs:\n  - job_name: octopus\n    scrape_interval: 15s\n    metrics_path: /metrics\n    static_configs:\n      - targets: ['${data.ip}:${data.prom_port}']`;
        if (!configLoaded) {
          for (const [name, value] of Object.entries({udpport:data.udp_port, blink:data.blink, fuso:data.fuso})) document.querySelector(`[name="${name}"]`).value = value;
          document.getElementById('dst').checked = data.dst;
          document.getElementById('prom-enabled').checked = data.prom_enabled;
          document.getElementById('prom-host').value = data.prom_host;
          document.getElementById('prom-port').value = data.prom_port;
          configLoaded = true;
        }
        
        document.getElementById('val-data').innerText = data.data;
        document.getElementById('val-hora').innerText = data.hora;
        document.getElementById('val-ram').innerText = data.ram;
        document.getElementById('val-rssi').innerText = data.rssi;
        document.getElementById('val-ssid').innerText = data.ssid;
        document.getElementById('val-ip').innerText = data.ip;
        document.getElementById('val-mac').innerText = data.mac;
        document.getElementById('val-uptime').innerText = data.uptime;

        const ramPct = data.heap_total ? Math.min(100, (data.heap_livre / data.heap_total) * 100) : 0;
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
          
          for (const text of ['Slot ' + r.slot, r.ssid, badgeText]) {
            const td = document.createElement('td');
            td.textContent = text;
            tr.appendChild(td);
          }
          tr.lastChild.className = badgeClass;
          tbody.appendChild(tr);
        });

      } catch (err) {
        document.getElementById('connection').textContent = 'SEM COMUNICAÇÃO';
        document.getElementById('connection').style.color = 'var(--danger)';
        document.getElementById('updated').textContent = 'Falha na atualização. Valores exibidos são da última leitura.';
      } finally { setTimeout(updateMetrics, 2000); }
    }

    updateMetrics();
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
    _server.on("/prometheus/salvar", HTTP_POST, [this]() { prometheus.salvarConfiguracao(_server); });
    _server.onNotFound([this]() { PrometheusRequest r(prometheus); r.erro = true; _server.send(404, "text/plain", "Nao encontrado\n"); });
    _server.begin();
    prometheus.iniciarServidor();
}

// Inicializa o servidor HTTP interno no modo Station
void WifiPortalManager::iniciarWebServerLocal() {
    _modoAP = false;
    _server.on("/", HTTP_GET, std::bind(&WifiPortalManager::handleRoot, this));
    _server.on("/salvar", HTTP_POST, std::bind(&WifiPortalManager::handleSalvar, this));
    _server.on("/status", HTTP_GET, std::bind(&WifiPortalManager::handleStatus, this)); 
    _server.on("/prometheus/salvar", HTTP_POST, [this]() { prometheus.salvarConfiguracao(_server); });
    _server.onNotFound([this]() { PrometheusRequest r(prometheus); r.erro = true; _server.send(404, "text/plain", "Nao encontrado\n"); });
    _server.begin();
    prometheus.iniciarServidor();
}

void WifiPortalManager::iniciarUDP() {
    if (!_udpMutex) _udpMutex = xSemaphoreCreateMutex();
    _udp.begin(getUdpPort());
}

void WifiPortalManager::processarPortal() {
    _server.handleClient();
    prometheus.processar();
}

void WifiPortalManager::handleRoot() { 
    PrometheusRequest request(prometheus);
    _server.send(200, "text/html", _htmlPage); 
}

void WifiPortalManager::handleStatus() {
    PrometheusRequest request(prometheus);
    uint32_t freeHeap = ESP.getFreeHeap() / 1024;
    float flashLivre = (float)ESP.getFreeSketchSpace() / (1024.0 * 1024.0);

    _prefs.begin("wifi_circular", true);
    int head = _prefs.getInt("head", -1);

    String json;
    json.reserve(2400); 
    json = "{";
    json += "\"data\":\"" + ntp.obterApenasData() + "\",";
    json += "\"hora\":\"" + ntp.obterApenasHora() + "\",";
    json += "\"ram\":" + String(freeHeap) + ",";
    json += "\"flash\":\"" + String(flashLivre, 1) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ssid\":" + jsonString(_modoAP ? String("OCTOPUS_CONFIG_PORTAL") : WiFi.SSID()) + ",";
    json += "\"ip\":" + jsonString((_modoAP ? WiFi.softAPIP() : WiFi.localIP()).toString()) + ",";
    json += "\"mac\":\"" + WiFi.macAddress() + "\",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"modo\":" + jsonString(_modoAP ? "AP" : "STA") + ",";
    json += "\"conectado\":" + String(isConnected() ? "true" : "false") + ",";
    json += "\"chip\":" + jsonString(ESP.getChipModel()) + ",";
    json += "\"revisao\":" + String(ESP.getChipRevision()) + ",";
    json += "\"nucleos\":" + String(ESP.getChipCores()) + ",";
    json += "\"cpu_mhz\":" + String(ESP.getCpuFreqMHz()) + ",";
    float temperatura = temperatureRead();
    json += "\"temperatura\":" + (isfinite(temperatura) ? String(temperatura, 1) : String("null")) + ",";
    json += "\"firmware\":" + jsonString(Config::FIRMWARE_VERSION) + ",\"build\":" + jsonString(String(__DATE__) + " " + __TIME__) + ",";
    json += "\"sdk\":" + jsonString(ESP.getSdkVersion()) + ",";
    json += "\"heap_livre\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"heap_total\":" + String(ESP.getHeapSize()) + ",";
    json += "\"heap_min\":" + String(ESP.getMinFreeHeap()) + ",";
    json += "\"heap_bloco\":" + String(ESP.getMaxAllocHeap()) + ",";
    json += "\"psram_livre\":" + String(ESP.getFreePsram()) + ",";
    json += "\"psram_total\":" + String(ESP.getPsramSize()) + ",";
    json += "\"flash_total\":" + String(ESP.getFlashChipSize()) + ",";
    json += "\"flash_mhz\":" + String(ESP.getFlashChipSpeed() / 1000000) + ",";
    // getSketchSize verifica toda a imagem em flash; o tamanho e fixo ate o proximo boot.
    json += "\"sketch\":" + String(prometheus.tamanhoFirmware()) + ",";
    json += "\"sketch_livre\":" + String(ESP.getFreeSketchSpace()) + ",";
    json += "\"sd\":" + String(storage.isAtivo() ? "true" : "false") + ",";
    auto sd = storage.metricas();
    json += "\"sd_total_bytes\":" + String((unsigned long long)sd.total) + ",";
    json += "\"sd_used_bytes\":" + String((unsigned long long)sd.usado) + ",";
    json += "\"sd_log_writes\":" + String(sd.gravacoes) + ",";
    json += "\"sd_write_errors\":" + String(sd.erros) + ",";
    json += "\"prom_enabled\":" + String(prometheus.ativo() ? "true" : "false") + ",";
    json += "\"prom_port\":" + String(prometheus.porta()) + ",";
    json += "\"prom_host\":" + jsonString(prometheus.servidor()) + ",";
    json += "\"prom_status\":" + jsonString(prometheus.status()) + ",";
    json += "\"canal\":" + String(WiFi.channel()) + ",";
    json += "\"gateway\":" + jsonString(_modoAP ? String("--") : WiFi.gatewayIP().toString()) + ",";
    json += "\"mascara\":" + jsonString((_modoAP ? WiFi.softAPSubnetMask() : WiFi.subnetMask()).toString()) + ",";
    json += "\"dns\":" + jsonString(_modoAP ? String("--") : WiFi.dnsIP().toString()) + ",";
    struct tm hora;
    json += "\"relogio_valido\":" + String(getLocalTime(&hora, 0) ? "true" : "false") + ",";
    json += "\"reset\":" + jsonString(CrashDiagnostics::motivoReset()) + ",";
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
    json += "\"coredump\":true,";
#else
    json += "\"coredump\":false,";
#endif
    // Use a separate Preferences instance: the circular-network namespace is open above.
    Preferences config;
    config.begin("wifi", true);
    json += "\"udp_port\":" + String(config.getUInt("udpport", 4210)) + ",";
    json += "\"blink\":" + String(config.getULong("blink", 500)) + ",";
    json += "\"fuso\":" + String(config.getInt("fuso", -3)) + ",";
    json += "\"dst\":" + String(config.getBool("dst", false) ? "true" : "false") + ",";
    config.end();
    extern TaskHandle_t TaskOtaHandle;
    json += "\"ota\":" + String(TaskOtaHandle != NULL ? "true" : "false") + ",";
    
    json += "\"redes\":[";
    for (int i = 0; i < 5; i++) {
        String keySsid = "ssid" + String(i);
        String ssidSalvo = _prefs.getString(keySsid.c_str(), "");
        bool ocupado = (ssidSalvo != "" && ssidSalvo.length() > 0);
        bool isHead = (head == i && ocupado);

        json += "{";
        json += "\"slot\":" + String(i) + ",";
        json += "\"ssid\":" + jsonString(ocupado ? ssidSalvo : "---") + ",";
        json += "\"ocupado\":" + String(ocupado ? "true" : "false") + ",";
        json += "\"is_head\":" + String(isHead ? "true" : "false");
        json += "}";
        if (i < 4) json += ",";
    }
    _prefs.end();
    
    json += "]}";
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void WifiPortalManager::handleSalvar() {
    PrometheusRequest request(prometheus);
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
    // Mantém a porta de origem e protege o socket compartilhado entre os núcleos.
    if (!_udpMutex) return;
    xSemaphoreTake(_udpMutex, portMAX_DELAY);
    bool enviado = _udp.beginPacket(_respostaIP, _respostaPorta) == 1;
    if (enviado) {
        enviado = _udp.print(resposta) == resposta.length();
        enviado = _udp.endPacket() == 1 && enviado;
    }
    xSemaphoreGive(_udpMutex);
    if (!enviado) prometheus.registrarErroUDP();
    storage.gravarLogUDP("[TX][" + ntp.obterApenasData() + " " + ntp.obterApenasHora() +
        "][uptime_ms=" + String(millis()) + "][" + _respostaIP.toString() + ":" + String(_respostaPorta) +
        "][" + (enviado ? "ENVIADO" : "FALHA") + "][bytes=" + String(resposta.length()) + "]\n" + resposta + "\n[/TX]");
}

void WifiPortalManager::definirDestinoResposta(IPAddress ip, uint16_t porta) {
    _respostaIP = ip;
    _respostaPorta = porta;
}

bool WifiPortalManager::checarMensagensUDP(String& msgOut) {
    if (_modoAP || !_udpMutex) return false;
    xSemaphoreTake(_udpMutex, portMAX_DELAY);
    int packetSize = _udp.parsePacket();
    if (packetSize) {
        char buffer[256]; 
        int len = _udp.read((uint8_t*)buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0';
            msgOut = String(buffer);
            _ultimoRemetenteIP = _udp.remoteIP();
            _ultimoRemetentePorta = _udp.remotePort();
            xSemaphoreGive(_udpMutex);
            storage.gravarLogUDP("[RX][" + ntp.obterApenasData() + " " + ntp.obterApenasHora() +
                "][uptime_ms=" + String(millis()) + "][" + _ultimoRemetenteIP.toString() + ":" + String(_ultimoRemetentePorta) +
                "][bytes=" + String(len) + "]\n" + msgOut + "\n[/RX]");
            return true;
        }
    }
    xSemaphoreGive(_udpMutex);
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

IPAddress WifiPortalManager::getUdpRemoteIP() { return _ultimoRemetenteIP; }
uint16_t WifiPortalManager::getUdpRemotePort() { return _ultimoRemetentePorta; }
