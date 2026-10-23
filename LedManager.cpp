#include "LedManager.h"
#include "NetScan.h"

LedManager::LedManager(uint8_t bluePin, uint8_t greenPin)
    : _bluePin(bluePin), _greenPin(greenPin), _tempoVerde(0), _tempoAzul(0), _intervaloBlink(500),
      _estadoVerde(false), _estadoAzul(false), _blinkAtivo(false)
{
}

void LedManager::begin()
{
    pinMode(_bluePin, OUTPUT);
    pinMode(_greenPin, OUTPUT);
    digitalWrite(_bluePin, LOW);
    digitalWrite(_greenPin, LOW);
    esp_timer_create_args_t timer = {};
    timer.callback = [](void* contexto) {
        auto* leds = static_cast<LedManager*>(contexto);
        portENTER_CRITICAL(&leds->_bootMux);
        if (leds->_bootAtivo) {
            leds->_estadoAzul = !leds->_estadoAzul;
            digitalWrite(leds->_bluePin, leds->_estadoAzul);
        }
        portEXIT_CRITICAL(&leds->_bootMux);
    };
    timer.arg = this;
    timer.dispatch_method = ESP_TIMER_TASK;
    timer.name = "led_boot";
    ESP_ERROR_CHECK(esp_timer_create(&timer, &_timerBoot));
    _bootAtivo = true;
    ESP_ERROR_CHECK(esp_timer_start_periodic(_timerBoot, 500000));
}

void LedManager::finalizarBoot()
{
    ESP_ERROR_CHECK(esp_timer_stop(_timerBoot));
    // Um callback ja despachado nao pode reacender o azul apos a transicao.
    portENTER_CRITICAL(&_bootMux);
    _bootAtivo = false;
    _blinkAtivo = false;
    _estadoAzul = false;
    digitalWrite(_bluePin, LOW);
    portEXIT_CRITICAL(&_bootMux);
    ESP_ERROR_CHECK(esp_timer_delete(_timerBoot));
    _timerBoot = nullptr;
    _estadoVerde = true;
    _tempoVerde = millis();
    digitalWrite(_greenPin, HIGH);
}

void LedManager::atualizar()
{
    unsigned long agora = millis();
    bool varrendo = NetScan::emAndamento();
    if (varrendo != _modoVarreduraAtivo) {
        _modoVarreduraAtivo = varrendo;
        _tempoVarredura = agora;
        _azulVarredura = true;
        digitalWrite(_bluePin, varrendo ? HIGH : _estadoAzul);
    }

    // LED VERDE - Heartbeat ativo apenas se NAO estiver varrendo a rede
    if (!_modoVarreduraAtivo)
    {
        if (agora - _tempoVerde >= 1000)
        {
            _tempoVerde = agora;
            _estadoVerde = !_estadoVerde;
            digitalWrite(_greenPin, _estadoVerde);
        }
    }
    else
    {
        digitalWrite(_greenPin, LOW);
    }

    // LED AZUL - Blink Dinâmico Assíncrono via comandos UDP
    if (_modoVarreduraAtivo) {
        if (agora - _tempoVarredura >= 250) {
            _tempoVarredura = agora;
            _azulVarredura = !_azulVarredura;
            digitalWrite(_bluePin, _azulVarredura);
        }
        return;
    }
    if (_blinkAtivo && (agora - _tempoAzul >= _intervaloBlink))
    {
        _tempoAzul = agora;
        _estadoAzul = !_estadoAzul;
        digitalWrite(_bluePin, _estadoAzul);
    }
}

void LedManager::ligar()
{
    _blinkAtivo = false;
    _estadoAzul = true;
    if (!NetScan::emAndamento()) digitalWrite(_bluePin, HIGH);
}

void LedManager::desligar()
{
    _blinkAtivo = false;
    _estadoAzul = false;
    if (!NetScan::emAndamento()) digitalWrite(_bluePin, LOW);
}

void LedManager::alternar()
{
    _estadoAzul = !_estadoAzul;
    if (!NetScan::emAndamento()) digitalWrite(_bluePin, _estadoAzul);
}

void LedManager::iniciarBlinkAsync(unsigned long intervalo)
{
    _intervaloBlink = intervalo;
    _blinkAtivo = true;
}

void LedManager::piscarSincrono(int piscadas, int tempoMs)
{
    _blinkAtivo = false;
    for (int i = 0; i < piscadas; i++)
    {
        digitalWrite(_bluePin, HIGH);
        delay(tempoMs);
        digitalWrite(_bluePin, LOW);
        delay(tempoMs);
    }
}
