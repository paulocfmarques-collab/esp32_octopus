#pragma once
#include <Arduino.h>

namespace RadioWatchdog {
enum Etapa : uint32_t { INICIO, LOOP, HTTP_PAINEL, HTTP_METRICAS, DNS_PROMETHEUS, UDP_RECEPCAO, UDP_LOG, FILA_CHEIA_LOG };
void registrarBoot();
void iniciar();
void checkpoint();
void cooperar();
const char* etapaAntesDoReset();

class Escopo {
public:
    explicit Escopo(Etapa etapa, bool esperaHttp = false);
    ~Escopo();
private:
    bool _ativo = false, _esperaAnterior = false;
    uint32_t _inicioAnterior = 0;
    Etapa _anterior = INICIO;
};
}
