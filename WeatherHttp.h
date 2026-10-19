#pragma once
#include <WiFiClient.h>

// Consulta unica, sem cookies: evita trazer strptime e o parser de cookies
// do HTTPClient para a IRAM.
class WeatherHttp {
public:
    static bool consultar(WiFiClient& client, String& corpo) {
        // Dados meteorologicos publicos, sem credenciais. HTTP evita os
        // buffers TLS de 16 KB cada neste ESP32 sem PSRAM.
        if (!client.connect("api.open-meteo.com", 80, 3000)) {
            Serial.println(F("[CLIMA] Falha na conexao com api.open-meteo.com."));
            return false;
        }
        const unsigned long inicio = millis();
        unsigned long ultimoDado = inicio;
        bool falhou = false;
        auto byte = [&]() -> int {
            for (;;) {
                if (millis() - inicio >= 15000) { falhou = true; return -1; }
                if (client.available()) { ultimoDado = millis(); return client.read(); }
                if (!client.connected()) return -1;
                if (millis() - ultimoDado >= 3000) { falhou = true; return -1; }
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        };
        auto linha = [&](String& out) -> bool {
            out = "";
            while (out.length() < 1024) {
                int c = byte();
                if (c < 0) return false;
                if (c == '\n') { if (out.endsWith("\r")) out.remove(out.length() - 1); return true; }
                out += char(c);
            }
            return false;
        };
        client.print("GET /v1/forecast?latitude=-30.03&longitude=-51.23&current=temperature_2m,relative_humidity_2m&temperature_unit=celsius&timeformat=unixtime&forecast_days=1 HTTP/1.0\r\nHost: api.open-meteo.com\r\nAccept: application/json\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
        String cabecalho;
        if (!linha(cabecalho) || !(cabecalho.startsWith("HTTP/1.0 200 ") || cabecalho.startsWith("HTTP/1.1 200 "))) {
            Serial.println("[CLIMA] Resposta HTTP invalida: " + cabecalho);
            return false;
        }
        bool chunks = false;
        int comprimento = -1;
        size_t totalCabecalho = 0;
        for (;;) {
            if (!linha(cabecalho)) return false;
            totalCabecalho += cabecalho.length() + 2;
            if (totalCabecalho > 8192) return false;
            if (!cabecalho.length()) break;
            cabecalho.toLowerCase();
            if (cabecalho.startsWith("content-length:")) {
                String valor = cabecalho.substring(15); valor.trim();
                if (!valor.length()) return false;
                comprimento = 0;
                for (size_t i = 0; i < valor.length(); ++i) {
                    if (valor[i] < '0' || valor[i] > '9') return false;
                    comprimento = comprimento * 10 + valor[i] - '0';
                    if (comprimento > 16384) return false;
                }
            }
            if (cabecalho.startsWith("transfer-encoding:")) {
                String valor = cabecalho.substring(18); valor.trim();
                if (valor != "chunked") return false;
                chunks = true;
            }
            if (cabecalho.startsWith("content-encoding:")) {
                String valor = cabecalho.substring(17); valor.trim();
                if (valor != "identity") return false;
            }
        }
        corpo = ""; corpo.reserve(comprimento > 0 ? comprimento : 2048);
        auto ler = [&](size_t quantidade) -> bool {
            if (quantidade > 16384 - corpo.length()) return false;
            while (quantidade--) { int c = byte(); if (c < 0) return false; corpo += char(c); }
            return true;
        };
        if (chunks) {
            for (;;) {
                if (!linha(cabecalho)) return false;
                int fim = cabecalho.indexOf(';');
                if (fim >= 0) cabecalho.remove(fim);
                cabecalho.trim();
                if (!cabecalho.length()) return false;
                size_t quantidade = 0;
                for (size_t i = 0; i < cabecalho.length(); ++i) {
                    char c = cabecalho[i];
                    int n = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                    if (n < 0 || quantidade > 1024) return false;
                    quantidade = quantidade * 16 + n;
                    if (quantidade > 16384) return false;
                }
                if (!quantidade) {
                    do {
                        if (!linha(cabecalho)) return false;
                        totalCabecalho += cabecalho.length() + 2;
                        if (totalCabecalho > 8192) return false;
                    } while (cabecalho.length());
                    return !falhou && corpo.length();
                }
                if (!ler(quantidade) || byte() != '\r' || byte() != '\n') return false;
            }
        }
        if (comprimento >= 0) return ler(comprimento) && !falhou && corpo.length();
        for (;;) {
            int c = byte();
            if (c < 0) return !falhou && corpo.length();
            if (corpo.length() >= 16384) return false;
            corpo += char(c);
        }
    }
};
