#include "ClimaService.h"
#include "WeatherHttp.h"
#include <WiFi.h>
#include <esp_timer.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

namespace {
// Le somente numeros do objeto current, sem confundir com current_units.
bool numero(const String& objeto, const char* chave, double& valor) {
    int pos = objeto.indexOf(String("\"") + chave + "\"");
    if (pos < 0) return false;
    pos = objeto.indexOf(':', pos + strlen(chave) + 2);
    if (pos < 0) return false;
    int fim = pos + 1;
    while (fim < int(objeto.length()) && objeto[fim] != ',' && objeto[fim] != '}') ++fim;
    String token = objeto.substring(pos + 1, fim);
    token.trim();
    if (!token.length() || !(token[0] == '-' || (token[0] >= '0' && token[0] <= '9'))) return false;
    char* resto = nullptr;
    valor = strtod(token.c_str(), &resto);
    return resto != token.c_str() && !*resto && isfinite(valor);
}
}

bool ClimaService::iniciar() {
    if (_task) return true;
    // HTTP e DNS ficam fora das tarefas monitoradas de radio, OTA e comandos.
    bool ok = xTaskCreatePinnedToCore(tarefa, "ClimaPortoAlegre", 4096, this, 1, &_task, 1) == pdPASS;
    if (!ok) { _task = nullptr; Serial.println(F("[CLIMA] Falha ao criar tarefa de consulta.")); }
    return ok;
}

ClimaService::Snapshot ClimaService::snapshot() {
    portENTER_CRITICAL(&_mux);
    Snapshot copia = _dados;
    portEXIT_CRITICAL(&_mux);
    return copia;
}

bool ClimaService::atual(const Snapshot& dados) {
    // Mantem o ultimo sucesso por ate 30 min em falhas transitorias.
    return dados.sincronizado && esp_timer_get_time() - dados.atualizadoUs <= 1800LL * 1000000;
}

String ClimaService::obterIdade() {
    Snapshot dados = snapshot();
    if (!dados.sincronizado) return "Clima de Porto Alegre ainda nao sincronizado.\n";
    String resposta = "Idade dos dados de Porto Alegre: " +
        String((esp_timer_get_time() - dados.atualizadoUs) / 1000000.0, 1) + " s\n";
    resposta += String("Dados: ") + (atual(dados) ? "VALIDOS" : "DESATUALIZADOS (mais de 30 min)") + "\n";
    if (dados.ultimaFalhou) resposta += "Ultima tentativa de atualizacao falhou; idade desde o ultimo sucesso.\n";
    return resposta;
}

void ClimaService::tarefa(void* contexto) {
    static_cast<ClimaService*>(contexto)->executar();
}

void ClimaService::executar() {
    bool tentou = false;
    uint32_t tentativaEm = 0;
    for (;;) {
        Snapshot dados = snapshot();
        uint32_t intervalo = dados.sincronizado && !dados.ultimaFalhou ? 900000 : 60000;
        if (WiFi.status() == WL_CONNECTED && (!tentou || millis() - tentativaEm >= intervalo)) {
            tentou = true;
            tentativaEm = millis();
            bool ok = consultar(dados);
            dados.ultimaFalhou = !ok;
            portENTER_CRITICAL(&_mux);
            _dados = dados;
            portEXIT_CRITICAL(&_mux);
            const char* evento = ok ? "[CLIMA] Porto Alegre atualizado via Open-Meteo."
                                    : "[CLIMA] Falha ao atualizar Porto Alegre; preservando ultimo sucesso.";
            Serial.println(evento);
            _storage.gravarLog(evento);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool ClimaService::consultar(Snapshot& dados) {
    WiFiClient client;
    String payload;
    bool ok = WeatherHttp::consultar(client, payload);
    client.stop();
    if (!ok) return false;
    int inicio = payload.indexOf("\"current\"");
    if (inicio < 0) return false;
    inicio = payload.indexOf('{', inicio + 9);
    int fim = inicio >= 0 ? payload.indexOf('}', inicio + 1) : -1;
    if (fim < 0) return false;
    String objeto = payload.substring(inicio, fim + 1);
    double temperatura, umidade, instante;
    if (!numero(objeto, "temperature_2m", temperatura) || temperatura < -100 || temperatura > 70 ||
        !numero(objeto, "relative_humidity_2m", umidade) || umidade < 0 || umidade > 100 ||
        !numero(objeto, "time", instante) || instante < 1577836800) return false;
    time_t agora = time(nullptr);
    // Quando o NTP esta valido, rejeita resposta muito antiga ou futura.
    if (agora >= 1577836800 && (instante > double(agora) + 300 || double(agora) - instante > 3600)) return false;
    dados.temperatura = temperatura;
    dados.umidade = umidade;
    dados.sincronizado = true;
    dados.atualizadoUs = esp_timer_get_time();
    return true;
}
