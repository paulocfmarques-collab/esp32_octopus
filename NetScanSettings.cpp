#include "NetScan.h"
#include "PgClient.h"
#include <Preferences.h>
#include <atomic>

namespace {
NetScan::Ajustes ajustes;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t salvarMutex = nullptr;
StorageManager* armazenamento = nullptr;
std::atomic<uint32_t> ultimaTentativa{0};
std::atomic<bool> testando{false};
char sqlEstado[320] = "PostgreSQL: ainda nao enviado.";
bool numero(const String& texto, uint32_t min, uint32_t max, uint32_t& valor) {
    if (texto.isEmpty() || texto.length() > 6) return false;
    valor = 0;
    for (size_t i = 0; i < texto.length(); ++i) {
        if (texto[i] < '0' || texto[i] > '9') return false;
        valor = valor * 10 + texto[i] - '0';
    }
    return valor >= min && valor <= max;
}
bool valido(const NetScan::Ajustes& a) {
    if (a.assinatura != 0x4e534331 || !a.porta || a.intervaloMin < 1 || a.intervaloMin > 1440 ||
        !memchr(a.servidor, 0, sizeof(a.servidor)) || !memchr(a.banco, 0, sizeof(a.banco)) ||
        !memchr(a.usuario, 0, sizeof(a.usuario)) || !memchr(a.senha, 0, sizeof(a.senha))) return false;
    for (size_t i = 0; a.servidor[i]; ++i) {
        char c = a.servidor[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    }
    if (!a.servidor[0] || strcmp(a.banco, "LogsRede") != 0) return false;
    for (const char* texto : {a.banco, a.usuario, a.senha})
        for (size_t i = 0; texto[i]; ++i) if (uint8_t(texto[i]) < 32 || uint8_t(texto[i]) > 126) return false;
    return !a.sqlAtivo || (a.banco[0] && a.usuario[0]);
}
// Chamador segura salvarMutex; dados publicados somente apos verificar a gravacao.
bool persistir(const NetScan::Ajustes& novo) {
    if (!valido(novo)) return false;
    Preferences prefs;
    if (!prefs.begin("netscan", false)) return false;
    NetScan::Ajustes leitura;
    bool ok = prefs.putBytes("config", &novo, sizeof(novo)) == sizeof(novo) &&
        prefs.getBytes("config", &leitura, sizeof(leitura)) == sizeof(leitura) && memcmp(&novo, &leitura, sizeof(novo)) == 0;
    prefs.end();
    if (ok) {
        portENTER_CRITICAL(&mux);
        ajustes = novo;
        strlcpy(sqlEstado, "Configuracao salva nas prefs; conexao PostgreSQL ainda nao testada.", sizeof(sqlEstado));
        portEXIT_CRITICAL(&mux);
        ultimaTentativa.store(millis());
    }
    return ok;
}
void agenda(void*) {
    for (;;) {
        auto a = NetScan::configuracao();
        if (a.automatico && millis() - ultimaTentativa.load() >= uint32_t(a.intervaloMin) * 60000 &&
            !NetScan::emAndamento() && !testando.load()) {
            ultimaTentativa.store(millis());
            String resultado = NetScan::iniciar(*armazenamento);
            if (resultado.startsWith("Erro:")) {
                Serial.println("[NETSCAN] Agenda: " + resultado);
                armazenamento->gravarLog("[NETSCAN] Agenda: " + resultado);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void testarConexao(void*) {
    const auto a = NetScan::configuracao();
    PgClient pg;
    if (!pg.conectar(a.servidor, a.porta, a.banco, a.usuario, a.senha))
        NetScan::registrarSQL(false, "Teste de login: " + pg.erro(), a);
    else if (!pg.executar("SELECT 1;"))
        NetScan::registrarSQL(false, "Login realizado, mas consulta de teste falhou: " + pg.erro(), a);
    else if (!pg.executar("SELECT mac,ip,response_time,ping_replied,discovery,data_leitura FROM public.netscan LIMIT 0;"))
        NetScan::registrarSQL(false, "Login confirmado; verificacao da tabela public.netscan falhou: " + pg.erro(), a);
    else NetScan::registrarSQL(true, "Login confirmado com as credenciais salvas. Tabela fixa public.netscan acessivel para leitura. Escrita sera validada no envio do netscan.", a);
    testando.store(false);
    vTaskDelete(nullptr);
}
}

void NetScan::inicializar(StorageManager& storage) {
    armazenamento = &storage;
    salvarMutex = xSemaphoreCreateMutex();
    Preferences prefs;
    if (prefs.begin("netscan", true)) {
        Ajustes salvo;
        if (prefs.getBytesLength("config") == sizeof(salvo) && prefs.getBytes("config", &salvo, sizeof(salvo)) == sizeof(salvo)) {
            // Mantem credenciais existentes ao migrar o nome do banco para o fixo.
            strlcpy(salvo.banco, "LogsRede", sizeof(salvo.banco));
            if (valido(salvo)) ajustes = salvo;
        }
        prefs.end();
    }
    ultimaTentativa.store(millis());
    if (!salvarMutex || xTaskCreatePinnedToCore(agenda, "NetScanAgenda", 4096, nullptr, 1, nullptr, 1) != pdPASS) {
        storage.gravarLog("[NETSCAN] Falha ao iniciar agenda automatica: memoria insuficiente.");
        Serial.println("[NETSCAN] Falha ao iniciar agenda automatica.");
    }
}
NetScan::Ajustes NetScan::configuracao() {
    portENTER_CRITICAL(&mux); Ajustes copia = ajustes; portEXIT_CRITICAL(&mux);
    return copia;
}
String NetScan::configurarIntervalo(const String& valor) {
    uint32_t minutos;
    if (!numero(valor, 1, 1440, minutos)) return "Erro: use netscan_interval:1 a 1440 (minutos).\n";
    if (!salvarMutex || xSemaphoreTake(salvarMutex, pdMS_TO_TICKS(100)) != pdTRUE) return "Erro: configuracao ocupada.\n";
    auto a = configuracao(); a.intervaloMin = minutos;
    bool ok = persistir(a); xSemaphoreGive(salvarMutex);
    return ok ? "Intervalo netscan salvo: " + String(minutos) + " minutos.\n" : "Erro: falha ao salvar intervalo.\n";
}
String NetScan::configurarAutomatico(const String& valor) {
    if (valor != "on" && valor != "off") return "Erro: use netscan_auto:on ou netscan_auto:off.\n";
    if (!salvarMutex || xSemaphoreTake(salvarMutex, pdMS_TO_TICKS(100)) != pdTRUE) return "Erro: configuracao ocupada.\n";
    auto a = configuracao(); a.automatico = valor == "on";
    bool ok = persistir(a); xSemaphoreGive(salvarMutex);
    return ok ? String("Netscan automatico ") + (a.automatico ? "ativado.\n" : "desativado.\n") : "Erro: falha ao salvar agenda.\n";
}
void NetScan::salvarWeb(WebServer& s) {
    if (testando.load()) { s.send(409, "text/plain", "Aguarde o teste PostgreSQL terminar antes de alterar as credenciais."); return; }
    if (!salvarMutex || xSemaphoreTake(salvarMutex, pdMS_TO_TICKS(100)) != pdTRUE) { s.send(503, "text/plain", "Configuracao ocupada."); return; }
    if (testando.load()) { xSemaphoreGive(salvarMutex); s.send(409, "text/plain", "Aguarde o teste PostgreSQL terminar antes de alterar as credenciais."); return; }
    auto a = configuracao(); uint32_t porta, intervalo;
    String host = s.arg("ns_host"), usuario = s.arg("ns_user"), senha = s.arg("ns_password");
    host.trim(); usuario.trim();
    bool ok = numero(s.arg("ns_port"), 1, 65535, porta) && numero(s.arg("ns_interval"), 1, 1440, intervalo) &&
        host.length() < sizeof(a.servidor) && usuario.length() < sizeof(a.usuario) && senha.length() < sizeof(a.senha);
    if (ok) {
        a.porta = porta; a.intervaloMin = intervalo; a.automatico = s.hasArg("ns_auto"); a.sqlAtivo = s.hasArg("ns_sql");
        strlcpy(a.servidor, host.c_str(), sizeof(a.servidor)); strlcpy(a.banco, "LogsRede", sizeof(a.banco)); strlcpy(a.usuario, usuario.c_str(), sizeof(a.usuario));
        if (!senha.isEmpty()) strlcpy(a.senha, senha.c_str(), sizeof(a.senha));
        if (s.hasArg("ns_clear_password")) a.senha[0] = 0;
        ok = valido(a);
    }
    int codigo = ok ? 200 : 400;
    if (ok && !persistir(a)) { ok = false; codigo = 500; }
    xSemaphoreGive(salvarMutex);
    String mensagem;
    if (ok) {
        mensagem = "Configuracao salva e verificada nas prefs. Usuario: " + String(a.usuario) + ". ";
        mensagem += s.hasArg("ns_clear_password") ? "Senha removida." : (!senha.isEmpty() ? "Senha alterada e gravada." : "Senha anterior mantida.");
        mensagem += " Use SALVAR E TESTAR LOGIN para confirmar o acesso ao PostgreSQL. Tabela fixa: public.netscan.";
    } else mensagem = "Falha: confira parametros, intervalo (1 a 1440), porta e usuario. Banco fixo: LogsRede. Use caracteres ASCII.";
    s.send(codigo, "text/plain; charset=utf-8", mensagem);
}
String NetScan::testarPostgres() {
    if (!salvarMutex || xSemaphoreTake(salvarMutex, pdMS_TO_TICKS(100)) != pdTRUE) return "Erro: configuracao ocupada.";
    String resposta;
    bool esperado = false;
    auto a = configuracao();
    if (NetScan::emAndamento()) resposta = "Erro: aguarde o netscan terminar para testar o login.";
    else if (!a.usuario[0]) resposta = "Erro: informe e salve o usuario. Banco fixo: LogsRede; tabela fixa: netscan.";
    else if (!testando.compare_exchange_strong(esperado, true)) resposta = "Erro: teste PostgreSQL ja em andamento.";
    else {
        if (NetScan::emAndamento()) {
            testando.store(false);
            xSemaphoreGive(salvarMutex);
            return "Erro: netscan iniciado; aguarde terminar para testar o login.";
        }
        portENTER_CRITICAL(&mux);
        strlcpy(sqlEstado, "Testando conexao e login PostgreSQL com as credenciais salvas...", sizeof(sqlEstado));
        portEXIT_CRITICAL(&mux);
        if (xTaskCreatePinnedToCore(testarConexao, "TestePostgres", 8192, nullptr, 1, nullptr, 1) != pdPASS) {
            testando.store(false);
            registrarSQL(false, "Memoria insuficiente para iniciar teste de conexao.", a);
            resposta = "Erro: memoria insuficiente para testar PostgreSQL.";
        } else resposta = "Teste PostgreSQL iniciado. Aguarde o resultado no estado do banco abaixo.";
    }
    xSemaphoreGive(salvarMutex);
    return resposta;
}
bool NetScan::testePostgresAtivo() { return testando.load(); }
void NetScan::registrarSQL(bool ok, const String& detalhe, const Ajustes& config) {
    String texto = String(ok ? "[NETSCAN SQL] OK " : "[NETSCAN SQL] ERRO ") + config.servidor + ":" + String(config.porta) + " banco=" + config.banco + " " + detalhe;
    if (config.senha[0]) texto.replace(config.senha, "***");
    texto.replace("\r", " "); texto.replace("\n", " ");
    Serial.println(texto);
    if (armazenamento) armazenamento->gravarLog(texto);
    portENTER_CRITICAL(&mux); strlcpy(sqlEstado, texto.c_str(), sizeof(sqlEstado)); portEXIT_CRITICAL(&mux);
}
String NetScan::estadoSQL() {
    char copia[sizeof(sqlEstado)];
    portENTER_CRITICAL(&mux); memcpy(copia, sqlEstado, sizeof(copia)); portEXIT_CRITICAL(&mux);
    return String(copia);
}
