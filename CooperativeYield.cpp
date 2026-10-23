#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "RadioWatchdog.h"

extern TaskHandle_t TaskRadioHandle;

// O yield fraco do Arduino ESP32 usa apenas vPortYield(). No parser HTTP,
// uma tarefa de maior prioridade continua pronta e pode impedir OTA/IDLE0
// de executar enquanto aguarda o restante de um cabecalho.
// Sobrescreve o hook somente para a tarefa de radio deste projeto.
extern "C" void yield(void) {
    if (TaskRadioHandle != nullptr && !xPortInIsrContext() &&
        xTaskGetCurrentTaskHandle() == TaskRadioHandle) {
        RadioWatchdog::cooperar();
        vTaskDelay(1); // Um tick bloqueado permite executar prioridades menores.
        return;
    }
    vPortYield(); // Preserva o comportamento original nas demais tarefas.
}
