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
    if (cmdLower == "help" || cmdLower == "?") {
        String listaComandos = 
            "--- Comandos Disponiveis ---\n"
            "help ou ?         - Lista todos os comandos existentes\n"
            "reset_wifi        - Reseta rede e volta ao modo Portal\n"
            "led_on            - Liga o LED azul\n"
            "led_off           - Desliga o LED azul\n"
            "led_blink:[ms]    - Configura piscada assincrona\n"
            "time ou hora      - Exibe a hora atual do sistema\n"
            "date ou data      - Exibe a data atual do sistema\n"
            "flash             - Diagnostico da memoria Flash\n"
            "ram               - Diagnostico de memoria RAM\n"
            "cpu               - Informacoes sobre o processador\n"
            "log_udp           - Stream do log atual pelo UDP\n"
            "log_old_udp       - Stream do log rotacionado\n"
            "del_log           - Deleta o arquivo de log principal\n"
            "list              - Lista arquivos presentes no SD\n"
            "size:[caminho]    - Exibe tamanho de um arquivo especifico\n"
            "read:[caminho]    - Exibe conteudo bruto de um arquivo\n"
            "write:[cam]=[txt] - Escreve dados em um arquivo no SD\n"
            "info              - Painel de informacoes consolidado\n"
            "net_info          - Informacoes basicas da rede Wi-Fi\n"
            "reset_reason      - Mostra o motivo do ultimo reset\n"
            "temp              - Temperatura interna da CPU C\n"            
            "uptime            - Tempo de atividade em segundos\n"
            "mac               - Endereco MAC fisico do Wi-Fi\n"
            "version           - Versao atual do firmware mestre\n"
            "build             - Data e hora da compilacao\n"
            "status            - Resumo rapido de conexao e heap\n"
            "----------------------------\n";
        _network.responderUDP(listaComandos);
    }
    else if (cmd == "status") {
        String statusWifi = (WiFi.status() == WL_CONNECTED) ? "OK" : "FALHA";
        String storageStatus = (_storage.isAtivo() ? "OK" : "FALHA");
        uint32_t heapKB = ESP.getFreeHeap() / 1024;
        _network.responderUDP("ONLINE\nWiFi: " + statusWifi + "\nSD: " + storageStatus + "\nNTP: OK\nHeap: " + String(heapKB) + " KB");
    }
    else if (cmd == "uptime") {
        snprintf(reply, sizeof(reply), "Uptime: %lu ms\n", millis());
        _network.responderUDP(String(reply));
    }
    else if (cmd == "version") {
        _network.responderUDP("Versao Firmware: v1.0.0");
    }
    else if (cmd == "build") {
        _network.responderUDP("Build:\nData: " + String(__DATE__) + "\nHora: " + String(__TIME__));
    }
    else if (cmd == "mac") {
        _network.responderUDP("MAC: " + WiFi.macAddress() + "\n");
    }
    else if (cmd == "temp") {
        _network.responderUDP("CPU Temp: " + String(temperatureRead(), 2) + " C");
    }
    // ─── 1. COMANDO: RESET DO WIFI ───
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
    else if (cmdLower.startsWith("led_blink:")) {
        _leds.iniciarBlinkAsync(cmd.substring(10).toInt()); 
        _network.responderUDP("Blink configurado.\n");
    }
    // ─── 3. COMANDOS DE DATA E HORA ───
    else if (cmdLower == "time" || cmdLower == "hora") {
        _network.responderUDP("Hora atual: " + ntp.obterApenasHora() + "\n");
    }
    else if (cmdLower == "date" || cmdLower == "data") {
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
    else if (cmdLower == "log_udp") streamFileUDP("/log.txt");
    else if (cmdLower == "log_old_udp") streamFileUDP("/log_old.txt");
    else if (cmdLower == "del_log") {
        _network.responderUDP(_storage.deletarArquivo("/log.txt") ? "Log principal deletado.\n" : "Falha ao deletar.\n");
    }
    // ─── 6. COMANDO DE LISTAGEM DO SD ───
    else if (cmdLower == "list") {
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
    else if (cmdLower.startsWith("size:")) {
        String caminho = cmd.substring(5); caminho.trim(); 
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
    else if (cmdLower.startsWith("read:")) {
        String caminho = cmd.substring(5); caminho.trim(); 
        if (!caminho.startsWith("/")) caminho = "/" + caminho;
        streamFileUDP(caminho.c_str());
    }
    // ─── 9. COMANDO: ESCREVER DADOS NO SD ───
    else if (cmdLower.startsWith("write:")) {
        String dadosGerais = cmd.substring(6); 
        int idx = dadosGerais.indexOf('=');
        if (idx == -1) { 
            _network.responderUDP("Erro: Use WRITE:/a.txt=conteudo\n"); 
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
    else if (cmdLower == "reset_reason" || cmdLower == "reason") {
        esp_reset_reason_t r = esp_reset_reason();
        String txt = (r == ESP_RST_POWERON) ? "POWER_ON" : 
                     (r == ESP_RST_EXT) ? "PIN_RESET" : 
                     (r == ESP_RST_TASK_WDT) ? "TASK_WATCHDOG" : "SOFTWARE/OUTROS";
        _network.responderUDP("Ultimo Reset: [" + txt + "]\n");
    }
    // ─── 12. TRATAMENTO DE COMANDO DESCONHECIDO ───
    else { 
        _network.responderUDP("Comando desconhecido\n"); 
    }
}
