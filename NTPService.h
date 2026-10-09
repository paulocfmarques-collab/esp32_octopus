#ifndef NTP_SERVICE_H
#define NTP_SERVICE_H

#include <Arduino.h>
#include <time.h>
#include <Preferences.h>

class NTPService {
public:
    bool inicializar();
    void configurarRelogio(int fuso, bool dstAtivo);
    void carregarEConfigurarHorario();
    void atualizarFuso(int novoFuso);
    void atualizarDST(bool novoDstAtivo);
    String obterApenasHora();
    String obterApenasData();
};

#endif
