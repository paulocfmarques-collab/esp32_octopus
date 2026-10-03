#include "TelemetryEngine.h"
#include "NTPService.h"
#include "esp_system.h"

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
        delay(2); 
    }
    _network.responderUDP("--- Stream Fim ---\n");
    file.close();
}

void TelemetryEngine::executarComando(String cmd) {
    cmd.trim();
    Serial.printf("[UDP CMD] Processando: '%s'\n", cmd.c_str());
    
    char reply[256];
    snprintf(reply, sizeof(reply), "Comando recebido: %s\n", cmd.c_str());
    if (_storage.isAtivo()) {
        _storage.gravarLog(reply);
    }

    if (cmd == "RESET_WIFI") {
        _network.responderUDP("Resetando rede e voltando ao modo Portal...\n");
        _network.clearConfig();
        _leds.piscarSincrono(10, 100);
        ESP.restart();
    }
    else if (cmd == "LED_ON") {
        _leds.ligar();
        _network.responderUDP("LED Ativo\n");
    }
    else if (cmd == "LED_OFF") {
        _leds.desligar();
        _network.responderUDP("LED Desativado\n");
    }
    else if (cmd.startsWith("LED_BLINK:")) {
        int intervalo = cmd.substring(10).toInt();
        _leds.iniciarBlinkAsync(intervalo);
        _network.responderUDP("Blink configurado.\n");
    }
    else if (cmd == "TIME" || cmd == "HORA") {
        _network.responderUDP("Hora atual: " + ntp.obterApenasHora() + "\n");
    }
    else if (cmd == "DATE" || cmd == "DATA") {
        _network.responderUDP("Data atual: " + ntp.obterApenasData() + "\n");
    }
    else if (cmd == "RAM") {
        snprintf(reply, sizeof(reply), "Heap livre: %u | Menor heap: %u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
        _network.responderUDP(reply);
    }
    else if (cmd == "CPU") {
        snprintf(reply, sizeof(reply), "Modelo: %s | Nucleos: %d | Freq: %d MHz\n", ESP.getChipModel(), ESP.getChipCores(), ESP.getCpuFreqMHz());
        _network.responderUDP(reply);
    }
    else if (cmd == "LOG_UDP") {
        streamFileUDP("/log.txt");
    }
    else if (cmd == "LOG_OLD_UDP") {
        streamFileUDP("/log_old.txt");
    }
    else if (cmd == "DEL_LOG") {
        bool res = _storage.deletarArquivo("/log.txt");
        _network.responderUDP(res ? "Log principal deletado.\n" : "Falha ao deletar.\n");
    }
    // ─── COMANDO: LIST ───
    else if (cmd == "LIST" || cmd == "list") {
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
    // ─── COMANDO: SIZE:caminho ───
    else if (cmd.startsWith("SIZE:") || cmd.startsWith("size:")) {
        if (!_storage.isAtivo()) {
            _network.responderUDP("Erro: SD inacessivel\n");
        } else {
            String caminho = cmd.substring(5); caminho.trim();
            if (!caminho.startsWith("/")) caminho = "/" + caminho;

            if (!_storage.arquivoExiste(caminho.c_str())) {
                _network.responderUDP("Arquivo nao existe\n");
            } else {
                File arquivo = _storage.abrirArquivo(caminho.c_str(), FILE_READ);
                if (!arquivo || arquivo.isDirectory()) {
                    _network.responderUDP("Falha ao abrir arquivo\n");
                } else {
                    uint32_t tamanhoBytes = arquivo.size();
                    arquivo.close();
                    char pMsg[64];
                    sprintf(pMsg, "Tamanho: %u B\n", tamanhoBytes);
                    _network.responderUDP(pMsg);
                }
            }
        }
    }
    // ─── COMANDO: READ:caminho ───
    else if (cmd.startsWith("READ:") || cmd.startsWith("read:")) {
        String caminho = cmd.substring(5); caminho.trim();
        if (!caminho.startsWith("/")) caminho = "/" + caminho;
        streamFileUDP(caminho.c_str());
    }
    // ─── COMANDO: WRITE:/arquivo.txt=conteudo ───
    else if (cmd.startsWith("WRITE:") || cmd.startsWith("write:")) {
        if (!_storage.isAtivo()) {
            _network.responderUDP("Erro: SD inacessivel\n");
        } else {
            String dadosGerais = cmd.substring(6); dadosGerais.trim();
            int indiceSeparador = dadosGerais.indexOf('=');
            
            if (indiceSeparador == -1) {
                _network.responderUDP("Erro: Use WRITE:/arquivo.txt=conteudo\n");
            } else {
                String caminho = dadosGerais.substring(0, indiceSeparador);
                String conteudo = dadosGerais.substring(indiceSeparador + 1);
                caminho.trim();
                if (!caminho.startsWith("/")) caminho = "/" + caminho;

                File arquivo = _storage.abrirArquivo(caminho.c_str(), FILE_WRITE);
                if (!arquivo) {
                    _network.responderUDP("Erro ao abrir arquivo\n");
                } else {
                    arquivo.print(conteudo);
                    arquivo.close();
                    _network.responderUDP("Ok\n");
                }
            }
        }
    }
    else if (cmd == "INFO" || cmd == "info") {
        char infoBuffer[512];
        uint32_t heapLivreKB = ESP.getFreeHeap() / 1024;
        float flashLivreMB = (float)ESP.getFreeSketchSpace() / (1024.0 * 1024.0); 
        String statusSD = _storage.isAtivo() ? "OK" : "FALHA";

        snprintf(infoBuffer, sizeof(infoBuffer), 
            "===== DEVICE INFO =====\n"
            "Firmware: v1.0.0\n"
            "Build: %s %s\n"
            "SSID: %s\n"
            "IP: %s\n"
            "MAC: %s\n"
            "RSSI: %d dBm\n"
            "Heap Livre: %u KB\n"
            "Flash Livre: %.1f MB\n"
            "SD Card: %s\n"
            "Data: %s\n"
            "Hora: %s\n"
            "Uptime: %lu ms\n"
            "=======================\n",
            __DATE__, __TIME__, WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
            WiFi.macAddress().c_str(), WiFi.RSSI(), heapLivreKB, flashLivreMB,
            statusSD.c_str(), ntp.obterApenasData().c_str(), ntp.obterApenasHora().c_str(), millis()
        );
        _network.responderUDP(infoBuffer);
    }
    else if (cmd == "NET_INFO") {
        snprintf(reply, sizeof(reply), "IP: %s | SSID: %s | RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(), WiFi.RSSI());
        _network.responderUDP(reply);
    }
    else {
        _network.responderUDP("Comando desconhecido\n");
    }
}
