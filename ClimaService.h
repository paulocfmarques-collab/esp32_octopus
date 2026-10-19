#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "StorageManager.h"

class ClimaService {
public:
    struct Snapshot {
        float temperatura = 0, umidade = 0;
        bool sincronizado = false, ultimaFalhou = false;
        int64_t atualizadoUs = 0;
    };
    explicit ClimaService(StorageManager& storage) : _storage(storage) {}
    bool iniciar();
    Snapshot snapshot();
    static bool atual(const Snapshot& dados);
    String obterIdade();

private:
    StorageManager& _storage;
    TaskHandle_t _task = nullptr;
    Snapshot _dados;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    static void tarefa(void* contexto);
    void executar();
    bool consultar(Snapshot& dados);
};
