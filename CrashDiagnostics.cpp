#include "CrashDiagnostics.h"
#include <esp_system.h>
#include <esp_core_dump.h>
#include <time.h>

String CrashDiagnostics::motivoReset()
{
    switch (esp_reset_reason())
    {
    case ESP_RST_POWERON:
        return "POWER_ON";
    case ESP_RST_EXT:
        return "PIN_RESET";
    case ESP_RST_SW:
        return "SOFTWARE_RESET";
    case ESP_RST_PANIC:
        return "PANIC";
    case ESP_RST_INT_WDT:
        return "INTERRUPT_WATCHDOG";
    case ESP_RST_TASK_WDT:
        return "TASK_WATCHDOG";
    case ESP_RST_WDT:
        return "WATCHDOG";
    case ESP_RST_DEEPSLEEP:
        return "DEEP_SLEEP";
    case ESP_RST_BROWNOUT:
        return "BROWNOUT";
    case ESP_RST_SDIO:
        return "SDIO";
    default:
        return "OUTRO (" + String((int)esp_reset_reason()) + ")";
    }
}

String CrashDiagnostics::lerArquivarEApagar(StorageManager& storage)
{
    String resultado = "Ultimo reset: " + motivoReset() + "\n";
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
    esp_err_t erro = esp_core_dump_image_check();
    if (erro != ESP_OK)
        return resultado + "Sem core dump valido: " + esp_err_to_name(erro) + "\n";
    char motivo[200] = {};
    if (esp_core_dump_get_panic_reason(motivo, sizeof(motivo)) == ESP_OK)
        resultado += "Panic salvo: " + String(motivo) + "\n";
    esp_core_dump_summary_t resumo = {};
    erro = esp_core_dump_get_summary(&resumo);
    if (erro != ESP_OK)
        return resultado + "Falha ao ler resumo: " + esp_err_to_name(erro) + "\n";
    char endereco[32];
    snprintf(endereco, sizeof(endereco), "0x%08lx", (unsigned long)resumo.exc_pc);
    resultado += "Tarefa: " + String(resumo.exc_task) + "\nPC: " + endereco + "\nBacktrace:";
    for (uint32_t i = 0; i < resumo.exc_bt_info.depth &&
                         i < sizeof(resumo.exc_bt_info.bt) / sizeof(resumo.exc_bt_info.bt[0]);
         ++i)
    {
        snprintf(endereco, sizeof(endereco), " 0x%08lx", (unsigned long)resumo.exc_bt_info.bt[i]);
        resultado += endereco;
    }
    resultado += "\nBacktrace corrompido: " + String(resumo.exc_bt_info.corrupted ? "SIM" : "NAO");
    resultado +=
        "\nELF SHA256: " + String(reinterpret_cast<const char*>(resumo.app_elf_sha256)) + "\n";
    if (!storage.isAtivo())
        return resultado + "SD indisponivel. Registro interno preservado.\n";
    struct tm hora;
    char data[32];
    if (getLocalTime(&hora, 0))
        strftime(data, sizeof(data), "%Y%m%d_%H%M%S", &hora);
    else
        snprintf(data, sizeof(data), "SEM_NTP_%lu", millis());
    String caminho;
    for (unsigned i = 0; i < 1000; ++i)
    {
        caminho = "/crash_" + String(data) + (i ? "_" + String(i) : String()) + ".txt";
        if (!storage.arquivoExiste(caminho.c_str()))
            break;
        caminho = "";
    }
    if (!caminho.length())
        return resultado + "Nomes esgotados. Registro interno preservado.\n";
    String conteudo =
        "Capturado em: " + String(data) + " | uptime=" + String(millis()) + "ms\n" + resultado;
    File arquivo = storage.abrirArquivo(caminho.c_str(), FILE_WRITE);
    if (!arquivo)
        return resultado + "Falha ao criar arquivo. Registro interno preservado.\n";
    size_t gravado = arquivo.print(conteudo);
    arquivo.flush();
    arquivo.close();
    arquivo = storage.abrirArquivo(caminho.c_str(), FILE_READ);
    bool ok = arquivo && gravado == conteudo.length() && arquivo.size() == conteudo.length();
    for (size_t i = 0; ok && i < conteudo.length(); ++i)
        ok = arquivo.read() == static_cast<uint8_t>(conteudo[i]);
    if (arquivo)
        arquivo.close();
    if (!ok)
        return resultado + "Falha ao conferir arquivo. Registro interno preservado.\n";
    resultado += "Crash salvo no SD: " + caminho + "\n";
    erro = esp_core_dump_image_erase();
    resultado += erro == ESP_OK
                     ? "Registro de crash apagado apos leitura.\n"
                     : "Falha ao apagar registro de crash: " + String(esp_err_to_name(erro)) + "\n";
#else
    resultado += "Core dump nao habilitado neste firmware.\n";
#endif
    return resultado;
}
