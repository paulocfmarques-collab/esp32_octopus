#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include <esp_timer.h>

class LedManager
{
  public:
    LedManager(uint8_t bluePin, uint8_t greenPin);
    void begin();
    void finalizarBoot();
    void atualizar();
    void ligar();
    void desligar();
    void alternar();
    void iniciarBlinkAsync(unsigned long intervalo);
    void piscarSincrono(int piscadas, int tempoMs);

  private:
    uint8_t _bluePin;
    uint8_t _greenPin;
    unsigned long _tempoVerde;
    unsigned long _tempoAzul;
    unsigned long _intervaloBlink;
    bool _estadoVerde;
    bool _estadoAzul;
    bool _blinkAtivo;
    bool _modoVarreduraAtivo = false;
    bool _azulVarredura = false;
    unsigned long _tempoVarredura = 0;
    esp_timer_handle_t _timerBoot = nullptr;
    portMUX_TYPE _bootMux = portMUX_INITIALIZER_UNLOCKED;
    bool _bootAtivo = false;
};

#endif
