#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class StorageManager {
public:
    struct MetricasSD {
        bool ativo = false, valido = false, logValido = false, udpLogValido = false;
        uint64_t capacidade = 0, total = 0, usado = 0;
        uint32_t logBytes = 0, udpLogBytes = 0;
        uint32_t gravacoes = 0, erros = 0, errosAppend = 0, rotacoes = 0, atualizadoEm = 0;
        int64_t atualizadoUs = 0;
    };
    StorageManager(uint8_t csPin, SPIClass& spiInstance);
    bool inicializar();
    void gravarLog(const String& mensagem);
    void gravarLogUDP(const String& mensagem);
    void lerParaSerial(const char* filename);
    bool deletarArquivo(const char* filename);
    bool arquivoExiste(const char* filename);
    File abrirArquivo(const char* filename, const char* mode);
    bool isActive() { return _sdcardOk; } // Alias para manter retrocompatibilidade
    bool isAtivo();
    MetricasSD metricas();
    void atualizarMetricas(bool forcar = false);

private:
    SemaphoreHandle_t _logMutex = nullptr;
    MetricasSD _metricas;
    MetricasSD _snapshot;
    portMUX_TYPE _metricasMux = portMUX_INITIALIZER_UNLOCKED;
    void publicarMetricas();
    void gravarRegistro(const char* caminho, const char* anterior, const String& mensagem);
    uint8_t _csPin;
    SPIClass& _spiSD;
    bool _sdcardOk;
    const size_t _maxLogSize = 512 * 1024; // Limite de 500KB para rotação
};

#endif
