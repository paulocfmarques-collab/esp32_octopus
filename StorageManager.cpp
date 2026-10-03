#include "StorageManager.h"

StorageManager::StorageManager(uint8_t csPin, SPIClass& spiInstance) 
    : _csPin(csPin), _spiSD(spiInstance), _sdcardOk(false) {}

bool StorageManager::inicializar() {
    if (!SD.begin(_csPin, _spiSD)) {
        Serial.println(F("[SD] Falha ao montar o Cartao SD!"));
        _sdcardOk = false;
    } else {
        _sdcardOk = true;
        Serial.println(F("[SD] Cartao SD montado com sucesso."));
    }
    return _sdcardOk;
}

void StorageManager::gravarLog(const String& mensagem) {
    if (!_sdcardOk) return;

    // Sistema de rotação automática de logs
    if (SD.exists("/log.txt")) {
        File checkFile = SD.open("/log.txt", FILE_READ);
        if (checkFile) {
            size_t totalBytes = checkFile.size();
            checkFile.close();
            if (totalBytes >= _maxLogSize) {
                Serial.println(F("[SD] Log atingiu o limite de 500KB. Rotacionando..."));
                SD.remove("/log_old.txt");
                SD.rename("/log.txt", "/log_old.txt");
            }
        }
    }

    File arquivoLog = SD.open("/log.txt", FILE_APPEND);
    if (arquivoLog) {
        arquivoLog.println(mensagem);
        arquivoLog.close();
    }
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
