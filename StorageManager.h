#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

class StorageManager {
public:
    StorageManager(uint8_t csPin, SPIClass& spiInstance);
    bool inicializar();
    void gravarLog(const String& mensagem);
    void lerParaSerial(const char* filename);
    bool deletarArquivo(const char* filename);
    bool arquivoExiste(const char* filename);
    File abrirArquivo(const char* filename, const char* mode);
    bool isActive() { return _sdcardOk; } // Alias para manter retrocompatibilidade
    bool isAtivo();

private:
    uint8_t _csPin;
    SPIClass& _spiSD;
    bool _sdcardOk;
    const size_t _maxLogSize = 512 * 1024; // Limite de 500KB para rotação
};

#endif
