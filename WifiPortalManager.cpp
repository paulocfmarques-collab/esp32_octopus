#include "WifiPortalManager.h"
#include "NTPService.h"
#include "defines.h"
#include "CrashDiagnostics.h"
#include "StorageManager.h"
#include "PrometheusMetrics.h"
#include "RadioWatchdog.h"
#include "NetScan.h"
#include <lwip/sockets.h>
#include <errno.h>
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
const char* WifiPortalManager::_htmlHead PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>🐙 OCTOPUS - Painel de Controle</title>
  <style>
    :root{color-scheme:dark;--bg-main:#0b111b;--bg-card:#131e2c;--accent:#65e7bc;--accent-hover:#a2f4d6;--text-main:#aebed0;--text-white:#edf4fb;--success:#65e7bc;--danger:#ff8c9a;--border:#29384a}
    *{box-sizing:border-box;margin:0;padding:0}body{font-family:Segoe UI,system-ui,sans-serif;color:var(--text-main);background:radial-gradient(ellipse at top right,#183b3c55,transparent 48%),var(--bg-main);padding:32px 24px;line-height:1.6;min-height:100vh}
    .wrapper{max-width:1160px;margin:auto;display:flex;flex-direction:column;gap:22px}header{display:flex;align-items:center;justify-content:space-between;gap:16px;padding:8px 0 20px;border-bottom:1px solid var(--border)}header h1{color:var(--text-white);font-size:23px;letter-spacing:2px;font-weight:750}header h1 span{color:var(--accent);font-size:12px;letter-spacing:1px;margin-left:8px}.brand-caption{font-size:12px;margin-top:3px;color:#8499af}
    .status-badge{display:flex;align-items:center;gap:8px;color:var(--success);background:#65e7bc0c;border:1px solid #65e7bc33;padding:7px 13px;border-radius:40px;font-size:11px;letter-spacing:1px;font-weight:700}.status-badge:before{content:'';width:6px;height:6px;border-radius:50%;background:currentColor}
    .tabs{display:flex;gap:6px;padding:5px;background:#111b28;border:1px solid var(--border);border-radius:12px;align-self:flex-start}.tab-btn{display:block;padding:9px 24px;border-radius:8px;color:var(--text-main);text-decoration:none;font-size:13px;font-weight:600}.tab-btn:hover{color:var(--text-white);background:#ffffff06}.tab-btn.active{background:#26394a;color:var(--text-white);box-shadow:0 2px 5px #0002}.tab-content{display:none;min-width:0}.tab-content.active{display:block}
    .page-intro{margin:2px 0 25px}.eyebrow{text-transform:uppercase;font-size:10px;font-weight:750;letter-spacing:2px;color:var(--accent);margin-bottom:8px}h2{color:var(--text-white);font-size:17px;font-weight:650;margin:24px 0 12px}.page-intro h2{font-size:29px;letter-spacing:-.8px;margin:0 0 5px}.page-intro p{font-size:14px;max-width:650px}p{font-size:13px;line-height:1.7}
    .settings-card,.card{background:linear-gradient(145deg,#152130,#111b28);border:1px solid var(--border);border-radius:16px;padding:24px;min-width:0}.settings-card{margin-bottom:22px}.section-heading{display:flex;align-items:flex-start;gap:13px;margin-bottom:23px}.section-number{font-family:Consolas,monospace;font-size:11px;color:var(--accent);padding:6px 8px;border-radius:7px;background:#65e7bc0b;border:1px solid #65e7bc22}.section-heading h2{margin:0 0 3px;font-size:18px}.section-heading p{color:#8ba0b7;font-size:12px}.settings-grid{display:grid;grid-template-columns:1fr 1fr;gap:22px}.settings-grid>fieldset{min-width:0;border:1px solid var(--border);border-radius:12px;padding:20px;background:#0d172333}legend{padding:0 8px;color:var(--text-white);font-size:12px;font-weight:650}
    form{min-width:0}label{display:block;color:#d5e1ee;font-size:12px;font-weight:550;margin-bottom:7px}input:not([type=checkbox]):not([type=radio]){display:block;width:100%;min-width:0;min-height:44px;border:1px solid #304358;border-radius:8px;background:#0c1623;color:var(--text-white);font:inherit;font-size:13px;padding:11px 13px;transition:border-color .15s,box-shadow .15s}input::placeholder{color:#667e96}input:hover{border-color:#48617a}input:focus-visible,button:focus-visible,a:focus-visible{outline:2px solid var(--accent);outline-offset:3px}input:focus{border-color:var(--accent);box-shadow:0 0 0 3px #65e7bc0a;outline:none}input[type=file]{font-size:12px}input::file-selector-button{background:#23384b;border:0;border-radius:5px;color:var(--text-white);padding:6px 10px;margin-right:10px}
    .form-group{display:flex;flex-direction:column;gap:0;margin-bottom:17px;min-width:0}.form-row{display:grid;grid-template-columns:1fr 1fr;gap:16px}.hint{font-size:11px;color:#879db4;margin-top:6px}.checkbox-group{display:flex;align-items:center;gap:10px;margin:15px 0}.checkbox-group label{margin:0;font-size:12px;font-weight:450}.checkbox-group input{width:17px;height:17px;accent-color:var(--accent);flex-shrink:0}.switch-row{padding:13px 0;display:flex;justify-content:space-between;align-items:center;gap:15px;border-bottom:1px solid #29384a88;margin-bottom:14px}.switch-row label{margin:0}.switch-row small{display:block;color:#879db4;font-size:11px;font-weight:400;margin-top:4px}.switch-row input{appearance:none;width:38px;height:22px;border-radius:22px;background:#34465b;position:relative;flex-shrink:0;cursor:pointer}.switch-row input:before{content:'';position:absolute;background:#c3d2e0;width:16px;height:16px;border-radius:50%;left:3px;top:3px;transition:transform .15s}.switch-row input:checked{background:var(--accent)}.switch-row input:checked:before{transform:translateX(16px);background:#0e2b27}
    button,.btn-submit{font-family:inherit;background:var(--accent);color:#0c2921;border:1px solid transparent;border-radius:9px;min-height:43px;padding:11px 18px;font-size:12px;font-weight:700;cursor:pointer;letter-spacing:.2px;transition:background .15s,transform .15s}button:hover{background:var(--accent-hover)}button:active{transform:translateY(1px)}button:disabled{opacity:.45;cursor:wait}.button-secondary{background:#23364a;border-color:#36516b;color:var(--text-white)}.button-secondary:hover{background:#304960}.button-warning{background:#f4c886;color:#3c2b14}.actions{display:flex;flex-wrap:wrap;gap:10px;margin-top:21px}.settings-card>form>button{margin-top:10px}.actions button{flex:0 1 auto}.notice{font-size:12px;padding:12px 15px;border:1px solid #36516b;border-radius:9px;background:#1a2b3d;margin-top:15px;overflow-wrap:anywhere}.notice:empty{display:none}.notice-success{color:#b3f7dc;background:#65e7bc08;border-color:#65e7bc44}.notice-error{color:#ffc0c8;background:#ff8c9a08;border-color:#ff8c9a55}.destination{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin:18px 0;padding:14px;border-radius:9px;background:#0b1520;border:1px solid #27394a}.destination small{display:block;font-size:10px;color:#839ab1;margin-bottom:3px}.destination strong{font-family:Consolas,monospace;color:#d4e6f6;font-size:13px;font-weight:500}.technical{border-top:1px solid var(--border);padding-top:13px;margin-top:17px;color:#748ba2;font-size:11px}
    .grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:16px}.card .label{font-size:10px;text-transform:uppercase;color:#8ea6be;font-weight:650;letter-spacing:.8px}.card .value{font-size:27px;font-weight:650;letter-spacing:-1px;color:var(--text-white);margin-top:9px}.card-full{grid-column:1/-1}.progress-bar-container{height:4px;background:#293b50;margin-top:16px;border-radius:4px;overflow:hidden}.progress-bar{height:100%;width:0;background:var(--accent);transition:width .3s}.progress-bar.success{background:var(--success)}.progress-bar.danger{background:var(--danger)}
    .console{white-space:pre-wrap;overflow-wrap:anywhere;background:#0b1420;border:1px solid #243649;border-radius:10px;padding:16px 18px;font:12px/1.9 Consolas,monospace;color:#a7bfd5;max-height:420px;overflow:auto;min-width:0}.console span{color:var(--accent)}.console.is-error{border-left:3px solid var(--danger);color:#eac2c8}.console.is-success{border-left:3px solid var(--success)}.log-title{font-size:11px;color:#92a9bf;font-weight:600;margin:20px 0 10px}.diagnostics-grid{display:grid;grid-template-columns:1fr 1fr;gap:20px}.diagnostics-grid>section{min-width:0}.diagnostics-grid h2{margin-top:0}.full-width{grid-column:1/-1}#diagnostics{padding:0}#diagnostics>.settings-card{margin-bottom:0}#diagnostics form{margin:14px 0 20px}#diagnostics form button{margin-top:10px}#updated{font:11px Consolas,monospace;color:#738ba2;text-align:right;padding:0 2px}
    table{width:100%;border-collapse:collapse;margin-top:13px;font-size:12px;text-align:left}th,td{padding:12px 8px;border-bottom:1px solid var(--border);overflow-wrap:anywhere}th{color:#8ca5bd;font-size:10px;font-weight:600;text-transform:uppercase;letter-spacing:.7px}.badge{display:inline-block;border-radius:5px;padding:3px 7px;font-size:10px;font-weight:600}.badge.active{color:var(--success);background:#65e7bc10}.badge.empty{color:#8194a7;background:#ffffff06}code{font-family:Consolas,monospace;font-size:12px;color:#b1ead9}
    @media(max-width:800px){body{padding:22px 16px}.settings-grid,.diagnostics-grid{grid-template-columns:1fr}.grid{grid-template-columns:repeat(2,minmax(0,1fr))}.settings-card,.card{padding:19px}.page-intro h2{font-size:25px}}
    @media(max-width:480px){body{padding:16px 12px}.wrapper{gap:16px}header h1{font-size:19px}header h1 span{display:none}.brand-caption{font-size:10px}.status-badge{font-size:9px;padding:6px 9px}.tabs{align-self:stretch}.tab-btn{flex:1;text-align:center;padding:9px 14px}.form-row{grid-template-columns:1fr;gap:0}.actions{flex-direction:column}.actions button{width:100%}.settings-grid>fieldset{padding:15px}.card .value{font-size:23px}.console{padding:13px;font-size:11px}.destination{gap:8px}}
    @media(prefers-reduced-motion:reduce){*{transition:none!important;animation:none!important}}
  </style>
</head>
<body>
  <div class="wrapper">
    <header>
      <div><h1>OCTOPUS <span>ESP32</span></h1><p class="brand-caption">Rede, telemetria e controle</p></div>
      <div class="status-badge" id="connection" role="status">CONECTANDO...</div>
    </header>

    <div class="tabs">
      <a id="nav-status" class="tab-btn" href="/">Visão geral</a>
      <a id="nav-config" class="tab-btn" href="/config">Configuração</a>
    </div>

)rawliteral";

const char* WifiPortalManager::_htmlStatus PROGMEM = R"rawliteral(
    <!-- STATUS -->
    <div id="dash" class="tab-content active">
      <div class="page-intro"><div class="eyebrow">Monitoramento em tempo real</div><h2>Visão geral do dispositivo</h2><p>Conexão, recursos e atividade do Octopus, em um só lugar.</p></div>
      <div class="grid">
        <div class="card">
          <div class="label">Data do sistema</div>
          <div class="value" id="val-data">--/--/----</div>
        </div>
        <div class="card">
          <div class="label">Hora local · NTP</div>
          <div class="value" id="val-hora" style="color: var(--success);">--:--:--</div>
        </div>
        <div class="card">
          <div class="label">Memória disponível</div>
          <div class="value"><span id="val-ram">0</span> KB</div>
          <div class="progress-bar-container"><div id="bar-ram" class="progress-bar success"></div></div>
        </div>
        <div class="card">
          <div class="label">Sinal Wi-Fi</div>
          <div class="value"><span id="val-rssi">0</span> dBm</div>
          <div class="progress-bar-container"><div id="bar-rssi" class="progress-bar"></div></div>
        </div>

        <div class="card card-full">
          <div class="label">Redes Wi-Fi salvas · até 5 redes</div>
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
          <div class="label">Conectividade</div>
          <div class="console" style="margin-top: 10px;">
            <div><span>SSID Conectado:</span> <span id="val-ssid" style="color:var(--text-white);">--</span></div>
            <div><span>Endereço IP:</span> <span id="val-ip" style="color:var(--text-white);">--</span></div>
            <div><span>Endereço MAC:</span> <span id="val-mac" style="color:var(--text-white);">--</span></div>
            <div><span>Tempo de Atividade (Uptime):</span> <span id="val-uptime" style="color:var(--text-white);">0</span> s</div>
          </div>
        </div>
      </div>
    </div>

    <div id="diagnostics" class="diagnostics-grid">
      <section class="settings-card"><h2>Hardware e firmware</h2><div id="hardware" class="console">Aguardando dados...</div></section>
      <section class="settings-card"><h2>Memória e armazenamento</h2><div id="memory" class="console">Aguardando dados...</div></section>
      <section class="settings-card full-width"><div class="section-heading"><span class="section-number">SD</span><div><h2>Arquivos do cartão</h2><p>Transfira inventários, registros e arquivos do dispositivo.</p></div></div>
        <div class="settings-grid"><fieldset><legend>Baixar arquivo</legend><p class="hint">Informe o caminho completo no cartão SD.</p><form action="/sd/download" method="GET"><label for="sd-path">Caminho do arquivo</label><input id="sd-path" name="arquivo" value="/netscan.json" required maxlength="160"><button type="submit">Baixar arquivo</button></form></fieldset>
        <fieldset><legend>Enviar ao dispositivo</legend><p class="hint">Salvo na raiz do SD. Substitui um arquivo com o mesmo nome.</p><form action="/sd/upload" method="POST" enctype="multipart/form-data"><label for="sd-upload">Arquivo do computador</label><input id="sd-upload" name="arquivo" type="file" required><button type="submit" class="button-secondary">Enviar para o SD</button></form></fieldset></div>
      </section>
      <section class="settings-card"><h2>Rede e serviços</h2><div id="services" class="console">Aguardando dados...</div></section>
      <section class="settings-card"><h2>Reinicialização e falhas</h2><div id="reset" class="console">Aguardando dados...</div>
      <p style="margin:12px 0">Envie <code>crash_info</code> por UDP para consultar o panic, tarefa e backtrace. O relatório é arquivado em /crash_*.txt no SD; o registro interno só é apagado após verificar a cópia. Use sd_list e sd_read:/arquivo para recuperar relatórios.</p>
      <div class="console">Consultas UDP: help · info · status · reason · crash_info · cpu · temp · ram · heap · flash · net_info · wifi_list · sd_status · uptime · time · date · age</div>
      <p style="margin-top:12px">Histórico UDP: /udp_log.txt (RX e TX com data, hora, cliente e resultado de envio). Rotação de 512 KB para /udp_log_old.txt. Consulte com sd_read:/udp_log.txt. Sem SD, não há gravação.</p>
      </section><section class="settings-card full-width"><div class="section-heading"><span class="section-number">SQL</span><div><h2>Atividade da rede</h2><p>Última varredura e comunicação com LogsRede / netscan.</p></div></div><pre id="netscan-status" class="console" role="status">Aguardando dados...</pre></section>
    </div>

)rawliteral";

const char* WifiPortalManager::_htmlConfig PROGMEM = R"rawliteral(
    <div id="config" class="tab-content active">
      <div class="page-intro"><div class="eyebrow">Preferências do dispositivo</div><h2>Configuração</h2><p>Ajuste a descoberta da rede, as integrações e a conectividade.</p></div>
      <section class="settings-card">
        <div class="section-heading"><span class="section-number">01</span><div><h2>Descoberta da rede e PostgreSQL</h2><p>Defina a frequência de coleta e o destino dos equipamentos encontrados.</p></div></div>
        <form id="netscan-form" action="/netscan/salvar" method="POST">
          <div class="settings-grid">
            <fieldset><legend>Varredura automática</legend>
              <div class="switch-row"><label for="ns-auto">Escanear a rede automaticamente<small>Busca os equipamentos da rede Wi-Fi local.</small></label><input type="checkbox" id="ns-auto" name="ns_auto" value="1" checked></div>
              <div class="form-group"><label for="ns-interval">Intervalo entre varreduras</label><input id="ns-interval" name="ns_interval" type="number" min="1" max="1440" value="15" required><p class="hint">Em minutos · mínimo 1, máximo 1440.</p></div>
              <div class="switch-row"><label for="ns-sql">Enviar ao PostgreSQL<small>Insere ou atualiza os equipamentos pelo MAC.</small></label><input type="checkbox" id="ns-sql" name="ns_sql" value="1"></div>
              <div class="destination"><div><small>BANCO DE DADOS</small><strong>LogsRede</strong></div><div><small>TABELA FIXA</small><strong>public.netscan</strong></div></div>
              <p class="hint">O JSON continua salvo no cartão SD. O LED azul pisca durante a varredura.</p>
            </fieldset>
            <fieldset><legend>Conexão com o banco</legend>
              <div class="form-row"><div class="form-group"><label for="ns-host">Servidor PostgreSQL</label><input id="ns-host" name="ns_host" value="192.168.0.20" maxlength="63" required></div><div class="form-group"><label for="ns-port">Porta</label><input id="ns-port" name="ns_port" type="number" value="5432" min="1" max="65535" required></div></div>
              <div class="form-group"><label for="ns-user">Usuário</label><input id="ns-user" name="ns_user" maxlength="63" autocomplete="username" placeholder="Usuário do PostgreSQL"></div>
              <div class="form-group"><label for="ns-password">Senha</label><input id="ns-password" name="ns_password" type="password" maxlength="127" autocomplete="new-password" placeholder="Em branco, mantém a senha atual"><p id="ns-password-state" class="hint" role="status">Consultando senha salva...</p></div>
              <div class="checkbox-group"><input type="checkbox" id="ns-clear-password" name="ns_clear_password" value="1"><label for="ns-clear-password">Remover a senha salva</label></div>
              <p class="hint">Salvar grava as preferências. Testar confirma o login e a leitura da tabela.</p>
            </fieldset>
          </div>
          <div class="actions"><button id="ns-save" type="submit">Salvar configuração</button><button id="ns-test" class="button-secondary" type="submit">Salvar e testar login</button></div>
          <p id="ns-result" class="notice" role="status"></p>
          <p class="technical">Conexão local sem TLS · SCRAM-SHA-256 ou MD5 · credenciais ASCII.</p>
        </form>
        <p class="log-title">ATIVIDADE DA COLETA E DO BANCO</p><pre id="ns-state" class="console" role="status">Aguardando dados...</pre>
      </section>
      <section class="settings-card">
        <div class="section-heading"><span class="section-number">02</span><div><h2>Prometheus</h2><p>Métricas em uma porta exclusiva. O painel web continua na porta 80.</p></div></div>
        <form id="prom-form" action="/prometheus/salvar" method="POST">
          <div class="switch-row"><label for="prom-enabled">Disponibilizar métricas<small>Ativa a coleta pelo endpoint /metrics.</small></label><input type="checkbox" id="prom-enabled" name="prom_enabled" value="1"></div>
          <div class="form-row"><div class="form-group"><label for="prom-host">Coletor autorizado (opcional)</label><input type="text" id="prom-host" name="prom_host" maxlength="253" placeholder="Ex.: 192.168.0.10"><p class="hint">IP ou hostname, sem http:// e sem porta. Em branco, permite qualquer cliente.</p></div><div class="form-group"><label for="prom-port">Porta de métricas no Octopus</label><input type="number" id="prom-port" name="prom_port" min="1" max="65535" value="9120" required><p class="hint">Portas 80 e 3232 são reservadas.</p></div></div>
          <button id="prom-save" type="submit">Salvar Prometheus</button><p id="prom-result" class="notice" role="status"></p>
        </form>
        <div class="settings-grid" style="margin-top:20px"><div><p class="log-title">ESTADO DA COLETA</p><div id="prom-state" class="console">Aguardando configuração...</div></div><div><p class="log-title">EXEMPLO DE PROMETHEUS.YML</p><pre id="prom-yaml" class="console"></pre></div></div>
      </section>
      <section class="settings-card">
        <div class="section-heading"><span class="section-number">03</span><div><h2>Wi-Fi e relógio</h2><p>As alterações desta seção reiniciam o dispositivo.</p></div></div>
        <form action="/salvar" method="POST">
          <div class="form-row"><div class="form-group"><label for="wifi-ssid">Nome da rede Wi-Fi</label><input id="wifi-ssid" type="text" name="ssid" placeholder="MinhaRede_2G" required></div><div class="form-group"><label for="wifi-password">Senha da rede</label><input id="wifi-password" type="password" name="senha" autocomplete="new-password" placeholder="Senha do Wi-Fi"></div></div>
          <div class="form-row"><div class="form-group"><label for="wifi-udp">Porta UDP</label><input id="wifi-udp" type="number" name="udpport" value="4210" min="1" max="65535" required></div><div class="form-group"><label for="wifi-blink">Intervalo padrão do LED (ms)</label><input id="wifi-blink" type="number" name="blink" value="500" required></div></div>
          <div class="form-row"><div class="form-group"><label for="wifi-zone">Fuso horário (GMT)</label><input id="wifi-zone" type="number" name="fuso" value="-3" min="-12" max="14" required></div><div class="checkbox-group"><input type="checkbox" id="dst" name="dst" value="1"><label for="dst">Ativar horário de verão</label></div></div>
          <div class="actions"><button type="submit" class="button-warning">Salvar e reiniciar dispositivo</button></div>
        </form>
      </section>
    </div>
)rawliteral";

const char* WifiPortalManager::_htmlFooter PROGMEM = R"rawliteral(
    <p id="updated" style="margin-top:12px" aria-live="polite">Atualização automática a cada 2 segundos.</p>
  </div>

  <script>
    const isConfigPage = Boolean(document.getElementById('netscan-form'));
    document.title = isConfigPage ? 'Octopus — Configuração' : 'Octopus — Status';
    document.getElementById(isConfigPage ? 'nav-config' : 'nav-status').classList.add('active');
    let configLoaded = false;
    document.getElementById('netscan-form')?.addEventListener('submit', async event => {
      event.preventDefault();
      const result = document.getElementById('ns-result');
      const testar = event.submitter?.id === 'ns-test';
      const port = Number(document.getElementById('ns-port').value);
      const interval = Number(document.getElementById('ns-interval').value);
      if (!Number.isInteger(port) || port < 1 || port > 65535 || !Number.isInteger(interval) || interval < 1 || interval > 1440) {
        result.textContent = 'Porta ou intervalo inválido.'; return;
      }
      const body = new URLSearchParams();
      for (const name of ['host', 'port', 'user', 'password', 'interval']) body.set('ns_' + name, document.getElementById('ns-' + name).value);
      for (const name of ['auto', 'sql', 'clear-password']) if (document.getElementById('ns-' + name).checked) body.set('ns_' + name.replace('-', '_'), '1');
      result.textContent = 'Salvando...';
      document.getElementById('ns-save').disabled = true;
      document.getElementById('ns-test').disabled = true;
      let testeIniciado = false;
      try {
        const response = await fetch('/netscan/salvar', {method:'POST', body, signal:AbortSignal.timeout(10000)});
        result.textContent = await response.text();
        result.className = 'notice ' + (response.ok ? 'notice-success' : 'notice-error');
        if (response.ok) {
          document.getElementById('ns-password').value = '';
          document.getElementById('ns-clear-password').checked = false;
          configLoaded = false;
          if (testar) {
            const test = await fetch('/netscan/testar', {method:'POST', signal:AbortSignal.timeout(5000)});
            result.textContent += ' ' + await test.text();
            result.className = 'notice ' + (test.ok ? 'notice-success' : 'notice-error');
            testeIniciado = test.ok;
          }
        }
      } catch (_) { result.textContent = 'Não foi possível confirmar a operação. Confira a conexão e o estado salvo antes de repetir.'; }
      finally { document.getElementById('ns-save').disabled = false; document.getElementById('ns-test').disabled = testeIniciado; }
    });
    document.getElementById('prom-form')?.addEventListener('submit', async event => {
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
        if (!isConfigPage) {
        document.getElementById('hardware').textContent = `Modelo: ${data.chip} · revisão ${data.revisao} · ${data.nucleos} núcleos\nCPU: ${data.cpu_mhz} MHz · temperatura: ${data.temperatura ?? 'indisponível'} °C\nFirmware: ${data.firmware} · build: ${data.build}\nESP-IDF: ${data.sdk}`;
        document.getElementById('memory').textContent = `Heap livre / total: ${data.heap_livre} / ${data.heap_total} bytes\nMenor heap livre: ${data.heap_min} bytes · maior bloco: ${data.heap_bloco} bytes\nPSRAM livre / total: ${data.psram_livre} / ${data.psram_total} bytes\nFlash física: ${data.flash_total} bytes · frequência: ${data.flash_mhz} MHz\nFirmware: ${data.sketch} bytes · espaço disponível para sketch: ${data.sketch_livre} bytes\nCartão SD: ${data.sd ? 'inicializado' : 'indisponível'}`;
        document.getElementById('services').textContent = `Modo: ${data.modo} · canal: ${data.canal}\nGateway: ${data.gateway} · máscara: ${data.mascara} · DNS: ${data.dns}\nHTTP: porta 80 · UDP: ${data.udp_port}\nOTA: ${data.ota ? 'inicializado (OCTOPUS_NODE)' : 'não inicializado'}\nRelógio: ${data.relogio_valido ? 'data/hora válidas' : 'aguardando sincronização'} · GMT ${data.fuso} · horário de verão: ${data.dst ? 'ativado' : 'desativado'}\nPisca padrão: ${data.blink} ms`;
        document.getElementById('reset').textContent = `Último reset: ${data.reset}\nCore dump em flash: ${data.coredump ? 'habilitado (ELF)' : 'não habilitado neste firmware'}\nTempo ativo: ${Math.floor(data.uptime/86400)}d ${Math.floor(data.uptime/3600)%24}h ${Math.floor(data.uptime/60)%60}m ${data.uptime%60}s`;
        }
        document.getElementById('updated').textContent = 'Última leitura: ' + new Date().toLocaleTimeString('pt-BR');
        if (isConfigPage) {
        document.getElementById('prom-state').textContent = `${data.prom_status}\nEndpoint: http://${data.ip}:${data.prom_port}/metrics\nServidor autorizado: ${data.prom_host || 'qualquer cliente da rede'}\nSD: ${data.sd ? 'inicializado' : 'indisponível'} · total: ${data.sd_total_bytes} B · usado: ${data.sd_used_bytes} B\nLogs gravados: ${data.sd_log_writes} · erros: ${data.sd_write_errors}`;
        document.getElementById('prom-yaml').textContent = `scrape_configs:\n  - job_name: octopus\n    scrape_interval: 15s\n    metrics_path: /metrics\n    static_configs:\n      - targets: ['${data.ip}:${data.prom_port}']`;
        }
        if (isConfigPage && !configLoaded) {
          for (const [name, value] of Object.entries({udpport:data.udp_port, blink:data.blink, fuso:data.fuso})) document.querySelector(`[name="${name}"]`).value = value;
          document.getElementById('dst').checked = data.dst;
          document.getElementById('prom-enabled').checked = data.prom_enabled;
          document.getElementById('prom-host').value = data.prom_host;
          document.getElementById('prom-port').value = data.prom_port;
          document.getElementById('ns-auto').checked = data.ns_auto ?? true;
          document.getElementById('ns-sql').checked = data.ns_sql ?? false;
          for (const name of ['host', 'port', 'user', 'interval']) {
            const fallback = {host:'192.168.0.20', port:5432, user:'', interval:15};
            document.getElementById('ns-' + name).value = data['ns_' + name] ?? fallback[name];
          }
          configLoaded = true;
        }
        
        const state = document.getElementById(isConfigPage ? 'ns-state' : 'netscan-status');
        state.textContent = data.ns_status || 'Aguardando dados...';
        state.className = 'console' + (data.ns_status?.includes('[NETSCAN SQL] ERRO') ? ' is-error' : data.ns_status?.includes('[NETSCAN SQL] OK') ? ' is-success' : '');
        if (isConfigPage) {
          document.getElementById('ns-password-state').textContent = data.ns_password_set ? 'Existe uma senha salva nas prefs.' : 'Nenhuma senha salva nas prefs.';
          document.getElementById('ns-test').disabled = Boolean(data.ns_testing);
        }
        if (!isConfigPage) {
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
        }

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
            tentativas++;
        }

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
    _server.on("/config", HTTP_GET, std::bind(&WifiPortalManager::handleConfig, this));
    _server.on("/salvar", HTTP_POST, std::bind(&WifiPortalManager::handleSalvar, this));
    _server.on("/status", HTTP_GET, std::bind(&WifiPortalManager::handleStatus, this)); 
    _server.on("/sd/download", HTTP_GET, std::bind(&WifiPortalManager::handleDownload, this));
    _server.on("/netscan/salvar", HTTP_POST, [this]() { PrometheusRequest r(prometheus); NetScan::salvarWeb(_server); });
    _server.on("/netscan/testar", HTTP_POST, [this]() { PrometheusRequest r(prometheus); String resposta = NetScan::testarPostgres(); r.erro = resposta.startsWith("Erro:"); _server.send(r.erro ? 409 : 202, "text/plain; charset=utf-8", resposta); });
    _server.on("/sd/upload", HTTP_POST, std::bind(&WifiPortalManager::handleUploadFim, this), std::bind(&WifiPortalManager::handleUpload, this));
    _server.on("/prometheus/salvar", HTTP_POST, [this]() { prometheus.salvarConfiguracao(_server); });
    _server.onNotFound([this]() { PrometheusRequest r(prometheus); r.erro = true; _server.send(404, "text/plain", "Nao encontrado\n"); });
    _server.begin();
    prometheus.iniciarServidor();
}

// Inicializa o servidor HTTP interno no modo Station
void WifiPortalManager::iniciarWebServerLocal() {
    _modoAP = false;
    _server.on("/", HTTP_GET, std::bind(&WifiPortalManager::handleRoot, this));
    _server.on("/config", HTTP_GET, std::bind(&WifiPortalManager::handleConfig, this));
    _server.on("/salvar", HTTP_POST, std::bind(&WifiPortalManager::handleSalvar, this));
    _server.on("/status", HTTP_GET, std::bind(&WifiPortalManager::handleStatus, this)); 
    _server.on("/sd/download", HTTP_GET, std::bind(&WifiPortalManager::handleDownload, this));
    _server.on("/netscan/salvar", HTTP_POST, [this]() { PrometheusRequest r(prometheus); NetScan::salvarWeb(_server); });
    _server.on("/netscan/testar", HTTP_POST, [this]() { PrometheusRequest r(prometheus); String resposta = NetScan::testarPostgres(); r.erro = resposta.startsWith("Erro:"); _server.send(r.erro ? 409 : 202, "text/plain; charset=utf-8", resposta); });
    _server.on("/sd/upload", HTTP_POST, std::bind(&WifiPortalManager::handleUploadFim, this), std::bind(&WifiPortalManager::handleUpload, this));
    _server.on("/prometheus/salvar", HTTP_POST, [this]() { prometheus.salvarConfiguracao(_server); });
    _server.onNotFound([this]() { PrometheusRequest r(prometheus); r.erro = true; _server.send(404, "text/plain", "Nao encontrado\n"); });
    _server.begin();
    prometheus.iniciarServidor();
}

void WifiPortalManager::iniciarUDP() {
    if (!_udpMutex) _udpMutex = xSemaphoreCreateMutex();
    bool aberto = _udpMutex && _udp.begin(getUdpPort()) == 1;
    prometheus.registrarListenerUDP(aberto);
    if (!aberto) Serial.println(F("[UDP] Falha ao abrir porta UDP ou criar mutex."));
}

void WifiPortalManager::processarPortal() {
    { RadioWatchdog::Escopo etapa(RadioWatchdog::HTTP_PAINEL, true); _server.handleClient(); }
    prometheus.processar();
}

namespace {
constexpr const char* UPLOAD_TEMP = "/.octopus-upload.tmp";
bool caminhoSDValido(const String& path) {
    if (!path.startsWith("/") || path.length() < 2 || path.length() > 160 ||
        path.indexOf("..") >= 0 || path.indexOf("//") >= 0 || path.endsWith("/") ||
        path.indexOf(".octopus-upload") >= 0 || path.endsWith(".tmp") || path.endsWith(".bak")) return false;
    for (size_t i = 1; i < path.length(); ++i) {
        char c = path[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '/' || c == '.' || c == '-' || c == '_' || c == ' ')) return false;
    }
    return true;
}
}

void WifiPortalManager::handleDownload() {
    PrometheusRequest request(prometheus);
    String path = _server.arg("arquivo");
    if (!path.startsWith("/")) path = "/" + path;
    if (!caminhoSDValido(path)) {
        request.erro = true; _server.send(400, "text/plain; charset=utf-8", "Caminho de arquivo invalido.\n"); return;
    }
    if (!storage.isAtivo()) {
        request.erro = true; _server.send(503, "text/plain; charset=utf-8", "Cartao SD indisponivel.\n"); return;
    }
    File file = storage.abrirArquivo(path.c_str(), FILE_READ);
    if (!file || file.isDirectory()) {
        file.close(); request.erro = true; _server.send(404, "text/plain; charset=utf-8", "Arquivo nao encontrado.\n"); return;
    }
    size_t restante = file.size(); // Logs podem crescer durante o download.
    String nome = path.substring(path.lastIndexOf('/') + 1);
    _server.sendHeader("Content-Disposition", "attachment; filename=\"" + nome + "\"");
    _server.sendHeader("Cache-Control", "no-store");
    _server.sendHeader("X-Content-Type-Options", "nosniff");
    _server.sendHeader("Connection", "close");
    _server.setContentLength(restante);
    _server.send(200, "application/octet-stream", "");
    auto client = _server.client();
    uint8_t buffer[1024];
    uint32_t progresso = millis();
    // Escrita nao bloqueante: evita as repeticoes longas de NetworkClient.write.
    while (restante && client.connected()) {
        size_t lidos = file.read(buffer, restante < sizeof(buffer) ? restante : sizeof(buffer));
        if (!lidos) break;
        size_t enviados = 0;
        while (enviados < lidos && client.connected()) {
            int n = lwip_send(client.fd(), buffer + enviados, lidos - enviados, MSG_DONTWAIT);
            if (n > 0) {
                enviados += n; restante -= n; progresso = millis();
                RadioWatchdog::checkpoint();
            } else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) break;
            if (millis() - progresso >= 1500) break;
            vTaskDelay(1);
        }
        if (enviados != lidos) break;
        vTaskDelay(1);
    }
    request.erro = restante != 0;
    file.close();
    client.stop(); // Content-Length permite ao navegador detectar transferencia incompleta.
}

void WifiPortalManager::handleUpload() {
    HTTPUpload& upload = _server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (++_uploadPartes > 1 || _uploadPronto || _uploadFile) {
            _uploadErro = "Envie apenas um arquivo por requisicao.";
            _uploadFile.close(); _uploadPronto = false;
            return;
        }
        _uploadErro = ""; _uploadBytes = 0;
        _uploadDestino = "/" + upload.filename;
        if (upload.name != "arquivo" || upload.filename.indexOf('/') >= 0 || !caminhoSDValido(_uploadDestino))
            _uploadErro = "Nome invalido. Use letras sem acento, numeros, espacos, ponto, hifen ou sublinhado.";
        else if (!storage.isAtivo()) _uploadErro = "Cartao SD indisponivel.";
        else if (NetScan::emAndamento()) _uploadErro = "Aguarde o netscan terminar antes de enviar arquivos.";
        else {
            File destino = storage.abrirArquivo(_uploadDestino.c_str(), FILE_READ);
            bool diretorio = destino && destino.isDirectory();
            destino.close();
            if (diretorio) _uploadErro = "Ja existe um diretorio com esse nome.";
            if (SD.exists(UPLOAD_TEMP) && !SD.remove(UPLOAD_TEMP)) _uploadErro = "Falha ao limpar arquivo temporario.";
            if (_uploadErro.isEmpty()) {
                _uploadFile = storage.abrirArquivo(UPLOAD_TEMP, FILE_WRITE);
                if (!_uploadFile) _uploadErro = "Falha ao abrir arquivo temporario no SD.";
            }
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (_uploadFile && _uploadErro.isEmpty()) {
            if (_uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize)
                _uploadErro = "Falha ao gravar no SD (verifique o espaco disponivel).";
            else {
                _uploadBytes += upload.currentSize;
                RadioWatchdog::checkpoint();
            }
            vTaskDelay(1);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (_uploadFile) {
            _uploadFile.flush();
            if (_uploadFile.getWriteError() || _uploadBytes != upload.totalSize) _uploadErro = "Upload incompleto ou erro no SD.";
            _uploadFile.close();
            File verificar = storage.abrirArquivo(UPLOAD_TEMP, FILE_READ);
            if (!verificar || verificar.size() != _uploadBytes) _uploadErro = "Falha ao verificar arquivo recebido.";
            verificar.close();
            _uploadPronto = _uploadErro.isEmpty();
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        _uploadFile.close(); _uploadPronto = false; _uploadPartes = 0; _uploadErro = "Upload interrompido.";
        if (storage.isAtivo()) SD.remove(UPLOAD_TEMP);
    }
}

void WifiPortalManager::handleUploadFim() {
    PrometheusRequest request(prometheus);
    bool ok = _uploadPronto && _uploadErro.isEmpty();
    if (ok && NetScan::emAndamento()) { ok = false; _uploadErro = "Netscan em andamento. Tente novamente ao terminar."; }
    if (ok) ok = storage.substituirArquivoSeguro(_uploadDestino.c_str(), UPLOAD_TEMP);
    if (!ok && _uploadErro.isEmpty()) _uploadErro = "Falha ao concluir upload. O destino anterior foi preservado.";
    if (!ok && storage.isAtivo()) SD.remove(UPLOAD_TEMP);
    request.erro = !ok;
    _server.sendHeader("Cache-Control", "no-store");
    String texto = ok ? "Arquivo enviado com sucesso para a raiz do SD." : _uploadErro;
    _server.send(ok ? 200 : 400, "text/html; charset=utf-8",
        "<!doctype html><html lang=\"pt-BR\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Upload SD</title><p>" + texto + "</p><p><a href=\"/\">Voltar ao painel</a></p></html>");
    _uploadFile.close(); _uploadPronto = false; _uploadPartes = 0; _uploadBytes = 0; _uploadDestino = ""; _uploadErro = "";
}

void WifiPortalManager::handleRoot() { 
    enviarPagina(false);
}
void WifiPortalManager::handleConfig() {
    enviarPagina(true);
}
void WifiPortalManager::enviarPagina(bool configuracao) {
    PrometheusRequest request(prometheus);
    _server.sendHeader("Cache-Control", "no-store");
    _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _server.send(200, "text/html; charset=utf-8", "");
    _server.sendContent_P(_htmlHead);
    _server.sendContent_P(configuracao ? _htmlConfig : _htmlStatus);
    _server.sendContent_P(_htmlFooter);
    _server.sendContent("");
}

void WifiPortalManager::handleStatus() {
    PrometheusRequest request(prometheus);
    uint32_t freeHeap = ESP.getFreeHeap() / 1024;
    float flashLivre = (float)ESP.getFreeSketchSpace() / (1024.0 * 1024.0);

    _prefs.begin("wifi_circular", true);
    int head = _prefs.getInt("head", -1);

    String json;
    json.reserve(2400); 
    const auto ns = NetScan::configuracao();
    json = "{";
    json += "\"ns_auto\":" + String(ns.automatico ? "true" : "false") + ",";
    json += "\"ns_sql\":" + String(ns.sqlAtivo ? "true" : "false") + ",";
    json += "\"ns_host\":" + jsonString(ns.servidor) + ",";
    json += "\"ns_port\":" + String(ns.porta) + ",";
    json += "\"ns_database\":" + jsonString(ns.banco) + ",";
    json += "\"ns_user\":" + jsonString(ns.usuario) + ",";
    json += "\"ns_password_set\":" + String(ns.senha[0] ? "true" : "false") + ",";
    json += "\"ns_testing\":" + String(NetScan::testePostgresAtivo() ? "true" : "false") + ",";
    json += "\"ns_interval\":" + String(ns.intervaloMin) + ",";
    json += "\"ns_status\":" + jsonString(NetScan::estado()) + ",";
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
    bool enviado = false;
    if (_udpMutex && xSemaphoreTake(_udpMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        enviado = _udp.beginPacket(_respostaIP, _respostaPorta) == 1;
        if (enviado) {
            enviado = _udp.print(resposta) == resposta.length();
            enviado = _udp.endPacket() == 1 && enviado;
        }
        xSemaphoreGive(_udpMutex);
    }
    prometheus.registrarEnvioUDP(enviado);
    storage.gravarLogUDP("[TX][" + ntp.obterApenasData() + " " + ntp.obterApenasHora() +
        "][uptime_ms=" + String(millis()) + "][" + _respostaIP.toString() + ":" + String(_respostaPorta) +
        "][" + (enviado ? "ENVIADO" : "FALHA") + "][bytes=" + String(resposta.length()) + "]\n" + resposta + "\n[/TX]");
}

void WifiPortalManager::definirDestinoResposta(IPAddress ip, uint16_t porta) {
    _respostaIP = ip;
    _respostaPorta = porta;
}

bool WifiPortalManager::checarMensagensUDP(String& msgOut) {
    RadioWatchdog::Escopo etapa(RadioWatchdog::UDP_RECEPCAO);
    if (_modoAP || !_udpMutex) return false;
    if (xSemaphoreTake(_udpMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    int packetSize = _udp.parsePacket();
    if (packetSize) {
        prometheus.registrarPacoteUDPRecebido();
        char buffer[256]; 
        int len = _udp.read((uint8_t*)buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0';
            msgOut = String(buffer);
            _ultimoRemetenteIP = _udp.remoteIP();
            _ultimoRemetentePorta = _udp.remotePort();
            xSemaphoreGive(_udpMutex);
            RadioWatchdog::Escopo etapaLog(RadioWatchdog::UDP_LOG);
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
