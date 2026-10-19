#ifndef DEFINES_H
#define DEFINES_H

#include <Arduino.h>

namespace Config {
    constexpr const char* FIRMWARE_VERSION = "v1.3.0";
    // Pinos do Cartão SD (VSPI Original)
    constexpr uint8_t PIN_SD_CS   = 13;
    constexpr uint8_t PIN_SD_MOSI = 23;
    constexpr uint8_t PIN_SD_MISO = 19;
    constexpr uint8_t PIN_SD_SCK  = 18;
    constexpr uint32_t SD_SPI_FREQUENCY_HZ = 4000000; // Mesmo padrao do SD.begin do Arduino ESP32.

    // Pinos dos LEDs e Botão
    constexpr uint8_t PIN_LED_BLUE = 2;
    constexpr uint8_t PIN_LED_GRN  = 15;
    constexpr uint8_t PIN_BTN_RST  = 0;

    // Parâmetros de Configuração de Rede e Tempo
    constexpr uint16_t UDP_PORTA_PADRAO = 4210;
    constexpr int FUSO_PADRAO = -3;
    
    // Configuração do RTOS
    constexpr uint8_t TAMANHO_FILA_UDP = 10; // Suporta até 10 comandos enfileirados na RAM

    constexpr float DivMb = (float)(1024*1024);
    constexpr float DivKb = (float)(1024);
}

#endif
