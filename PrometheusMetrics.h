#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <atomic>
#include <math.h>
#include <freertos/task.h>
#include "StorageManager.h"

// Servidor pull independente do painel HTTP na porta 80.
class PrometheusMetrics {
public:
    explicit PrometheusMetrics(StorageManager& storage);
    void inicializar();
    void iniciarServidor();
    void processar();
    void salvarConfiguracao(WebServer& painel);
    void registrarRequisicao(bool erro, uint32_t duracaoUs);
    void registrarComandoUDP(bool erro, uint32_t duracaoUs);
    void registrarErroUDP();
    bool resetCounters();
    bool ativo() const { return _ativo; }
    uint16_t porta() const { return _porta; }
    const String& servidor() const { return _servidor; }
    uint32_t tamanhoFirmware() const { return _imageBytes; }
    String status() const;
    String gerarMetricas();

private:
    StorageManager& _storage;
    WebServer _server;
    bool _ativo = false;
    bool _iniciado = false;
    bool _reconfigurar = false;
    uint16_t _porta = 9120;
    String _device;
    uint32_t _imageBytes = 0, _partitionBytes = 0;
    String _servidor;
    IPAddress _servidorIP;
    bool _resolvido = false;
    uint32_t _ultimaResolucao = 0;
    uint32_t _bootCount = 0;
    bool _contadoresValidos = false;
    uint32_t _watchdogCount = 0;
    std::atomic<uint32_t> _desconexoes{0};
    std::atomic<uint32_t> _requisicoes{0};
    std::atomic<uint32_t> _erros{0};
    std::atomic<uint32_t> _ultimaDuracaoUs{0};
    std::atomic<uint32_t> _scrapes{0};
    std::atomic<uint32_t> _comandosUDP{0}, _errosUDP{0}, _duracaoUDPUs{0};
    float _cpuPercent = NAN;
#if configGENERATE_RUN_TIME_STATS && configUSE_TRACE_FACILITY
    TaskStatus_t _tarefas[64];
    configRUN_TIME_COUNTER_TYPE _ultimoIdle[portNUM_PROCESSORS]{};
    int64_t _ultimoTempoCPU = 0;
    uint32_t _ultimaCPU = 0;
    bool _baselineCPU = false;
#endif
    void resolverServidor();
    void atualizarCPU();
    void handleMetrics();
};

// Mede a requisicao completa, inclusive os caminhos de erro e retornos antecipados.
class PrometheusRequest {
public:
    explicit PrometheusRequest(PrometheusMetrics& metrics, bool udp = false) : _metrics(metrics), _inicio(micros()), _udp(udp) {}
    ~PrometheusRequest() {
        if (_udp) _metrics.registrarComandoUDP(erro, micros() - _inicio);
        else _metrics.registrarRequisicao(erro, micros() - _inicio);
    }
    bool erro = false;
private:
    PrometheusMetrics& _metrics;
    uint32_t _inicio;
    bool _udp;
};
