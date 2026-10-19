#include "PrometheusMetrics.h"
#include "CrashDiagnostics.h"
#include "defines.h"
#include "ClimaService.h"
extern ClimaService clima;
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <freertos/task.h>
#include <math.h>

namespace {
String label(const String& value) {
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i) {
        char c = value[i];
        if (c == '\\' || c == '"') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\n";
        else out += c;
    }
    return out;
}

// Identidade capturada uma vez por coleta e aplicada a todas as amostras.
struct MetricText {
    String text;
    String deviceLabels;
    explicit MetricText(const String& mac) : deviceLabels("device=\"" + label(mac) + "\"") {
        text.reserve(15000);
    }
};

void sample(MetricText& out, const char* name, const String& value, const String& labels = "") {
    out.text += name;
    out.text += "{" + out.deviceLabels;
    if (labels.length()) out.text += "," + labels;
    out.text += "} " + value + "\n";
}

void metric(MetricText& out, const char* name, const char* help, const String& value,
            const String& labels = "", const char* type = "gauge") {
    out.text += "# HELP "; out.text += name; out.text += ' '; out.text += help; out.text += '\n';
    out.text += "# TYPE "; out.text += name; out.text += ' '; out.text += type; out.text += '\n';
    sample(out, name, value, labels);
}

bool validHost(const String& host) {
    if (host.length() > 253) return false;
    for (size_t i = 0; i < host.length(); ++i) {
        char c = host[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    }
    return host.isEmpty() || (host[0] != '.' && host[0] != '-' && host[host.length()-1] != '-');
}
}

PrometheusMetrics::PrometheusMetrics(StorageManager& storage)
    : _storage(storage), _server(9120) {}

void PrometheusMetrics::inicializar() {
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        char id[18];
        snprintf(id, sizeof(id), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        _device = id;
    }
    _imageBytes = ESP.getSketchSize();
    const esp_partition_t* particao = esp_ota_get_running_partition();
    _partitionBytes = particao ? particao->size : 0;
    Preferences prefs;
    if (prefs.begin("prometheus", true)) {
        _ativo = prefs.getBool("enabled", false);
        _porta = prefs.getUShort("port", 9120);
        _servidor = prefs.getString("host", "");
        prefs.end();
    }
    if (_porta == 0 || _porta == 80 || _porta == 3232) _porta = 9120;
    if (!validHost(_servidor)) { _ativo = false; _servidor = ""; }
    if (prefs.begin("metric_counts", false)) {
        bool validos = (!prefs.isKey("boots") || prefs.getType("boots") == PT_U32) &&
            (!prefs.isKey("watchdogs") || prefs.getType("watchdogs") == PT_U32);
        _bootCount = prefs.getUInt("boots", 0);
        if (_bootCount < UINT32_MAX) ++_bootCount;
        esp_reset_reason_t motivo = esp_reset_reason();
        _watchdogCount = prefs.getUInt("watchdogs", 0);
        if (_watchdogCount < UINT32_MAX &&
            (motivo == ESP_RST_TASK_WDT || motivo == ESP_RST_INT_WDT || motivo == ESP_RST_WDT)) ++_watchdogCount;
        if (validos) {
            bool bootSalvo = prefs.putUInt("boots", _bootCount) == sizeof(uint32_t);
            bool watchdogSalvo = prefs.putUInt("watchdogs", _watchdogCount) == sizeof(uint32_t);
            _contadoresValidos = bootSalvo && watchdogSalvo;
        }
        prefs.end();
    }
    if (!_contadoresValidos) Serial.println("[PROMETHEUS] Contadores persistentes indisponiveis; series omitidas.");
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) ++_desconexoes;
    });
    _server.on("/metrics", HTTP_GET, [this]() { handleMetrics(); });
    _server.onNotFound([this]() {
        uint32_t inicio = micros();
        _server.send(404, "text/plain; charset=utf-8", "Use GET /metrics\n");
        registrarRequisicao(true, micros() - inicio);
    });
}

bool PrometheusMetrics::resetCounters() {
    Preferences prefs;
    if (!prefs.begin("metric_counts", false)) {
        _contadoresValidos = false;
        return false;
    }
    bool ok = prefs.putUInt("boots", 0) == sizeof(uint32_t);
    ok = (prefs.putUInt("watchdogs", 0) == sizeof(uint32_t)) && ok;
    prefs.end();
    _contadoresValidos = ok;
    if (ok) {
        _bootCount = 0;
        _watchdogCount = 0;
    }
    return ok;
}

