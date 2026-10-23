#include "StorageManager.h"
#include "defines.h"
#include <esp_timer.h>
#include <time.h>

StorageManager::StorageManager(uint8_t csPin, SPIClass& spiInstance) 
    : _csPin(csPin), _spiSD(spiInstance), _sdcardOk(false) {}

bool StorageManager::inicializar() {
    if (!_logMutex) _logMutex = xSemaphoreCreateMutex();
    if (!SD.begin(_csPin, _spiSD, Config::SD_SPI_FREQUENCY_HZ)) {
        Serial.println(F("[SD] Falha ao montar o Cartao SD!"));
        _sdcardOk = false;
    } else {
        _sdcardOk = true;
        Serial.println(F("[SD] Cartao SD montado com sucesso."));
    }
    atualizarMetricas(true);
    return _sdcardOk;
}

void StorageManager::gravarLog(const String& mensagem) {
    // Captura o instante do evento antes de esperar pelo SD, sem aguardar NTP.
    const time_t agora = time(nullptr);
    struct tm horario;
    char dataHora[32] = {};
    if (agora >= 1577836800 && localtime_r(&agora, &horario))
        strftime(dataHora, sizeof(dataHora), "%d/%m/%Y %H:%M:%S", &horario);
    const auto uptime = static_cast<unsigned long long>(esp_timer_get_time() / 1000000);
    String prefixo = dataHora[0] ? "[" + String(dataHora) + "]" : String("[SEM_NTP]");
    char tempo[48];
    snprintf(tempo, sizeof(tempo), " [uptime=%llus] ", uptime);
    prefixo += tempo;
    String texto = mensagem;
    texto.replace("\r\n", "\n");
    texto.replace('\r', '\n');
    while (texto.endsWith("\n")) texto.remove(texto.length() - 1);
    texto.replace("\n", "\n" + prefixo);
    gravarRegistro("/log.txt", "/log_old.txt", prefixo + texto);
}

void StorageManager::gravarLogUDP(const String& mensagem) {
    gravarRegistro("/udp_log.txt", "/udp_log_old.txt", mensagem);
}

