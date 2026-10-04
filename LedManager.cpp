#include "LedManager.h"

LedManager::LedManager(uint8_t bluePin, uint8_t greenPin)
    : _bluePin(bluePin), _greenPin(greenPin), _tempoVerde(0), _tempoAzul(0),
      _intervaloBlink(500), _estadoVerde(false), _estadoAzul(false), _blinkAtivo(false) {}

void LedManager::begin() {
    pinMode(_bluePin, OUTPUT);
    pinMode(_greenPin, OUTPUT);
    digitalWrite(_bluePin, LOW);
    digitalWrite(_greenPin, LOW);
}

void LedManager::atualizar() {
    unsigned long agora = millis();

    // LED VERDE - Heartbeat ativo apenas se NÃO estiver varrendo a rede
    if (!_modoVarreduraAtivo) {
        if (agora - _tempoVerde >= 1000) {
            _tempoVerde = agora;
            _estadoVerde = !_estadoVerde;
            digitalWrite(_greenPin, _estadoVerde);
        }
    } else {
        // Garante que o verde permaneça desligado de forma segura no fluxo lógico
        digitalWrite(_greenPin, LOW);
    }

    // LED AZUL - Blink Dinâmico Assíncrono via comandos UDP
    if (_blinkAtivo && (agora - _tempoAzul >= _intervaloBlink)) {
        _tempoAzul = agora;
        _estadoAzul = !_estadoAzul;
        digitalWrite(_bluePin, _estadoAzul);
    }
}

void LedManager::ligar() {
    _blinkAtivo = false;
    _estadoAzul = true;
    digitalWrite(_bluePin, HIGH);
}

void LedManager::desligar() {
    _blinkAtivo = false;
    _estadoAzul = false;
    digitalWrite(_bluePin, LOW);
}

void LedManager::alternar() {
    _estadoAzul = !_estadoAzul;
    digitalWrite(_bluePin, _estadoAzul);
}

void LedManager::iniciarBlinkAsync(unsigned long intervalo) {
    _intervaloBlink = intervalo;
    _blinkAtivo = true;
}

void LedManager::piscarSincrono(int piscadas, int tempoMs) {
    _blinkAtivo = false;
    for (int i = 0; i < piscadas; i++) {
        digitalWrite(_bluePin, HIGH); delay(tempoMs);
        digitalWrite(_bluePin, LOW);  delay(tempoMs);
    }
}