void PrometheusMetrics::iniciarServidor() {
    if (_iniciado) _server.stop();
    _iniciado = false;
    _resolvido = false;
    if (_ativo) {
        resolverServidor();
        _server.begin(_porta);
        _iniciado = true;
        Serial.printf("[PROMETHEUS] GET /metrics na porta %u\n", _porta);
    }
}

void PrometheusMetrics::resolverServidor() {
    _ultimaResolucao = millis();
    _resolvido = _servidor.isEmpty();
    if (_servidor.length()) {
        _resolvido = _servidorIP.fromString(_servidor);
        if (!_resolvido && WiFi.status() == WL_CONNECTED)
            _resolvido = WiFi.hostByName(_servidor.c_str(), _servidorIP) == 1;
    }
}

String PrometheusMetrics::status() const {
    if (!_ativo) return "Desativado";
    if (!_resolvido) return "Aguardando resolver o servidor autorizado";
    return _servidor.length() ? "Ativo; coleta restrita ao servidor configurado" : "Ativo; coleta liberada na rede local";
}

void PrometheusMetrics::processar() {
    if (_reconfigurar) { _reconfigurar = false; iniciarServidor(); }
    if (_ativo && _servidor.length() && millis() - _ultimaResolucao >= 60000) resolverServidor();
    atualizarCPU();
    if (_iniciado) _server.handleClient();
}

void PrometheusMetrics::salvarConfiguracao(WebServer& painel) {
    uint32_t inicio = micros();
    String host = painel.arg("prom_host");
    host.trim();
    String portaTexto = painel.arg("prom_port");
    bool valido = portaTexto.length() > 0 && portaTexto.length() <= 5;
    for (size_t i = 0; i < portaTexto.length(); ++i)
        valido = valido && portaTexto[i] >= '0' && portaTexto[i] <= '9';
    long novaPorta = portaTexto.toInt();
    valido = valido && novaPorta >= 1 && novaPorta <= 65535 && novaPorta != 80 && novaPorta != 3232 && validHost(host);
    if (!valido) {
        painel.send(400, "text/plain; charset=utf-8", "Use IP/hostname sem http:// nem porta. Porta: 1 a 65535, exceto 80 (painel) e 3232 (OTA).\n");
        registrarRequisicao(true, micros() - inicio);
        return;
    }
    bool habilitar = painel.hasArg("prom_enabled");
    Preferences prefs;
    if (!prefs.begin("prometheus", false)) {
        painel.send(500, "text/plain; charset=utf-8", "Falha ao abrir Preferences. Configuracao mantida.\n");
        registrarRequisicao(true, micros() - inicio);
        return;
    }
    // putString retorna strlen: vazio gravado com sucesso tambem retorna zero.
    prefs.putString("host", host);
    bool ok = prefs.isKey("host") && prefs.getString("host", "") == host;
    ok = prefs.putUShort("port", (uint16_t)novaPorta) > 0 && ok;
    ok = prefs.putBool("enabled", habilitar) > 0 && ok;
    prefs.end();
    if (ok) {
        _servidor = host;
        _porta = novaPorta;
        _ativo = habilitar;
        _reconfigurar = true;
    }
    painel.send(ok ? 200 : 500, "text/plain; charset=utf-8", ok ?
        "Configuracao Prometheus salva. A nova porta sera aplicada agora, sem reiniciar.\n" :
        "Falha ao gravar Preferences; confira a configuracao antes de reiniciar.\n");
    registrarRequisicao(!ok, micros() - inicio);
}

void PrometheusMetrics::registrarRequisicao(bool erro, uint32_t duracaoUs) {
    ++_requisicoes;
    if (erro) ++_erros;
    _ultimaDuracaoUs = duracaoUs;
}

void PrometheusMetrics::registrarComandoUDP(bool erro, uint32_t duracaoUs) {
    ++_comandosUDP;
    if (erro) ++_errosUDP;
    _duracaoUDPUs = duracaoUs;
}

void PrometheusMetrics::registrarErroUDP() { ++_errosUDP; }

