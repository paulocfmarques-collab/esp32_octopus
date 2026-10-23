#include "RadioWatchdog.h"
#include <WebServer.h>
#include <esp_task_wdt.h>
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// build_opt.h aplica os limites tambem a Parsing.cpp da biblioteca WebServer.
static_assert(WEBSERVER_MAX_LINE_WAIT > 0 && WEBSERVER_MAX_LINE_WAIT <= 750, "Mantenha build_opt.h junto do sketch");
static_assert(WEBSERVER_MAX_HEADER_WAIT > 0 && WEBSERVER_MAX_HEADER_WAIT <= 1500, "Limite HTTP excede o orcamento do radio");

namespace {
constexpr uint32_t ASSINATURA = 0x52414431;
RTC_NOINIT_ATTR volatile uint32_t assinaturaRTC;
RTC_NOINIT_ATTR volatile uint32_t etapaRTC;
RadioWatchdog::Etapa etapaBoot = RadioWatchdog::INICIO;
TaskHandle_t dono = nullptr;
esp_task_wdt_user_handle_t watchdog = nullptr;
bool esperaHttp = false;
uint32_t inicioHttp = 0;
bool naTarefa() { return dono && xTaskGetCurrentTaskHandle() == dono; }
}

void RadioWatchdog::registrarBoot() {
    if (assinaturaRTC == ASSINATURA && etapaRTC <= FILA_CHEIA_LOG) etapaBoot = static_cast<Etapa>(etapaRTC);
    assinaturaRTC = ASSINATURA;
    etapaRTC = INICIO;
}

const char* RadioWatchdog::etapaAntesDoReset() {
    switch (etapaBoot) {
        case LOOP: return "laco principal";
        case HTTP_PAINEL: return "HTTP painel (porta 80)";
        case HTTP_METRICAS: return "HTTP metricas Prometheus";
        case DNS_PROMETHEUS: return "DNS do servidor Prometheus";
        case UDP_RECEPCAO: return "mutex/leitura UDP";
        case UDP_LOG: return "gravacao do log RX no SD";
        case FILA_CHEIA_LOG: return "gravacao de fila cheia no SD";
        default: return "indisponivel (boot anterior sem marcador)";
    }
}

void RadioWatchdog::iniciar() {
    dono = xTaskGetCurrentTaskHandle();
    ESP_ERROR_CHECK(esp_task_wdt_add_user("WdtRadioCore0", &watchdog));
    etapaRTC = LOOP;
    checkpoint();
}

void RadioWatchdog::checkpoint() {
    if (naTarefa() && watchdog) ESP_ERROR_CHECK(esp_task_wdt_reset_user(watchdog));
}

void RadioWatchdog::cooperar() {
    // Somente a propria tarefa, dentro de HTTP, por no maximo 2 s.
    // Nao alimenta SD/DNS/mutex presos nem uma espera HTTP indefinida.
    if (naTarefa() && esperaHttp && millis() - inicioHttp < 2000) checkpoint();
}

RadioWatchdog::Escopo::Escopo(Etapa etapa, bool http) {
    if (!naTarefa()) return;
    _ativo = true;
    _anterior = static_cast<Etapa>(etapaRTC);
    _esperaAnterior = esperaHttp;
    _inicioAnterior = inicioHttp;
    etapaRTC = etapa;
    esperaHttp = http;
    inicioHttp = millis();
    checkpoint();
}

RadioWatchdog::Escopo::~Escopo() {
    if (!_ativo) return;
    etapaRTC = _anterior;
    esperaHttp = _esperaAnterior;
    inicioHttp = _inicioAnterior;
    checkpoint();
}
