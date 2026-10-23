// TelemetryEngine.cpp - Parte 1 de 2
#include "TelemetryEngine.h"
#include "NTPService.h"
#include "defines.h"
#include "esp_system.h"
#include "CrashDiagnostics.h"
#include "PrometheusMetrics.h"
#include "ClimaService.h"
#include "NetScan.h"
extern ClimaService clima;
#include <WiFi.h>
#include <esp_task_wdt.h>

extern NTPService ntp;
extern PrometheusMetrics prometheus;

// Inicialização das variáveis estáticas primitivas para respostas estáveis
char TelemetryEngine::ultimoClienteIP[16] = "0.0.0.0";
uint16_t TelemetryEngine::ultimoClientePorta = 4210;

TelemetryEngine::TelemetryEngine(StorageManager& storage, WifiPortalManager& network,
                                 LedManager& leds)
    : _storage(storage), _network(network), _leds(leds)
{
}

bool TelemetryEngine::streamFileUDP(const char* filename)
{
    if (!_storage.arquivoExiste(filename))
    {
        _network.responderUDP("Erro: Arquivo nao encontrado.\n");
        return false;
    }

    File file = _storage.abrirArquivo(filename, FILE_READ);
    if (!file)
    {
        _network.responderUDP("Erro ao abrir arquivo.\n");
        return false;
    }

    _network.responderUDP("--- Stream Inicio ---\n");
    // Limita a leitura ao tamanho inicial, inclusive ao consultar o próprio log UDP.
    const size_t tamanhoInicial = file.size();
    while (file.available() && file.position() < tamanhoInicial)
    {
        String linha = file.readStringUntil('\n') + "\n";
        _network.responderUDP(linha);

        // Proteção do Watchdog durante a leitura de arquivos grandes no SD
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    _network.responderUDP("--- Stream Fim ---\n");
    file.close();
    return true;
}

void TelemetryEngine::executarComando(String cmd)
{
    PrometheusRequest request(prometheus, true);
    cmd.trim();
    Serial.printf("[UDP CMD] Processando: '%s'\n", cmd.c_str());

    char reply[256];

    String cmdLower = cmd;
    cmdLower.toLowerCase();

    // ─── 0. COMANDO: LISTAR TODOS OS COMANDOS (HELP) ───
    if (cmdLower == "help")
    {
        String listaComandos = "COMANDOS\n"
                               "help\n"
                               "info\n"
                               "status\n"
                               "version\n"
                               "build\n"
                               "reason\n"
                               "crash_info\n"
                               "reboot\n"
                               "reset_wifi\n"
                               "reset_counters\n"
                               "wifi_list\n"
                               "alive\n"
                               "name\n"
                               "temp\n"
                               "cpu\n"
                               "ram\n"
                               "heap\n"
                               "flash\n"
                               "uptime\n"
                               "net_info\n"
                               "netscan\n"
                               "netscan_status\n"
                               "netscan_interval:minutos\n"
                               "netscan_auto:on|off\n"
                               "mac\n"
                               "rssi\n"
                               "ip\n"
                               "time\n"
                               "date\n"
                               "age\n"
                               "led_on\n"
                               "led_off\n"
                               "led_blink[:ms]\n"
                               "sd_status\n"
                               "sd_list\n"
                               "sd_log\n"
                               "sd_clear_log\n"
                               "sd_read:arquivo\n"
                               "sd_size:arquivo\n"
                               "sd_write:arquivo:texto\n"
                               "sd_del:arquivo\n";
        _network.responderUDP(listaComandos);
    }
    else if (cmdLower.startsWith("netscan_interval:"))
    {
        String resposta = NetScan::configurarIntervalo(cmdLower.substring(17));
        request.erro = resposta.startsWith("Erro:");
        _network.responderUDP(resposta);
    }
    else if (cmdLower.startsWith("netscan_auto:"))
    {
        String resposta = NetScan::configurarAutomatico(cmdLower.substring(13));
        request.erro = resposta.startsWith("Erro:");
        _network.responderUDP(resposta);
    }
    else if (cmdLower == "netscan")
    {
        String resposta = NetScan::iniciar(_storage);
        request.erro = resposta.startsWith("Erro:");
        _network.responderUDP(resposta);
    }
    else if (cmdLower == "netscan_status")
    {
        _network.responderUDP(NetScan::estado());
    }
    else if (cmdLower == "age")
    {
        _network.responderUDP(clima.obterIdade());
    }
    else if (cmdLower == "name")
    {
        _network.responderUDP("Octopus\n");
    }
    else if (cmdLower == "reset_counters")
    {
        _network.responderUDP(prometheus.resetCounters()
                                  ? "Contadores NVS de boot/watchdog zerados.\n"
                                  : "Erro ao zerar contadores NVS.\n");
    }
    else if (cmdLower == "status")
    {
        String statusWifi = (WiFi.status() == WL_CONNECTED) ? "OK" : "FALHA";
        String storageStatus = (_storage.isAtivo() ? "OK" : "FALHA");
        uint32_t heapKB = ESP.getFreeHeap() / 1024;
        _network.responderUDP("ONLINE\nWiFi: " + statusWifi + "\nSD: " + storageStatus +
                              "\nNTP: OK\nHeap: " + String(heapKB) + " KB");
    }
    else if (cmdLower == "uptime")
    {
        snprintf(reply, sizeof(reply), "Uptime: %lu s\n", millis() / 1000);
        _network.responderUDP(String(reply));
    }
    else if (cmdLower == "version")
    {
        _network.responderUDP("Versao Firmware: " + String(Config::FIRMWARE_VERSION));
    }
    else if (cmdLower == "build")
    {
        _network.responderUDP("Build:\nData: " + String(__DATE__) + "\nHora: " + String(__TIME__));
    }
    else if (cmdLower == "mac")
    {
        _network.responderUDP("MAC: " + WiFi.macAddress() + "\n");
    }
    else if (cmdLower == "temp")
    {
        _network.responderUDP("CPU Temp: " + String(temperatureRead(), 2) + " C");
    }
    else if (cmdLower == "reboot")
    {
        _network.responderUDP("Reiniciando...\n");
        delay(200);
        ESP.restart();
    }
    else if (cmdLower == "alive")
    {
        _network.responderUDP("alive\n");
    }
    // TelemetryEngine.cpp - Parte 2 de 2 (Continuação direta do bloco anterior)
    else if (cmdLower == "heap")
    {
        _network.responderUDP("Heap livre: " + String(ESP.getFreeHeap() / 1024) + " KB\n");
    }
    else if (cmdLower == "rssi")
    {
        _network.responderUDP("RSSI: " + String(WiFi.RSSI()) + " dBm\n");
    }
    else if (cmdLower == "ip")
    {
        _network.responderUDP("IP: " + WiFi.localIP().toString() + "\n");
    }
    else if (cmdLower == "sd_status")
    {
        _network.responderUDP(String("SD: ") + (_storage.isAtivo() ? "OK" : "FALHA") + "\n");
    }
    else if (cmdLower == "reset_wifi")
    {
        _network.responderUDP("Resetando rede e voltando ao modo Portal...\n");
        _network.clearConfig();
        _leds.piscarSincrono(10, 100);
        ESP.restart();
    }
    else if (cmdLower == "wifi_list")
    {
        String listagem = _network.obterListaRedesTXT();
        _network.responderUDP(listagem);
    }
    else if (cmdLower == "led_on")
    {
        _leds.ligar();
        _network.responderUDP("LED Ativo\n");
    }
    else if (cmdLower == "led_off")
    {
        _leds.desligar();
        _network.responderUDP("LED Desativado\n");
    }
    else if (cmdLower.startsWith("led_blink"))
    {
        int p = cmd.indexOf(':');
        unsigned long ms = (p > 0) ? (unsigned long)cmd.substring(p + 1).toInt() : 1000;
        if (ms < 50)
            ms = 50;
        _leds.iniciarBlinkAsync(ms);
        _network.responderUDP("LED piscando a cada " + String(ms) + " ms\n");
    }
    else if (cmdLower == "time")
    {
        _network.responderUDP("Hora atual: " + ntp.obterApenasHora() + "\n");
    }
    else if (cmdLower == "date")
    {
        _network.responderUDP("Data atual: " + ntp.obterApenasData() + "\n");
    }
    else if (cmdLower == "flash")
    {
        snprintf(reply, sizeof(reply),
                 "Flash total: %0.2f MB\nSketch: %0.2f MB\nLivre: %0.2f MB - %0.2f %%\n",
                 (float)ESP.getFlashChipSize() / Config::DivMb,
                 (float)ESP.getSketchSize() / Config::DivMb,
                 (float)ESP.getFreeSketchSpace() / Config::DivMb,
                 100.0 * (float)ESP.getFreeSketchSpace() / (float)ESP.getFlashChipSize());
        _network.responderUDP(String(reply));
    }
    else if (cmdLower == "ram")
    {
        snprintf(reply, sizeof(reply),
                 "Heap livre: %0.2f kb\nMenor heap livre: %.02f kb\nMaior bloco livre: %0.2f kb\n",
                 ESP.getFreeHeap() / Config::DivKb, ESP.getMinFreeHeap() / Config::DivKb,
                 ESP.getMaxAllocHeap() / Config::DivKb);
        _network.responderUDP(String(reply));
    }
    else if (cmdLower == "cpu")
    {
        snprintf(reply, sizeof(reply),
                 "Modelo: %s\nRevisao: %d\nNucleos: %d\nCPU: %d MHz\nRAM livre: %u bytes\n",
                 ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz(),
                 ESP.getFreeHeap());
        _network.responderUDP(String(reply));
    }
    else if (cmdLower == "sd_log")
        request.erro = !streamFileUDP("/log.txt");
    else if (cmdLower == "sd_clear_log")
    {
        bool ok = _storage.deletarArquivo("/log.txt");
        request.erro = !ok;
        _network.responderUDP(ok ? "Log principal deletado.\n" : "Falha ao deletar.\n");
    }
    else if (cmdLower.startsWith("sd_list"))
    {
        if (!_storage.isAtivo())
        {
            request.erro = true;
            _network.responderUDP("Erro: SD inacessivel\n");
        }
        else
        {
            File raiz = SD.open("/");
            File arquivo = raiz.openNextFile();
            if (!arquivo)
            {
                _network.responderUDP("Diretorio vazio\n");
            }
            else
            {
                _network.responderUDP("--- Arquivos SD ---\n");
                while (arquivo)
                {
                    _network.responderUDP(String(arquivo.name()) + " (" + String(arquivo.size()) +
                                          " B)\n");
                    arquivo = raiz.openNextFile();
                }
                _network.responderUDP("-------------------\n");
            }
            raiz.close();
        }
    }
    else if (cmdLower.startsWith("sd_size:"))
    {
        String caminho = cmd.substring(8);
        caminho.trim();
        if (!caminho.startsWith("/"))
            caminho = "/" + caminho;

        if (!_storage.arquivoExiste(caminho.c_str()))
        {
            request.erro = true;
            _network.responderUDP("Nao existe\n");
        }
        else
        {
            File arquivo = _storage.abrirArquivo(caminho.c_str(), FILE_READ);
            char pMsg[64];
            sprintf(pMsg, "Tamanho: %u B\n", arquivo.size());
            arquivo.close();
            _network.responderUDP(String(pMsg));
        }
    }
    else if (cmdLower.startsWith("sd_read:"))
    {
        String caminho = cmd.substring(8);
        caminho.trim();
        if (!caminho.startsWith("/"))
            caminho = "/" + caminho;
        request.erro = !streamFileUDP(caminho.c_str());
    }
    else if (cmdLower.startsWith("sd_write:"))
    {
        String dadosGerais = cmd.substring(9);
        int idx = dadosGerais.indexOf(':');
        if (idx == -1)
        {
            request.erro = true;
            _network.responderUDP("Erro: Use sd_write:/a.txt:conteudo\n");
        }
        else
        {
            String caminho = dadosGerais.substring(0, idx);
            String conteudo = dadosGerais.substring(idx + 1);
            caminho.trim();
            if (!caminho.startsWith("/"))
                caminho = "/" + caminho;

            File arquivo = _storage.abrirArquivo(caminho.c_str(), FILE_WRITE);
            bool ok = arquivo && arquivo.print(conteudo) == conteudo.length();
            arquivo.close();
            request.erro = !ok;
            _network.responderUDP(ok ? "Ok\n" : "Falha ao gravar arquivo\n");
        }
    }
    else if (cmdLower == "info")
    {
        char infoBuffer[512];
        uint32_t heapLivreKB = ESP.getFreeHeap() / 1024;
        float flashLivreMB = (float)ESP.getFreeSketchSpace() / (1024.0 * 1024.0);
        String statusSD = _storage.isAtivo() ? "OK" : "FALHA";

        String rStr = CrashDiagnostics::motivoReset();

        snprintf(infoBuffer, sizeof(infoBuffer),
                 "===== DEVICE INFO =====\nFirmware: %s\nBuild: %s %s\nReset: %s\nSSID: "
                 "%s\nIP: %s\nMAC: %s\nRSSI: %d dBm\nHeap: %u KB\nFlash L.: %.1f MB\nSD Card: "
                 "%s\nData: %s\nHora: %s\nUptime: %lu ms\n=======================\n",
                 Config::FIRMWARE_VERSION, __DATE__, __TIME__, rStr.c_str(), WiFi.SSID().c_str(),
                 WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(), WiFi.RSSI(),
                 heapLivreKB, flashLivreMB, statusSD.c_str(), ntp.obterApenasData().c_str(),
                 ntp.obterApenasHora().c_str(), millis());
        _network.responderUDP(String(infoBuffer));
    }
    else if (cmdLower == "net_info")
    {
        snprintf(reply, sizeof(reply), "IP: %s\nSSID: %s\nRSSI: %d dBm\n",
                 WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(), WiFi.RSSI());
        _network.responderUDP(String(reply));
    }
    else if (cmdLower == "crash_info")
    {
        _network.responderUDP(CrashDiagnostics::lerArquivarEApagar(_storage));
    }
    else if (cmdLower == "reason")
    {
        String txt = CrashDiagnostics::motivoReset();
        _network.responderUDP("Ultimo Reset: [" + txt + "]\n");
    }
    else if (cmdLower.startsWith("sd_del:"))
    {
        String caminho = cmd.substring(7);
        caminho.trim();
        if (!caminho.startsWith("/"))
            caminho = "/" + caminho;
        bool ok = _storage.deletarArquivo(caminho.c_str());
        request.erro = !ok;
        _network.responderUDP(ok ? "Arquivo deletado.\n" : "Falha ao deletar.\n");
    }
    else
    {
        request.erro = true;
        _network.responderUDP("Comando desconhecido\n");
    }
}