void PrometheusMetrics::atualizarCPU() {
#if configGENERATE_RUN_TIME_STATS && configUSE_TRACE_FACILITY
    if (millis() - _ultimaCPU < 5000) return;
    _ultimaCPU = millis();
    if (uxTaskGetNumberOfTasks() > 64) { _cpuPercent = NAN; _baselineCPU = false; return; }
    configRUN_TIME_COUNTER_TYPE total = 0;
    UBaseType_t lidos = uxTaskGetSystemState(_tarefas, 64, &total);
    int64_t agora = esp_timer_get_time();
    uint64_t deltaIdle = 0;
    bool completos = lidos != 0;
    for (BaseType_t core = 0; core < portNUM_PROCESSORS; ++core) {
        TaskHandle_t idle = xTaskGetIdleTaskHandleForCore(core);
        bool encontrado = false;
        for (UBaseType_t i = 0; i < lidos; ++i) if (_tarefas[i].xHandle == idle) {
            auto contador = _tarefas[i].ulRunTimeCounter;
            deltaIdle += static_cast<configRUN_TIME_COUNTER_TYPE>(contador - _ultimoIdle[core]);
            _ultimoIdle[core] = contador;
            encontrado = true;
            break;
        }
        completos = completos && encontrado;
    }
    uint64_t capacidade = (agora - _ultimoTempoCPU) * portNUM_PROCESSORS;
    _cpuPercent = _baselineCPU && completos && capacidade && deltaIdle <= capacidade
        ? 100.0 * (1.0 - static_cast<double>(deltaIdle) / capacidade) : NAN;
    _ultimoTempoCPU = agora;
    _baselineCPU = completos;
#endif
}

void PrometheusMetrics::handleMetrics() {
    uint32_t inicio = micros();
    if (!_resolvido) {
        _server.send(503, "text/plain", "Servidor autorizado ainda nao resolvido\n");
        registrarRequisicao(true, micros() - inicio);
        return;
    }
    IPAddress origem = _server.client().remoteIP();
    if (_servidor.length() && origem != _servidorIP) {
        String mensagem = "Coletor nao autorizado\nIP recebido: " + origem.toString() +
            "\nIP autorizado: " + _servidorIP.toString() +
            "\nServidor configurado: " + _servidor +
            "\nSomente requisicoes originadas do IP autorizado podem coletar.\n";
        _server.sendHeader("Cache-Control", "no-store");
        _server.send(403, "text/plain; charset=utf-8", mensagem);
        Serial.printf("[PROMETHEUS] Coleta bloqueada: origem=%s autorizado=%s\n",
            origem.toString().c_str(), _servidorIP.toString().c_str());
        registrarRequisicao(true, micros() - inicio);
        return;
    }
    ++_scrapes;
    String body = gerarMetricas();
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "text/plain; version=0.0.4; charset=utf-8", body);
    registrarRequisicao(false, micros() - inicio);
}

