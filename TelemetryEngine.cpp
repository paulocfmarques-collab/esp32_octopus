// TelemetryEngine.cpp
#include "TelemetryEngine.h"
#include "NTPService.h"
#include "defines.h"
#include "esp_system.h"
#include <WiFi.h>
#include <esp_task_wdt.h>

// Inicialização das variáveis estáticas primitivas para respostas estáveis
char TelemetryEngine::ultimoClienteIP[16] = "0.0.0.0";
uint16_t TelemetryEngine::ultimoClientePorta = 4210;

TelemetryEngine::TelemetryEngine(StorageManager& storage, WifiPortalManager& network, LedManager& leds)
    : _storage(storage), _network(network), _leds(leds) {}

void TelemetryEngine::streamFileUDP(const char* filename) {
    if (!_storage.arquivoExiste(filename)) {
        _network.responderUDP("Erro: Arquivo nao encontrado.\n");
        return;
    }

    File file = _storage.abrirArquivo(filename, FILE_READ);
    if (!file) {
        _network.responderUDP("Erro ao abrir arquivo.\n");
        return;
    }

    _network.responderUDP("--- Stream Inicio ---\n");
    while (file.available()) {
        String linha = file.readStringUntil('\n') + "\n";
        _network.responderUDP(linha);
        
        // Proteção do Watchdog durante a leitura de arquivos grandes no SD
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(2)); 
    }
    _network.responderUDP("--- Stream Fim ---\n");
    file.close();
}

