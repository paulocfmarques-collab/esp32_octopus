#pragma once
#include <Arduino.h>
#include <WiFiClient.h>

// Cliente PostgreSQL v3 para a rede local. SCRAM-SHA-256 e MD5; sem TLS.
class PgClient {
public:
    bool conectar(const char* host, uint16_t porta, const char* banco, const char* usuario, const char* senha);
    bool executar(const String& sql);
    const String& erro() const { return _erro; }
    ~PgClient() { _tcp.stop(); }
private:
    WiFiClient _tcp;
    uint8_t _dados[2048];
    size_t _tamanho = 0;
    uint32_t _inicio = 0;
    String _erro;
    bool receber(void* buf, size_t n);
    bool enviar(const void* buf, size_t n);
    bool mensagem(char& tipo);
    bool pacote(char tipo, const void* dados, size_t n);
    bool falha(const String& erro);
};
