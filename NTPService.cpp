#include "NTPService.h"

bool NTPService::inicializar() {
    Serial.println(F("[NTP] Inicializando sincronizacao de tempo..."));
    carregarEConfigurarHorario();

    for (int tentativa = 0; tentativa < 15; tentativa++) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 1000)) {
            Serial.println(F("[NTP] Sincronizacao concluida com sucesso."));
            return true;
        }
        Serial.print(".");
        delay(500);
    }
    Serial.println(F("\n[NTP] Erro de sincronismo. Operando offline."));
    return false;
}

void NTPService::configurarRelogio(int fuso, bool dstAtivo) {
    char tzString[64];
    int fusoInvertido = -fuso; // Inversão POSIX para o ESP32

    if (dstAtivo) {
        snprintf(tzString, sizeof(tzString), "GMT%dGMT%d,M10.3.0/0,M2.3.0/0", fusoInvertido, fusoInvertido - 1);
    } else {
        snprintf(tzString, sizeof(tzString), "GMT%d", fusoInvertido);
    }
    
    configTzTime(tzString, "a.st1.ntp.br", "pool.ntp.org", "time.nist.gov");
}

void NTPService::carregarEConfigurarHorario() {
    Preferences prefs;
    prefs.begin("relogio", true);
    int fuso = prefs.getInt("gmt", -3);
    bool dst = prefs.getBool("dst", false);
    prefs.end();

    configurarRelogio(fuso, dst);
}

void NTPService::atualizarFuso(int novoFuso) {
    Preferences prefs;
    prefs.begin("relogio", false);
    prefs.putInt("gmt", novoFuso);
    prefs.end();
    carregarEConfigurarHorario();
}

void NTPService::atualizarDST(bool novoDstAtivo) {
    Preferences prefs;
    prefs.begin("relogio", false);
    prefs.putBool("dst", novoDstAtivo);
    prefs.end();
    carregarEConfigurarHorario();
}

String NTPService::obterApenasHora() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 50)) return "00:00:00";
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return String(buffer);
}

String NTPService::obterApenasData() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 50)) return "00/00/0000";
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    return String(buffer);
}
