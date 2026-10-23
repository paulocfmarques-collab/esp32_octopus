#pragma once
#include <Arduino.h>
#include "StorageManager.h"
#include <WebServer.h>

namespace NetScan {
struct Ajustes {
    uint32_t assinatura = 0x4e534331;
    bool automatico = true;
    bool sqlAtivo = false;
    uint16_t intervaloMin = 15;
    uint16_t porta = 5432;
    char servidor[64] = "192.168.0.20";
    char banco[64] = "LogsRede";
    char usuario[64] = "";
    char senha[128] = "";
};
void inicializar(StorageManager& storage);
Ajustes configuracao();
void salvarWeb(WebServer& server);
String configurarIntervalo(const String& valor);
String configurarAutomatico(const String& valor);
String estadoSQL();
String testarPostgres();
bool testePostgresAtivo();
void registrarSQL(bool ok, const String& detalhe, const Ajustes& config);
// Retorna imediatamente; a tarefa de baixa prioridade faz a descoberta.
String iniciar(StorageManager& storage);
String estado();
bool emAndamento();
}
