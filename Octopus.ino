#include "defines.h"
#include "StorageManager.h"
#include "WifiPortalManager.h"
#include "LedManager.h"
#include "TelemetryEngine.h"
#include "NTPService.h"
#include <ArduinoOTA.h> // Biblioteca do OTA reintroduzida com segurança
#include <esp_task_wdt.h>

// Instanciação dos Gerenciadores Estáticos
SPIClass spiSD(VSPI);
StorageManager storage(Config::PIN_SD_CS, spiSD); 
WifiPortalManager network(Config::PIN_LED_BLUE);
LedManager leds(Config::PIN_LED_BLUE, Config::PIN_LED_GRN);
NTPService ntp; 
TelemetryEngine telemetry(storage, network, leds);

// Handlers do FreeRTOS
TaskHandle_t TaskRadioHandle = NULL;
TaskHandle_t TaskProcessorHandle = NULL;
TaskHandle_t TaskOtaHandle = NULL; // Novo Handler para a linha de execução do OTA
QueueHandle_t filaComandosUDP = NULL;

struct ComandoUDP {
    char texto[255];
};

void TaskRadioCore0(void *pvParameters);
void TaskProcessorCore1(void *pvParameters);
void TaskOtaCore0(void *pvParameters); // Protótipo da nova Task de segundo plano
void inicializarConfiguracaoOTA();

void setup() {
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    Serial.println("\n======================================================================");
    Serial.println("[SYSTEM] INICIALIZANDO FIRMWARE MULTITAREFA PARALELO + OTA (OCTOPUS)");
    Serial.println("======================================================================");

    leds.begin();
    
    spiSD.begin(Config::PIN_SD_SCK, Config::PIN_SD_MISO, Config::PIN_SD_MOSI, Config::PIN_SD_CS);
    if (storage.inicializar()) {
        Serial.println(F("[STORAGE] Armazenamento inicializado com sucesso."));
    } else {
        Serial.println(F("[STORAGE] Alerta: Operando sem cartao SD local."));
    }

    if (network.conectar()) {
        String ipLocal = WiFi.localIP().toString();
        Serial.println(F("\n----------------------------------------------------------------------"));
        Serial.print(F("[WIFI] Conectado com sucesso! Endereco IP do Dispositivo: "));
        Serial.println(ipLocal);
        Serial.println(F("----------------------------------------------------------------------\n"));

        if (storage.isAtivo()) {
            storage.gravarLog("[SISTEMA] Dispositivo online no IP: " + ipLocal);
        }
        
        ntp.inicializar();
        network.iniciarUDP();
        network.iniciarWebServerLocal();
        
        // Configura as diretrizes e rotinas de feedback do OTA
        inicializarConfiguracaoOTA();
    } else {
        Serial.println(F("[SYSTEM] Rede falhou. Disparando Portal de Emergencia..."));
        network.iniciarPortal();
    }

    filaComandosUDP = xQueueCreate(Config::TAMANHO_FILA_UDP, sizeof(ComandoUDP));
    if (filaComandosUDP == NULL) {
        Serial.println(F("[CRITICAL] Falha ao criar a fila do FreeRTOS. Resetando..."));
        ESP.restart();
    }

    // Configuração do Watchdog NATIVO do ESP32 para 5 segundos
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 5000,                    
        .idle_core_mask = (1 << 0) | (1 << 1), 
        .trigger_panic = true                  
    };
    esp_task_wdt_reconfigure(&twdt_config);

    // ─── CRIAÇÃO E ALOCAÇÃO DAS TASKS EM DUAL-CORE ───
    Serial.println(F("[RTOS] Distribuindo tarefas paralelas entre os nucleos..."));

    // Task de Coleta de Rádio - Prioridade 3 (Alta) no Core 0
    xTaskCreatePinnedToCore(TaskRadioCore0, "TaskRadioCore0", 4096, NULL, 3, &TaskRadioHandle, 0);

    // Task de Processamento Lógico - Prioridade 2 (Média) no Core 1
    xTaskCreatePinnedToCore(TaskProcessorCore1, "TaskProcessorCore1", 8192, NULL, 2, &TaskProcessorHandle, 1);

    // NOVA TASK: Monitoramento de Gravação Sem Fio (OTA) - Prioridade 1 (Baixa) no Core 0
    // Ela roda em background no Core 0 aproveitando os momentos ociosos do rádio Wi-Fi
    if (network.isConnected()) {
        xTaskCreatePinnedToCore(TaskOtaCore0, "TaskOtaCore0", 4096, NULL, 1, &TaskOtaHandle, 0);
    }

    Serial.println(F("[SYSTEM] Inicializacao multitarefa com OTA concluida com sucesso."));
    Serial.println(F("======================================================================\n"));
}

void loop() {
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
                ComandoUDP novoComando;
                snprintf(novoComando.texto, sizeof(novoComando.texto), "%s", msgUdpRecebida.c_str());
                xQueueSend(filaComandosUDP, &novoComando, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2)); 
    }
}

// ================= TAREFA DE PROCESSAMENTO (NÚCLEO 1) =================
void TaskProcessorCore1(void *pvParameters) {
    (void) pvParameters;
    esp_task_wdt_user_handle_t wdt_handle = NULL;
    esp_task_wdt_add_user("WdtProcessorCore1", &wdt_handle);

    ComandoUDP comandoRecebido;

    for (;;) {
        if (wdt_handle != NULL) esp_task_wdt_reset_user(wdt_handle);

        leds.atualizar();

        if (xQueueReceive(filaComandosUDP, &comandoRecebido, pdMS_TO_TICKS(10)) == pdTRUE) {
            telemetry.executarComando(String(comandoRecebido.texto));
        }

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
                network.clearConfig();
                leds.piscarSincrono(10, 50);
                ESP.restart();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ================= NOVA TAREFA EXCLUSIVA DE BACKGROUND: OTA (NÚCLEO 0) =================
void TaskOtaCore0(void *pvParameters) {
    (void) pvParameters;
    Serial.println(F("[RTOS_BOOT] TaskOtaCore0 em background ativa no Core 0."));
    
    esp_task_wdt_user_handle_t wdt_handle = NULL;
    esp_task_wdt_add_user("WdtOtaCore0", &wdt_handle);

    for (;;) {
        if (wdt_handle != NULL) esp_task_wdt_reset_user(wdt_handle);

        // Varre de forma isolada as requisições de upload sem fios
        ArduinoOTA.handle();
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Delay maior (50ms) para rodar de forma leve e invisível
    }
}

// ================= CONFIGURAÇÃO VISUAL DAS ROTINAS DO OTA =================
void inicializarConfiguracaoOTA() {
    ArduinoOTA.setHostname("OCTOPUS_NODE");

    ArduinoOTA.onStart([]() {
        Serial.println(F("\n[OTA] Gravacao remota iniciada! Bloqueando operacoes..."));
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println(F("\n[OTA] SUCESSO! Firmware gravado. Reiniciando processador..."));
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progresso do Upload: %u%%\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Erro encontrado, Codigo: [%u]\n", error);
    });

    ArduinoOTA.begin();
    Serial.println(F("[INIT] Servidor OTA injetado no kernel em segundo plano."));
}
