#include "NetScan.h"
#include "PgClient.h"
#include <WiFi.h>
#include <atomic>
#include <new>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <ping/ping_sock.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>
#include <freertos/semphr.h>

namespace {
constexpr uint32_t MAX_HOSTS = 1022;
struct Registro { uint32_t ip; uint8_t mac[6]; bool ping; uint32_t ms; };
struct ArpConsulta {
    SemaphoreHandle_t pronto;
    ip4_addr_t ip;
    uint8_t mac[6];
    bool encontrado = false, solicitar = false;
};
struct PingConsulta { SemaphoreHandle_t pronto; bool respondeu = false; uint32_t ms = 0; };
StorageManager* sd = nullptr;
std::atomic<bool> ocupado{false};
std::atomic<uint32_t> verificados{0}, encontrados{0};
// 0: nunca executado; 1: salvo; 2: falha (arquivo anterior preservado).
std::atomic<int> resultado{0};
uint32_t inicioRede, fimRede, proprioIP;
uint32_t numero(IPAddress ip) {
    return (uint32_t(ip[0]) << 24) | (uint32_t(ip[1]) << 16) | (uint32_t(ip[2]) << 8) | ip[3];
}
IPAddress endereco(uint32_t ip) { return IPAddress(ip >> 24, ip >> 16, ip >> 8, ip); }

// A tabela ARP so pode ser acessada na tarefa TCP/IP do lwIP.
void consultarArp(void* arg) {
    auto& c = *static_cast<ArpConsulta*>(arg);
    auto* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    auto* n = sta ? static_cast<struct netif*>(esp_netif_get_netif_impl(sta)) : nullptr;
    if (n) {
        if (c.solicitar) etharp_request(n, &c.ip);
        struct eth_addr* mac = nullptr;
        const ip4_addr_t* ip = nullptr;
        c.encontrado = etharp_find_addr(n, &c.ip, &mac, &ip) >= 0;
        if (c.encontrado) memcpy(c.mac, mac->addr, 6);
    }
    xSemaphoreGive(c.pronto);
}
bool arp(ArpConsulta& c, bool solicitar) {
    c.solicitar = solicitar;
    c.encontrado = false;
    if (tcpip_callback(consultarArp, &c) != ERR_OK) return false;
    // Mantem o contexto vivo ate o callback terminar; nenhuma tarefa de radio espera aqui.
    xSemaphoreTake(c.pronto, portMAX_DELAY);
    return c.encontrado;
}
void pingSucesso(esp_ping_handle_t h, void* arg) {
    auto& c = *static_cast<PingConsulta*>(arg);
    c.respondeu = esp_ping_get_profile(h, ESP_PING_PROF_TIMEGAP, &c.ms, sizeof(c.ms)) == ESP_OK;
}
void pingFim(esp_ping_handle_t, void* arg) {
    xSemaphoreGive(static_cast<PingConsulta*>(arg)->pronto);
}
struct Escrita { Registro* registros; uint32_t quantidade; };
String macTexto(const uint8_t* mac) {
    char texto[18];
    snprintf(texto, sizeof(texto), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(texto);
}
const char* descoberta(const Registro& r) { return r.ip == proprioIP ? "self" : (r.ping ? "icmp_arp" : "arp_cache"); }
String upsert(const Registro& r) {
    // Todos os valores vem de IP/MAC binarios, numeros ou enumeracoes fixas.
    return "INSERT INTO public.netscan(mac,ip,response_time,ping_replied,discovery,data_leitura) VALUES ('" +
        macTexto(r.mac) + "','" + endereco(r.ip).toString() + "'," + (r.ping ? String(r.ms) : String("NULL")) + "," +
        (r.ping ? "TRUE" : "FALSE") + ",'" + descoberta(r) + "',CURRENT_TIMESTAMP) ON CONFLICT(mac) DO UPDATE SET "
        "ip=EXCLUDED.ip,response_time=EXCLUDED.response_time,ping_replied=EXCLUDED.ping_replied,"
        "discovery=EXCLUDED.discovery,data_leitura=CURRENT_TIMESTAMP;";
}
void enviarSQL(Registro* registros, uint32_t total, const NetScan::Ajustes& config) {
    PgClient pg;
    if (!pg.conectar(config.servidor, config.porta, config.banco, config.usuario, config.senha)) {
        NetScan::registrarSQL(false, "Conexao/autenticacao: " + pg.erro(), config); return;
    }
    if (!pg.executar("SET statement_timeout='2000ms'; SET lock_timeout='1000ms';") || !pg.executar("BEGIN;")) {
        NetScan::registrarSQL(false, "Preparacao da transacao: " + pg.erro(), config); return;
    }
    uint32_t inicio = millis();
    for (uint32_t i = 0; i < total; ++i) {
        if (millis() - inicio > 120000) {
            NetScan::registrarSQL(false, "Lote excedeu 120 s; transacao sem commit.", config); return;
        }
        if (!pg.executar(upsert(registros[i]))) {
            NetScan::registrarSQL(false, "INSERT/UPDATE MAC=" + macTexto(registros[i].mac) + ": " + pg.erro() + " (lote sem commit).", config); return;
        }
        vTaskDelay(1);
    }
    if (!pg.executar("COMMIT;")) {
        NetScan::registrarSQL(false, "Confirmacao do COMMIT: " + pg.erro() + " Resultado deve ser conferido no servidor.", config); return;
    }
    NetScan::registrarSQL(true, String(total) + " registros confirmados.", config);
}
bool escrever(File& file, void* arg) {
    auto& e = *static_cast<Escrita*>(arg);
    if (file.print("{\n") != 2) return false;
    for (uint32_t i = 0; i < e.quantidade; ++i) {
        auto& r = e.registros[i];
        char mac[18], linha[240];
        strlcpy(mac, macTexto(r.mac).c_str(), sizeof(mac));
        String tempo = r.ping ? String(r.ms) : String("null");
        int len = snprintf(linha, sizeof(linha),
            "  \"%s\": {\"ip\":\"%s\",\"mac\":\"%s\",\"response_time_ms\":%s,\"ping_replied\":%s,\"discovery\":\"%s\"}%s\n",
            mac, endereco(r.ip).toString().c_str(), mac, tempo.c_str(), r.ping ? "true" : "false",
            descoberta(r), i + 1 < e.quantidade ? "," : "");
        if (len <= 0 || size_t(len) >= sizeof(linha) || file.write(reinterpret_cast<const uint8_t*>(linha), len) != size_t(len)) return false;
        if ((i & 31) == 31) vTaskDelay(1);
    }
    return file.print("}\n") == 2;
}
void tarefa(void*) {
    const auto config = NetScan::configuracao();
    const uint32_t capacidade = fimRede - inicioRede - 1;
    auto* registros = new (std::nothrow) Registro[capacidade];
    ArpConsulta ac{}; ac.pronto = xSemaphoreCreateBinary();
    PingConsulta pc{}; pc.pronto = xSemaphoreCreateBinary();
    bool ok = registros && ac.pronto && pc.pronto;
    uint32_t total = 0;
    for (uint32_t ip = inicioRede + 1; ok && ip < fimRede; ++ip) {
        if (WiFi.status() != WL_CONNECTED || numero(WiFi.localIP()) != proprioIP ||
            (numero(WiFi.subnetMask()) & proprioIP) != inicioRede ||
            (inicioRede | ~numero(WiFi.subnetMask())) != fimRede) { ok = false; break; }
        Registro r{}; r.ip = ip;
        if (ip == proprioIP) {
            WiFi.macAddress(r.mac);
        } else {
            IPAddress alvo = endereco(ip);
            ac.ip.addr = static_cast<uint32_t>(alvo);
            arp(ac, true);
            esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
            config.count = 1; config.timeout_ms = 300; config.interval_ms = 10; config.task_prio = 1;
            IP_ADDR4(&config.target_addr, alvo[0], alvo[1], alvo[2], alvo[3]);
            auto* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (!sta) { ok = false; break; }
            int interface = esp_netif_get_netif_impl_index(sta);
            if (interface <= 0) { ok = false; break; }
            config.interface = static_cast<uint32_t>(interface);
            esp_ping_callbacks_t cb{}; cb.cb_args = &pc; cb.on_ping_success = pingSucesso; cb.on_ping_end = pingFim;
            esp_ping_handle_t ping = nullptr;
            pc.respondeu = false; pc.ms = 0;
            if (esp_ping_new_session(&config, &cb, &ping) != ESP_OK) { ok = false; break; }
            if (esp_ping_start(ping) != ESP_OK) { esp_ping_delete_session(ping); ok = false; break; }
            xSemaphoreTake(pc.pronto, portMAX_DELAY);
            esp_ping_delete_session(ping);
            // O ping aguardou ate 300 ms; a consulta ARP inclui quem bloqueia ICMP.
            if (!arp(ac, false)) { verificados.fetch_add(1); vTaskDelay(1); continue; }
            memcpy(r.mac, ac.mac, 6); r.ping = pc.respondeu; r.ms = pc.ms;
        }
        // Um unico objeto por MAC, mesmo com aliases de IP. Prefere resposta ICMP.
        uint32_t indice = 0;
        while (indice < total && memcmp(registros[indice].mac, r.mac, 6)) ++indice;
        if (indice == total) registros[total++] = r;
        else if (r.ping) registros[indice] = r;
        encontrados.store(total); verificados.fetch_add(1);
        vTaskDelay(1);
    }
    const bool varreduraCompleta = ok;
    if (ok) {
        Escrita escrita{registros, total};
        ok = sd->salvarArquivoSeguro("/netscan.json", escrever, &escrita);
    }
    if (varreduraCompleta && config.sqlAtivo) enviarSQL(registros, total, config);
    if (ac.pronto) vSemaphoreDelete(ac.pronto);
    if (pc.pronto) vSemaphoreDelete(pc.pronto);
    delete[] registros;
    resultado.store(ok ? 1 : 2);
    Serial.printf("[NETSCAN] %s: %lu equipamentos\n", ok ? "Salvo /netscan.json" : "Falha", static_cast<unsigned long>(total));
    ocupado.store(false);
    vTaskDelete(nullptr);
}
}

String NetScan::iniciar(StorageManager& storage) {
    if (testePostgresAtivo()) return "Erro: aguarde o teste PostgreSQL terminar antes do netscan.\n";
    bool esperado = false;
    if (!ocupado.compare_exchange_strong(esperado, true)) return estado();
    if (testePostgresAtivo()) {
        ocupado.store(false); return "Erro: aguarde o teste PostgreSQL terminar antes do netscan.\n";
    }
    if (WiFi.status() != WL_CONNECTED || !storage.isAtivo()) {
        ocupado.store(false); return "Erro: netscan precisa de WiFi conectado e SD ativo.\n";
    }
    proprioIP = numero(WiFi.localIP());
    uint32_t mascara = numero(WiFi.subnetMask()), hosts = ~mascara;
    if (!mascara || (hosts & (hosts + 1)) || hosts < 3 || hosts - 1 > MAX_HOSTS) {
        ocupado.store(false); return "Erro: sub-rede invalida ou maior que 1022 hosts (/22).\n";
    }
    inicioRede = proprioIP & mascara; fimRede = inicioRede | hosts;
    sd = &storage; verificados.store(0); encontrados.store(0); resultado.store(0);
    if (xTaskCreatePinnedToCore(tarefa, "NetScan", 8192, nullptr, 1, nullptr, 1) != pdPASS) {
        ocupado.store(false); resultado.store(2); return "Erro: memoria insuficiente para netscan.\n";
    }
    return "Netscan iniciado em segundo plano. Consulte netscan_status; resultado: /netscan.json.\n";
}
String NetScan::estado() {
    return String(ocupado.load() ? "Netscan em andamento" : (resultado.load() == 1 ? "Netscan concluido: /netscan.json" : (resultado.load() == 2 ? "Netscan falhou; resultado anterior preservado" : "Netscan ainda nao executado"))) +
        "\nIPs verificados: " + String(verificados.load()) + "\nMACs encontrados: " + String(encontrados.load()) +
        "\nAutomatico: " + (configuracao().automatico ? "on" : "off") + "\nIntervalo: " + String(configuracao().intervaloMin) +
        " min\nPostgreSQL habilitado: " + (configuracao().sqlAtivo ? "sim" : "nao") + "\n" + estadoSQL() + "\n";
}

bool NetScan::emAndamento() { return ocupado.load(); }
