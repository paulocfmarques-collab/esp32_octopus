#include "defines.h"
#include "StorageManager.h"
#include "WifiPortalManager.h"
#include "LedManager.h"
#include "TelemetryEngine.h"
#include "NTPService.h"
#include "PrometheusMetrics.h"
#include "ClimaService.h"
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include "RadioWatchdog.h"
#include "NetScan.h"

// Instanciação e Alocação dos Gerenciadores Estáticos do Sistema
SPIClass spiSD(VSPI);
StorageManager storage(Config::PIN_SD_CS, spiSD); 
WifiPortalManager network(Config::PIN_LED_BLUE);
LedManager leds(Config::PIN_LED_BLUE, Config::PIN_LED_GRN);
NTPService ntp; 
TelemetryEngine telemetry(storage, network, leds);
PrometheusMetrics prometheus(storage);
ClimaService clima(storage);

// Handlers do Sistema Multitarefa FreeRTOS
TaskHandle_t TaskRadioHandle = NULL;
TaskHandle_t TaskProcessorHandle = NULL;
TaskHandle_t TaskOtaHandle = NULL;
QueueHandle_t filaComandosUDP = NULL;

struct ComandoUDP {
    char texto[256];
    uint32_t ip;
    uint16_t porta;
};

// Protótipos das Tasks e Rotinas do Kernel
void TaskRadioCore0(void *pvParameters);
void TaskProcessorCore1(void *pvParameters);
void TaskOtaCore0(void *pvParameters);
void inicializarConfiguracaoOTA();

void setup() {
    Serial.begin(115200);
    RadioWatchdog::registrarBoot();
    leds.begin();
    prometheus.inicializar();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    Serial.println("\n======================================================================");
    Serial.println("[SYSTEM] INICIALIZANDO FIRMWARE MULTITAREFA PARALELO + OTA (OCTOPUS)");
    Serial.println("======================================================================");

    pinMode(Config::PIN_BTN_RST, INPUT_PULLUP);
    
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
    NetScan::inicializar(storage);
    Serial.println(F("[RTOS] Distribuindo tarefas paralelas entre os nucleos..."));

    // Task de Coleta e Rádio - Prioridade 3 (Alta) alocada no Core 0
    xTaskCreatePinnedToCore(TaskRadioCore0, "TaskRadioCore0", 4096, NULL, 3, &TaskRadioHandle, 0);

    // Task de Processamento Lógico e Interpretador - Prioridade 2 (Média) alocada no Core 1
    xTaskCreatePinnedToCore(TaskProcessorCore1, "TaskProcessorCore1", 8192, NULL, 2, &TaskProcessorHandle, 1);

    // Task de Gravação Sem Fio (OTA) - Roda em background aproveitando os momentos ociosos do Core 0
    if (network.isConnected()) {
        xTaskCreatePinnedToCore(TaskOtaCore0, "TaskOtaCore0", 4096, NULL, 1, &TaskOtaHandle, 0);
    }

    clima.iniciar();
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
    RadioWatchdog::iniciar();

    for (;;) {
        RadioWatchdog::checkpoint();

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
                novoComando.ip = (uint32_t)network.getUdpRemoteIP();
                novoComando.porta = network.getUdpRemotePort();
                snprintf(novoComando.texto, sizeof(novoComando.texto), "%s", msgUdpRecebida.c_str());
                if (xQueueSend(filaComandosUDP, &novoComando, 0) != pdTRUE) {
                    prometheus.registrarErroUDP();
                    RadioWatchdog::Escopo etapa(RadioWatchdog::FILA_CHEIA_LOG);
                    storage.gravarLogUDP("[ERRO] Fila UDP cheia; comando RX descartado: " + msgUdpRecebida);
                }
            }
        }
        // Mesmo com tick maior que 2 ms, bloqueia ao menos um tick para OTA/IDLE0.
        const TickType_t pausaRadio = pdMS_TO_TICKS(2);
        vTaskDelay(pausaRadio ? pausaRadio : 1);
    }
}

// ================= TAREFA DE PROCESSAMENTO LÓGICO (NÚCLEO 1) =================
void TaskProcessorCore1(void *pvParameters) {
    (void) pvParameters;
    leds.finalizarBoot();
    esp_task_wdt_user_handle_t wdt_handle = NULL;
    esp_task_wdt_add_user("WdtProcessorCore1", &wdt_handle);

    ComandoUDP comandoRecebido;

    for (;;) {
        if (wdt_handle != NULL) esp_task_wdt_reset_user(wdt_handle);

        // Processa estados assíncronos e blinks dos LEDs
        leds.atualizar();
        storage.atualizarMetricas();

        // Verifica se há pacotes enviados pelo Core 0 prontos para execução
        if (xQueueReceive(filaComandosUDP, &comandoRecebido, pdMS_TO_TICKS(10)) == pdTRUE) {
            network.definirDestinoResposta(IPAddress(comandoRecebido.ip), comandoRecebido.porta);
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
    
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    for (;;) {
        ESP_ERROR_CHECK(esp_task_wdt_reset());

        // Upload sincrono: os callbacks tambem alimentam o watchdog desta tarefa.
        ArduinoOTA.handle();
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Delay estendido para não impactar o tráfego do rádio
    }
}

// ================= CONFIGURACAO OTA =================
void inicializarConfiguracaoOTA() {
    ArduinoOTA.setHostname("OCTOPUS_NODE");

    ArduinoOTA.onStart([]() {
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        Serial.println(F("\n[OTA] Gravacao remota iniciada. Watchdog ativo."));
    });
    
    ArduinoOTA.onEnd([]() {
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        Serial.println(F("\n[OTA] SUCESSO! Firmware gravado. Reiniciando processador..."));
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        static unsigned int ultimoPercentual = 101;
        unsigned int percentual = total ? uint64_t(progress) * 100 / total : 0;
        if (progress == 0 || percentual != ultimoPercentual) {
            ultimoPercentual = percentual;
            Serial.printf("[OTA] Progresso do Upload: %u%%\r", percentual);
        }
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        Serial.printf("[OTA] Erro encontrado, Codigo: [%u]\n", error);
    });

    ArduinoOTA.begin();
    Serial.println(F("[INIT] Servidor OTA injetado no kernel em segundo plano."));
}
