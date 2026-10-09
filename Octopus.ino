#include "defines.h"
#include "StorageManager.h"
#include "WifiPortalManager.h"
#include "LedManager.h"
#include "TelemetryEngine.h"
#include "NTPService.h"
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>

// Instanciação e Alocação dos Gerenciadores Estáticos do Sistema
SPIClass spiSD(VSPI);
StorageManager storage(Config::PIN_SD_CS, spiSD); 
WifiPortalManager network(Config::PIN_LED_BLUE);
LedManager leds(Config::PIN_LED_BLUE, Config::PIN_LED_GRN);
NTPService ntp; 
TelemetryEngine telemetry(storage, network, leds);

// Handlers do Sistema Multitarefa FreeRTOS
TaskHandle_t TaskRadioHandle = NULL;
TaskHandle_t TaskProcessorHandle = NULL;
TaskHandle_t TaskOtaHandle = NULL;
QueueHandle_t filaComandosUDP = NULL;

struct ComandoUDP {
    char texto[255];
};

// Protótipos das Tasks e Rotinas do Kernel
void TaskRadioCore0(void *pvParameters);
void TaskProcessorCore1(void *pvParameters);
void TaskOtaCore0(void *pvParameters);
void inicializarConfiguracaoOTA();

void setup() {
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    Serial.println("\n======================================================================");
    Serial.println("[SYSTEM] INICIALIZANDO FIRMWARE MULTITAREFA PARALELO + OTA (OCTOPUS)");
    Serial.println("======================================================================");

    // Inicializa o gerenciador físico de LEDs
    leds.begin();
    
    // Configura barramento SPI compartilhado para o driver do SD Card
    spiSD.begin(Config::PIN_SD_SCK, Config::PIN_SD_MISO, Config::PIN_SD_MOSI, Config::PIN_SD_CS);
    if (storage.inicializar()) {
        Serial.println(F("[STORAGE] Armazenamento inicializado com sucesso."));
    } else {
        Serial.println(F("[STORAGE] Alerta: Operando sem cartao SD local."));
    }

    // Varre a Fila Circular de 5 Redes para estabelecer conexão Station
    if (network.conectar()) {
        String ipLocal = WiFi.localIP().toString();
        Serial.println(F("\n--------------------------------------------------"));
        Serial.print(F("[WIFI] Conectado! Endereco IP do Dispositivo: "));
        Serial.println(ipLocal);
        Serial.println(F("--------------------------------------------------\n"));

        if (storage.isAtivo()) {
            storage.gravarLog("[SISTEMA] Dispositivo online no IP: " + ipLocal);
        }
        
        // Sincroniza relógio e dispara serviços de comunicação locais
        ntp.inicializar();
        network.iniciarUDP();
        network.iniciarWebServerLocal();
        
        // Configura diretrizes e proteções contra pânico do OTA
        inicializarConfiguracaoOTA();
    } else {
        Serial.println(F("[SYSTEM] Fila Wi-Fi esgotada. Disparando Portal de Emergencia..."));
        network.iniciarPortal();
    }

    // Criação da Fila Dinâmica do FreeRTOS para isolamento de comandos entre os núcleos
    filaComandosUDP = xQueueCreate(Config::TAMANHO_FILA_UDP, sizeof(ComandoUDP));
    if (filaComandosUDP == NULL) {
        Serial.println(F("[CRITICAL] Falha ao criar a fila do FreeRTOS. Resetando..."));
        ESP.restart();
    }

    // Configuração do Watchdog NATIVO do ESP32 para tolerar até 5 segundos de retenção
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 5000,                    
        .idle_core_mask = (1 << 0) | (1 << 1), 
        .trigger_panic = true                  
    };
    esp_task_wdt_reconfigure(&twdt_config);

    // ─── CRIAÇÃO E ALOCAÇÃO DAS TASKS EM DUAL-CORE NATIVO ───
    Serial.println(F("[RTOS] Distribuindo tarefas paralelas entre os nucleos..."));

    // Task de Coleta e Rádio - Prioridade 3 (Alta) alocada no Core 0
    xTaskCreatePinnedToCore(TaskRadioCore0, "TaskRadioCore0", 4096, NULL, 3, &TaskRadioHandle, 0);

    // Task de Processamento Lógico e Interpretador - Prioridade 2 (Média) alocada no Core 1
    xTaskCreatePinnedToCore(TaskProcessorCore1, "TaskProcessorCore1", 8192, NULL, 2, &TaskProcessorHandle, 1);

    // Task de Gravação Sem Fio (OTA) - Roda em background aproveitando os momentos ociosos do Core 0
    if (network.isConnected()) {
        xTaskCreatePinnedToCore(TaskOtaCore0, "TaskOtaCore0", 4096, NULL, 1, &TaskOtaHandle, 0);
    }

    Serial.println(F("[SYSTEM] Inicializacao multitarefa com OTA concluida com sucesso."));
    Serial.println(F("======================================================================\n"));
}

void loop() {
    // Exclui a task principal do loop padrão do Arduino para liberar recursos e dar total poder ao FreeRTOS
    vTaskDelete(NULL);
}