String PrometheusMetrics::gerarMetricas() {
    String mac = _device.length() ? _device : WiFi.macAddress();
    MetricText out(mac);
    bool online = WiFi.status() == WL_CONNECTED;
    metric(out, "esp_info", "Identificacao do dispositivo; valor sempre 1.", "1",
        "chip=\"" + label(ESP.getChipModel()) + "\",model=\"Octopus\"");
    metric(out, "esp_firmware_version", "Versao e build do firmware; valor sempre 1.", "1",
        "version=\"" + label(Config::FIRMWARE_VERSION) + "\",build=\"" + label(String(__DATE__) + " " + __TIME__) + "\"");
    if (online)
        metric(out, "esp_ip_address", "Endereco IPv4 por interface ativa; valor sempre 1.", "1",
            "ip=\"" + label(WiFi.localIP().toString()) + "\",interface=\"sta\"");
    if ((WiFi.getMode() & WIFI_AP) && WiFi.softAPIP()) {
        String labels = "ip=\"" + label(WiFi.softAPIP().toString()) + "\",interface=\"ap\"";
        if (online) sample(out, "esp_ip_address", "1", labels);
        else metric(out, "esp_ip_address", "Endereco IPv4 por interface ativa; valor sempre 1.", "1", labels);
    }
    metric(out, "esp_mac_address", "Endereco MAC; valor sempre 1.", "1", "mac=\"" + label(mac) + "\"");
    metric(out, "esp_status_online", "1 quando Wi-Fi station conectado, 0 caso contrario.", online ? "1" : "0");
    metric(out, "esp_uptime_seconds", "Tempo desde o boot em segundos, sem rollover de millis.", String((double)esp_timer_get_time() / 1000000.0, 3));
    metric(out, "esp_last_restart_reason", "Codigo esp_reset_reason no label reason; valor sempre 1.", "1",
        "reason=\"" + String(static_cast<int>(esp_reset_reason())) + "\"");
    if (_contadoresValidos) {
        metric(out, "esp_boot_count", "Inicializacoes acumuladas em Preferences.", String(_bootCount));
        metric(out, "esp_reboot_count", "Inicializacoes acumuladas menos a primeira, incluindo ciclos de energia.", String(_bootCount ? _bootCount - 1 : 0));
        metric(out, "esp_watchdog_resets", "Resets por watchdog acumulados em Preferences.", String(_watchdogCount));
    }
    if (online) {
        metric(out, "esp_wifi_rssi_dbm", "Sinal recebido do AP em dBm.", String(WiFi.RSSI()));
        metric(out, "esp_wifi_channel", "Canal da conexao station.", String(WiFi.channel()));
        int8_t potencia;
        if (esp_wifi_get_max_tx_power(&potencia) == ESP_OK)
            metric(out, "esp_wifi_tx_power", "Potencia maxima configurada de transmissao Wi-Fi em dBm.", String(potencia / 4.0, 2));
    }
    metric(out, "esp_wifi_disconnect_count", "Eventos de desconexao station desde o boot.", String(_desconexoes.load()));
    metric(out, "esp_cpu_frequency_mhz", "Frequencia da CPU em MHz.", String(ESP.getCpuFreqMHz()));
    if (isfinite(_cpuPercent)) metric(out, "esp_cpu_usage_percent", "CPU ocupada media entre nucleos; amostrada a cada 5s.", String(_cpuPercent, 3));
    float temperatura = temperatureRead();
    if (isfinite(temperatura)) metric(out, "esp_cpu_temperature_celsius", "Temperatura interna da CPU; nao representa temperatura ambiente.", String(temperatura, 2));
    auto weather = clima.snapshot();
    if (ClimaService::atual(weather)) {
        metric(out, "sensor_temperature_celsius", "Temperatura de Porto Alegre via Open-Meteo em Celsius.",
            String(weather.temperatura, 2), "source=\"open_meteo\",location=\"porto_alegre\"");
        metric(out, "sensor_humidity_percent", "Umidade relativa de Porto Alegre via Open-Meteo em percentual.",
            String(weather.umidade, 0), "location=\"porto_alegre\"");
    }
    if (weather.sincronizado) metric(out, "esp_weather_data_age_seconds", "Segundos desde a ultima consulta de clima bem-sucedida.",
        String((esp_timer_get_time() - weather.atualizadoUs) / 1000000.0, 3));
    metric(out, "esp_task_count", "Numero atual de tarefas FreeRTOS.", String(uxTaskGetNumberOfTasks()));
    metric(out, "esp_heap_free_bytes", "Heap livre em bytes.", String(ESP.getFreeHeap()));
    metric(out, "esp_heap_total_bytes", "Heap total em bytes.", String(ESP.getHeapSize()));
    metric(out, "esp_heap_minimum_free_bytes", "Menor heap livre desde o boot.", String(ESP.getMinFreeHeap()));
    metric(out, "esp_psram_free_bytes", "PSRAM livre; zero quando ausente.", String(ESP.getFreePsram()));
    metric(out, "esp_psram_total_bytes", "PSRAM total; zero quando ausente.", String(ESP.getPsramSize()));
    // Como no ESP32C6, publica somente integracoes existentes e leituras validas.
    metric(out, "gpio_digital_input", "Nivel logico bruto da entrada digital (botao ativo em LOW).", String(digitalRead(Config::PIN_BTN_RST)), "pin=\"" + String(Config::PIN_BTN_RST) + "\"");
    metric(out, "gpio_digital_output", "Nivel logico bruto das saidas digitais.", String(digitalRead(Config::PIN_LED_BLUE)), "pin=\"" + String(Config::PIN_LED_BLUE) + "\"");
    sample(out, "gpio_digital_output", String(digitalRead(Config::PIN_LED_GRN)), "pin=\"" + String(Config::PIN_LED_GRN) + "\"");
    metric(out, "esp_free_rtos_tasks", "Alias de esp_task_count.", String(uxTaskGetNumberOfTasks()));
    if (_partitionBytes) metric(out, "esp_flash_usage_percent", "Sketch em percentual da capacidade da particao em execucao.", String(100.0 * _imageBytes / _partitionBytes, 3));
    metric(out, "application_requests_total", "Requisicoes HTTP concluidas nos servidores web e metricas desde o boot.", String(_requisicoes.load()), "", "counter");
    metric(out, "application_errors_total", "Requisicoes HTTP concluidas com erro desde o boot.", String(_erros.load()), "", "counter");
    metric(out, "application_response_time_ms", "Duracao da ultima requisicao concluida em ms.", String(_ultimaDuracaoUs.load() / 1000.0, 3));
    metric(out, "esp_prometheus_scrapes_total", "Coletas autorizadas desde o boot.", String(_scrapes.load()), "", "counter");
    metric(out, "esp_udp_commands_total", "Comandos UDP concluidos desde o boot.", String(_comandosUDP.load()), "", "counter");
    metric(out, "esp_udp_errors_total", "Erros de comando, falhas TX e descartes por fila cheia desde o boot.", String(_errosUDP.load()), "", "counter");
    metric(out, "esp_udp_response_time_ms", "Duracao do ultimo comando UDP concluido em ms.", String(_duracaoUDPUs.load() / 1000.0, 3));

    StorageManager::MetricasSD sd = _storage.metricas();
    metric(out, "sdcard_mounted", "SD inicializado (1) ou indisponivel (0).", sd.ativo ? "1" : "0");
    metric(out, "sdcard_metrics_available", "1 quando o cache de capacidade e ocupacao SD e valido.", sd.valido ? "1" : "0");
    metric(out, "sdcard_log_records_total", "Registros de log gravados com sucesso desde o boot.", String(sd.gravacoes), "", "counter");
    metric(out, "sdcard_log_errors_total", "Registros de log que falharam por SD ausente, abertura ou escrita incompleta desde o boot.", String(sd.errosAppend), "", "counter");
    metric(out, "sdcard_append_operations_total", "Operacoes de append de log concluidas com sucesso desde o boot.", String(sd.gravacoes), "", "counter");
    metric(out, "sdcard_append_errors_total", "Falhas de append de log desde o boot.", String(sd.errosAppend), "", "counter");
    if (sd.valido) {
        metric(out, "sdcard_capacity_bytes", "Capacidade fisica do SD.", String((unsigned long long)sd.capacidade));
        metric(out, "sdcard_total_bytes", "Tamanho do sistema de arquivos em bytes.", String((unsigned long long)sd.total));
        metric(out, "sdcard_used_bytes", "Espaco usado no sistema de arquivos em bytes.", String((unsigned long long)sd.usado));
        metric(out, "sdcard_free_bytes", "Espaco livre no sistema de arquivos em bytes.", String((unsigned long long)(sd.total - sd.usado)));
        metric(out, "sdcard_usage_percent", "Ocupacao do sistema de arquivos em percentual.", String(100.0 * sd.usado / sd.total, 3));
        metric(out, "sdcard_spi_frequency_hz", "Frequencia SPI configurada para o SD em Hz.", String(Config::SD_SPI_FREQUENCY_HZ));
        String idade = String((esp_timer_get_time() - sd.atualizadoUs) / 1000000.0, 3);
        metric(out, "sdcard_data_age_seconds", "Idade do cache valido de capacidade e ocupacao SD.", idade);
        metric(out, "sdcard_stats_age_seconds", "Alias de sdcard_data_age_seconds mantido para compatibilidade.", idade);
        if (sd.logValido) metric(out, "sdcard_log_size_bytes", "Tamanho dos logs existentes e legiveis em bytes.", String(sd.logBytes), "file=\"/log.txt\"");
        if (sd.udpLogValido) {
            if (sd.logValido) sample(out, "sdcard_log_size_bytes", String(sd.udpLogBytes), "file=\"/udp_log.txt\"");
            else metric(out, "sdcard_log_size_bytes", "Tamanho dos logs existentes e legiveis em bytes.", String(sd.udpLogBytes), "file=\"/udp_log.txt\"");
        }
    }
    metric(out, "sdcard_log_writes_total", "Registros de log gravados com sucesso desde o boot.", String(sd.gravacoes), "", "counter");
    metric(out, "sdcard_write_errors_total", "Falhas detectadas de gravacao de logs desde o boot.", String(sd.erros), "", "counter");
    metric(out, "sdcard_log_rotations_total", "Rotacoes de logs concluidas desde o boot.", String(sd.rotacoes), "", "counter");
    return out.text;
}