void TelemetryEngine::executarComando(String cmd) {
    cmd.trim();
    Serial.printf("[UDP CMD] Processando: '%s'\n", cmd.c_str());
    
    char reply[256];
    snprintf(reply, sizeof(reply), "Comando recebido: %s\n", cmd.c_str());
    if (_storage.isAtivo()) _storage.gravarLog(reply);

    String cmdLower = cmd;
    cmdLower.toLowerCase();

    // ─── 0. COMANDO: LISTAR TODOS OS COMANDOS (HELP) ───
    if (cmdLower == "help") {
        String listaComandos = 
            "--- Comandos Disponiveis ---\n"
            "info / status / version / build / reason\n"
            "reboot / reset_wifi / alive\n"
            "temp / cpu / ram / heap / flash / uptime\n"
            "net_info / mac / rssi / ip\n"
            "time / date\n"
            "led_on / led_off / led_blink[:ms]\n"
            "--- SD ---\n"
            "sd_status / sd_list / sd_log / sd_clear_log\n"
            "sd_read:/arq / sd_size:/arq\n"
            "sd_write:/arq:texto / sd_del:/arq\n"
            "----------------------------\n";
        _network.responderUDP(listaComandos);
    }
    else if (cmdLower == "status") {
        String statusWifi = (WiFi.status() == WL_CONNECTED) ? "OK" : "FALHA";
        String storageStatus = (_storage.isAtivo() ? "OK" : "FALHA");
        uint32_t heapKB = ESP.getFreeHeap() / 1024;
        _network.responderUDP("ONLINE\nWiFi: " + statusWifi + "\nSD: " + storageStatus + "\nNTP: OK\nHeap: " + String(heapKB) + " KB");
    }
    else if (cmdLower == "uptime") {
        snprintf(reply, sizeof(reply), "Uptime: %lu s\n", millis() / 1000);
        _network.responderUDP(String(reply));
    }
    else if (cmdLower == "version") {
        _network.responderUDP("Versao Firmware: v1.0.0");
    }
    else if (cmdLower == "build") {
        _network.responderUDP("Build:\nData: " + String(__DATE__) + "\nHora: " + String(__TIME__));
    }
    else if (cmdLower == "mac") {
        _network.responderUDP("MAC: " + WiFi.macAddress() + "\n");
    }
    else if (cmdLower == "temp") {
        _network.responderUDP("CPU Temp: " + String(temperatureRead(), 2) + " C");
    }
    // ─── 1. COMANDO: RESET DO WIFI ───
    else if (cmdLower == "reboot") {
        _network.responderUDP("Reiniciando...\n");
        delay(200);
        ESP.restart();
    }
    else if (cmdLower == "alive") {
        _network.responderUDP("ip: " + WiFi.localIP().toString() + " - yes\n");
    }
    else if (cmdLower == "heap") {
        _network.responderUDP("Heap livre: " + String(ESP.getFreeHeap() / 1024) + " KB\n");
    }
    else if (cmdLower == "rssi") {
        _network.responderUDP("RSSI: " + String(WiFi.RSSI()) + " dBm\n");
    }
    else if (cmdLower == "ip") {
        _network.responderUDP("IP: " + WiFi.localIP().toString() + "\n");
    }
    else if (cmdLower == "sd_status") {
        _network.responderUDP(String("SD: ") + (_storage.isAtivo() ? "OK" : "FALHA") + "\n");
    }
    else if (cmdLower == "reset_wifi") {
        _network.responderUDP("Resetando rede e voltando ao modo Portal...\n");
        _network.clearConfig();
        _leds.piscarSincrono(10, 100); 
        ESP.restart();
    }
    // ─── 2. COMANDOS DOS LEDS ───
    else if (cmdLower == "led_on") { 
        _leds.ligar(); 
        _network.responderUDP("LED Ativo\n"); 
    }
    else if (cmdLower == "led_off") { 
        _leds.desligar(); 
        _network.responderUDP("LED Desativado\n"); 
    }
    else if (cmdLower.startsWith("led_blink")) {
        int p = cmd.indexOf(':');
        unsigned long ms = (p > 0) ? (unsigned long)cmd.substring(p + 1).toInt() : 1000;
        if (ms < 50) ms = 50;
        _leds.iniciarBlinkAsync(ms); 
        _network.responderUDP("LED piscando a cada " + String(ms) + " ms\n");
    }
    // ─── 3. COMANDOS DE DATA E HORA ───
    else if (cmdLower == "time") {
        _network.responderUDP("Hora atual: " + ntp.obterApenasHora() + "\n");
    }
    else if (cmdLower == "date") {
        _network.responderUDP("Data atual: " + ntp.obterApenasData() + "\n");
    }
    // ─── 4. DIAGNÓSTICOS DO SISTEMA ───
    else if (cmdLower == "flash") {
        snprintf(reply, sizeof(reply), "Flash total: %0.2f MB\nVelocidade: %u\nSketch: %0.2f MB\nLivre: %0.2f MB\n", 
                (float)ESP.getFlashChipSize() / Config::DivMb, ESP.getFlashChipSpeed(), (float)ESP.getSketchSize() / Config::DivMb, (float)ESP.getFreeSketchSpace() / Config::DivMb);
        _network.responderUDP(String(reply));
    }
    else if (cmdLower == "ram") {
        snprintf(reply, sizeof(reply), "Heap livre: %0.2f kb\nMenor heap livre: %.02f kb\nMaior bloco livre: %0.2f kb\n", 
                ESP.getFreeHeap() / Config::DivKb, ESP.getMinFreeHeap() / Config::DivKb, ESP.getMaxAllocHeap() / Config::DivKb);
        _network.responderUDP(String(reply));
    }
    else if (cmdLower == "cpu") {
        snprintf(reply, sizeof(reply), "Modelo: %s\nRevisao: %d\nNucleos: %d\nCPU: %d MHz\nRAM livre: %u bytes\n", 
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz(), ESP.getFreeHeap());
        _network.responderUDP(String(reply));
    }
    // ─── 5. GERENCIAMENTO DE LOGS ───
    else if (cmdLower == "sd_log") streamFileUDP("/log.txt");
    else if (cmdLower == "sd_clear_log") {
        _network.responderUDP(_storage.deletarArquivo("/log.txt") ? "Log principal deletado.\n" : "Falha ao deletar.\n");
    }
    // ─── 6. COMANDO DE LISTAGEM DO SD ───
    else if (cmdLower == "sd_list") {
        if (!_storage.isAtivo()) { 
            _network.responderUDP("Erro: SD inacessivel\n"); 
        } else {
            File raiz = SD.open("/"); 
            File arquivo = raiz.openNextFile();
            if (!arquivo) { 
                _network.responderUDP("Diretorio vazio\n"); 
            } else {
                _network.responderUDP("--- Arquivos SD ---\n");
                while (arquivo) {
                    _network.responderUDP(String(arquivo.name()) + " (" + String(arquivo.size()) + " B)\n");
                    arquivo = raiz.openNextFile();
                }
                _network.responderUDP("-------------------\n");
            }
            raiz.close();
        }
    }
    // ─── 7. COMANDO: DETERMINAR TAMANHO DE ARQUIVO ───
    else if (cmdLower.startsWith("sd_size:")) {
        String caminho = cmd.substring(8); caminho.trim(); 
        if (!caminho.startsWith("/")) caminho = "/" + caminho;
        
        if (!_storage.arquivoExiste(caminho.c_str())) { 
            _network.responderUDP("Nao existe\n"); 
        } else {
            File arquivo = _storage.abrirArquivo(caminho.c_str(), FILE_READ);
            char pMsg[64]; 
            sprintf(pMsg, "Tamanho: %u B\n", arquivo.size());
            arquivo.close(); 
            _network.responderUDP(String(pMsg));
        }
    }
    // ─── 8. COMANDO: LER ARQUIVO BRUTO DO SD ───
    else if (cmdLower.startsWith("sd_read:")) {
        String caminho = cmd.substring(8); caminho.trim(); 
        if (!caminho.startsWith("/")) caminho = "/" + caminho;
        streamFileUDP(caminho.c_str());
    }
    // ─── 9. COMANDO: ESCREVER DADOS NO SD ───
    else if (cmdLower.startsWith("sd_write:")) {
        String dadosGerais = cmd.substring(9); 
        int idx = dadosGerais.indexOf(':');
        if (idx == -1) { 
            _network.responderUDP("Erro: Use sd_write:/a.txt:conteudo\n"); 
        } else {
            String caminho = dadosGerais.substring(0, idx); 
            String conteudo = dadosGerais.substring(idx + 1);
            caminho.trim(); 
            if (!caminho.startsWith("/")) caminho = "/" + caminho;
            
            File arquivo = _storage.abrirArquivo(caminho.c_str(), FILE_WRITE);
            arquivo.print(conteudo); 
            arquivo.close(); 
            _network.responderUDP("Ok\n");
        }
    }
    // ─── 10. PAINEL DE INFORMAÇÕES CONSOLIDADO (INFO) ───
    else if (cmdLower == "info") {
        char infoBuffer[512]; 
        uint32_t heapLivreKB = ESP.getFreeHeap() / 1024;
        float flashLivreMB = (float)ESP.getFreeSketchSpace() / (1024.0 * 1024.0); 
        String statusSD = _storage.isAtivo() ? "OK" : "FALHA";
        
        esp_reset_reason_t motivoRaw = esp_reset_reason();
        String rStr = (motivoRaw == ESP_RST_TASK_WDT) ? "TASK_WATCHDOG" : 
                      (motivoRaw == ESP_RST_POWERON) ? "POWER_ON" : 
                      (motivoRaw == ESP_RST_EXT) ? "PIN_RESET" : "SOFTWARE_RESET";

        snprintf(infoBuffer, sizeof(infoBuffer), 
            "===== DEVICE INFO =====\nFirmware: v1.0.0\nBuild: %s %s\nReset: %s\nSSID: %s\nIP: %s\nMAC: %s\nRSSI: %d dBm\nHeap: %u KB\nFlash L.: %.1f MB\nSD Card: %s\nData: %s\nHora: %s\nUptime: %lu ms\n=======================\n",
            __DATE__, __TIME__, rStr.c_str(), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(), WiFi.RSSI(), heapLivreKB, flashLivreMB, statusSD.c_str(), ntp.obterApenasData().c_str(), ntp.obterApenasHora().c_str(), millis()
        );
        _network.responderUDP(String(infoBuffer));
    }
    else if (cmdLower == "net_info") {
        snprintf(reply, sizeof(reply), "IP: %s\nSSID: %s\nRSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(), WiFi.RSSI());
        _network.responderUDP(String(reply));
    }
    // ─── 11. COMANDO DIAGNÓSTICO DO MOTIVO DO RESET ───
    else if (cmdLower == "reason") {
        esp_reset_reason_t r = esp_reset_reason();
        String txt = (r == ESP_RST_POWERON) ? "POWER_ON" : 
                     (r == ESP_RST_EXT) ? "PIN_RESET" : 
                     (r == ESP_RST_TASK_WDT) ? "TASK_WATCHDOG" : "SOFTWARE/OUTROS";
        _network.responderUDP("Ultimo Reset: [" + txt + "]\n");
    }
    // ─── 12. TRATAMENTO DE COMANDO DESCONHECIDO ───
    else if (cmdLower.startsWith("sd_del:")) {
        String caminho = cmd.substring(7); caminho.trim();
        if (!caminho.startsWith("/")) caminho = "/" + caminho;
        _network.responderUDP(_storage.deletarArquivo(caminho.c_str()) ? "Arquivo deletado.\n" : "Falha ao deletar.\n");
    }
    else { 
        _network.responderUDP("Comando desconhecido\n"); 
    }
}
