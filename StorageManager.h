#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <SPI.h>
#include <SD.h>

class StorageManager {
private:
    uint8_t _csPin;
    SPIClass& _spiSD;
    const size_t _maxLogSize = 512000; // Limite de 500 KB por arquivo
    bool _sdcardOk;

public:
    StorageManager(uint8_t csPin, SPIClass& spiInstance);
    bool inicializar();
    void gravarLog(const String& mensagem);
    void lerParaSerial(const char* filename);
    bool deletarArquivo(const char* filename);
    bool arquivoExiste(const char* filename);
    File abrirArquivo(const char* filename, const char* mode);
    bool isAtivo();
};

#endif