void StorageManager::gravarRegistro(const char* caminho, const char* anterior, const String& mensagem) {
    // A varredura FAT pode manter o mutex por segundos; nao prender o radio.
    if (!_logMutex || xSemaphoreTake(_logMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ++_falhasEsperaLog;
        return;
    }
    if (!_sdcardOk) { ++_metricas.erros; ++_metricas.errosAppend; publicarMetricas(); xSemaphoreGive(_logMutex); return; }

    // Sistema de rotação automática de logs de 500KB
    if (SD.exists(caminho)) {
        File checkFile = SD.open(caminho, FILE_READ);
        if (checkFile) {
            size_t totalBytes = checkFile.size();
            checkFile.close();
            if (totalBytes >= _maxLogSize) {
                Serial.println(F("[SD] Log atingiu o limite de 500KB. Rotacionando..."));
                bool removeu = !SD.exists(anterior) || SD.remove(anterior);
                if (removeu && SD.rename(caminho, anterior)) ++_metricas.rotacoes;
                else ++_metricas.erros;
            }
        }
    }

    File arquivoLog = SD.open(caminho, FILE_APPEND);
    if (arquivoLog) {
        size_t gravado = arquivoLog.println(mensagem);
        if (gravado == mensagem.length() + 2) ++_metricas.gravacoes;
        else { ++_metricas.erros; ++_metricas.errosAppend; }
        if (String(caminho) == "/log.txt") { _metricas.logBytes = arquivoLog.size(); _metricas.logValido = true; }
        else if (String(caminho) == "/udp_log.txt") { _metricas.udpLogBytes = arquivoLog.size(); _metricas.udpLogValido = true; }
        arquivoLog.close();
    } else {
        ++_metricas.erros;
        ++_metricas.errosAppend;
        Serial.printf("[SD] Falha ao gravar %s\n", caminho);
    }
    publicarMetricas();
    xSemaphoreGive(_logMutex);
}

void StorageManager::lerParaSerial(const char* filename) {
    if (!_sdcardOk || !SD.exists(filename)) return;
    File file = SD.open(filename, FILE_READ);
    if (!file) return;
    while (file.available()) {
        Serial.write(file.read());
    }
    file.close();
}

bool StorageManager::deletarArquivo(const char* filename) {
    if (!_sdcardOk) return false;
    return SD.remove(filename);
}

bool StorageManager::arquivoExiste(const char* filename) {
    if (!_sdcardOk) return false;
    return SD.exists(filename);
}

File StorageManager::abrirArquivo(const char* filename, const char* mode) {
    if (!_sdcardOk) return File();
    return SD.open(filename, mode);
}

bool StorageManager::isAtivo() {
    return _sdcardOk;
}

bool StorageManager::substituirArquivoSeguro(const char* filename, const char* temporario) {
    if (!_sdcardOk || !_logMutex || xSemaphoreTake(_logMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    String backup = String(filename) + ".upload.bak";
    bool ok = SD.exists(temporario);
    if (ok && SD.exists(backup.c_str())) {
        if (!SD.exists(filename)) ok = SD.rename(backup.c_str(), filename);
        else ok = SD.remove(backup.c_str());
    }
    bool anterior = SD.exists(filename);
    if (ok && anterior) ok = SD.rename(filename, backup.c_str());
    if (ok) {
        ok = SD.rename(temporario, filename);
        if (!ok && anterior) SD.rename(backup.c_str(), filename);
    }
    if (ok && anterior) SD.remove(backup.c_str());
    xSemaphoreGive(_logMutex);
    return ok;
}

bool StorageManager::salvarArquivoSeguro(const char* filename, bool (*escrever)(File&, void*), void* contexto) {
    if (!_sdcardOk || !_logMutex || xSemaphoreTake(_logMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    String temporario = String(filename) + ".tmp";
    String backup = String(filename) + ".bak";
    // Recupera uma troca interrompida antes de iniciar outra escrita.
    bool ok = true;
    if (SD.exists(backup.c_str())) {
        if (!SD.exists(filename)) ok = SD.rename(backup.c_str(), filename);
        else ok = SD.remove(backup.c_str());
    }
    if (ok && SD.exists(temporario.c_str())) ok = SD.remove(temporario.c_str());
    File file = ok ? SD.open(temporario.c_str(), FILE_WRITE) : File();
    ok = file && escrever(file, contexto);
    if (file) { file.flush(); ok = ok && !file.getWriteError(); file.close(); }
    bool anterior = SD.exists(filename);
    if (ok && anterior) ok = SD.rename(filename, backup.c_str());
    if (ok) {
        ok = SD.rename(temporario.c_str(), filename);
        if (!ok && anterior) SD.rename(backup.c_str(), filename);
    }
    if (ok && anterior) SD.remove(backup.c_str());
    if (!ok) SD.remove(temporario.c_str());
    xSemaphoreGive(_logMutex);
    return ok;
}

StorageManager::MetricasSD StorageManager::metricas() {
    // O HTTP copia somente memoria; nunca espera por varredura FAT ou escrita no SD.
    portENTER_CRITICAL(&_metricasMux);
    MetricasSD copia = _snapshot;
    portEXIT_CRITICAL(&_metricasMux);
    uint32_t falhasEspera = _falhasEsperaLog.load();
    copia.erros += falhasEspera;
    copia.errosAppend += falhasEspera;
    return copia;
}

void StorageManager::publicarMetricas() {
    portENTER_CRITICAL(&_metricasMux);
    _snapshot = _metricas;
    portEXIT_CRITICAL(&_metricasMux);
}

void StorageManager::atualizarMetricas(bool forcar) {
    if (!_logMutex || (!forcar && millis() - _metricas.atualizadoEm < 30000)) return;
    // A coleta HTTP usa o cache, evitando varrer a FAT a cada scrape.
    if (xSemaphoreTake(_logMutex, 0) != pdTRUE) return;
    _metricas.ativo = _sdcardOk;
    _metricas.valido = false;
    _metricas.logValido = false;
    _metricas.udpLogValido = false;
    if (_sdcardOk) {
        _metricas.capacidade = SD.cardSize();
        _metricas.total = SD.totalBytes();
        _metricas.usado = SD.usedBytes();
        _metricas.valido = _metricas.capacidade && _metricas.total && _metricas.usado <= _metricas.total;
        File log = SD.open("/log.txt", FILE_READ);
        _metricas.logBytes = log ? log.size() : 0;
        _metricas.logValido = log && !log.isDirectory();
        if (log) log.close();
        log = SD.open("/udp_log.txt", FILE_READ);
        _metricas.udpLogBytes = log ? log.size() : 0;
        _metricas.udpLogValido = log && !log.isDirectory();
        if (log) log.close();
    }
    _metricas.atualizadoEm = millis();
    if (_metricas.valido) _metricas.atualizadoUs = esp_timer_get_time();
    publicarMetricas();
    xSemaphoreGive(_logMutex);
}
