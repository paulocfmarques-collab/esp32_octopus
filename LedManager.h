#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>

class LedManager {
private:
    uint8_t _bluePin;
    uint8_t _greenPin;
    
    unsigned long _tempoVerde;
    unsigned long _tempoAzul;
    unsigned long _intervaloBlink;
    
    bool _estadoVerde;
    bool _estadoAzul;
    bool _blinkAtivo;

public:
    LedManager(uint8_t bluePin, uint8_t greenPin);
    void begin();
    void atualizar();
    
    void ligar();
    void desligar();
    void alternar();
    void iniciarBlinkAsync(unsigned long intervalo);
    void piscarSincrono(int piscadas, int tempoMs);
};

#endif