// ================= TAREFA DE RÁDIO (NÚCLEO 0) =================
void TaskRadioCore0(void *pvParameters) {
    (void) pvParameters;
    esp_task_wdt_user_handle_t wdt_handle = NULL;
    esp_task_wdt_add_user("WdtRadioCore0", &wdt_handle);

    for (;;) {
        if (wdt_handle != NULL) esp_task_wdt_reset_user(wdt_handle);

        network.processarPortal();

        if (network.isConnected()) {
            String msgUdpRecebida;
            if (network.checarMensagensUDP(msgUdpRecebida)) {
                
                // Captura os dados do cliente emissor via pontes criadas na classe
                String ipRemoto = network.getUdpRemoteIP().toString();
                snprintf(TelemetryEngine::ultimoClienteIP, 16, "%s", ipRemoto.c_str());
                TelemetryEngine::ultimoClientePorta = network.getUdpRemotePort();

                // Constrói e enfileira a struct para execução assíncrona no Core 1
                ComandoUDP novoComando;
                snprintf(novoComando.texto, sizeof(novoComando.texto), "%s", msgUdpRecebida.c_str());
                xQueueSend(filaComandosUDP, &novoComando, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2)); 
    }
}

// ================= TAREFA DE PROCESSAMENTO LÓGICO (NÚCLEO 1) =================
void TaskProcessorCore1(void *pvParameters) {
    (void) pvParameters;
    esp_task_wdt_user_handle_t wdt_handle = NULL;
    esp_task_wdt_add_user("WdtProcessorCore1", &wdt_handle);

    ComandoUDP comandoRecebido;

    for (;;) {
        if (wdt_handle != NULL) esp_task_wdt_reset_user(wdt_handle);

        // Processa estados assíncronos e blinks dos LEDs
        leds.atualizar();

        // Verifica se há pacotes enviados pelo Core 0 prontos para execução
        if (xQueueReceive(filaComandosUDP, &comandoRecebido, pdMS_TO_TICKS(10)) == pdTRUE) {
            telemetry.executarComando(String(comandoRecebido.texto));
        }

        // Monitor de Reset físico via Botão IO0 (Pressionar por 3s)
        if (digitalRead(Config::PIN_BTN_RST) == LOW) {
            unsigned long tempoPressionado = millis();
            bool pressionadoValido = true;

            while (millis() - tempoPressionado < 3000) {
                leds.atualizar();
                if (digitalRead(Config::PIN_BTN_RST) == HIGH) {
                    pressionadoValido = false;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            if (pressionadoValido) {
                network.clearConfig(); // Limpa inclusive a fila circular
                leds.piscarSincrono(10, 50);
                ESP.restart();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ================= TAREFA DE SEGUNDO PLANO: MONITOR OTA (NÚCLEO 0) =================
void TaskOtaCore0(void *pvParameters) {
    (void) pvParameters;
    Serial.println(F("[RTOS_BOOT] TaskOtaCore0 em background ativa no Core 0.\n"));
    
    esp_task_wdt_user_handle_t wdt_handle = NULL;
    esp_task_wdt_add_user("WdtOtaCore0", &wdt_handle);

    for (;;) {
        if (wdt_handle != NULL) esp_task_wdt_reset_user(wdt_handle);

        // Varre requisições de upload de binário via rede sem fios
        ArduinoOTA.handle();
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Delay estendido para não impactar o tráfego do rádio
    }
}

// ================= CONFIGURAÇÃO VISUAL E PROTEÇÃO SUAVIZADA DO OTA =================
void inicializarConfiguracaoOTA() {
    ArduinoOTA.setHostname("OCTOPUS_NODE");

    ArduinoOTA.onStart([]() {
        Serial.println(F("\n[OTA] Gravacao remota iniciada! Bloqueando operacoes..."));
        
        // Alivia as travas do Watchdog para tolerar a latência de gravação física na Flash
        esp_task_wdt_config_t twdt_disable_config = {
            .timeout_ms = 30000, // Eleva para 30 segundos
            .idle_core_mask = 0,  // Suspende monitoramento ocioso nos Cores durante o processo
            .trigger_panic = false // Inibe o reset por estouro
        };
        esp_task_wdt_reconfigure(&twdt_disable_config);
        
        Serial.println(F("[WDT] Temporizador suavizado para o processo de Upload."));
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println(F("\n[OTA] SUCESSO! Firmware gravado. Reiniciando processador..."));
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progresso do Upload: %u%%\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Erro encontrado, Codigo: [%u]\n", error);
        
        // Restaura imediatamente os 5 segundos originais se o upload falhar por queda de sinal
        esp_task_wdt_config_t twdt_config = {
            .timeout_ms = 5000,                    
            .idle_core_mask = (1 << 0) | (1 << 1), 
            .trigger_panic = true                  
        };
        esp_task_wdt_reconfigure(&twdt_config);
        Serial.println(F("[WDT] Upload falhou. Watchdog de seguranca restaurado para 5s."));
    });

    ArduinoOTA.begin();
    Serial.println(F("[INIT] Servidor OTA injetado no kernel em segundo plano."));
}
