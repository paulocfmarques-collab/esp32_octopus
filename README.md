# Octopus — firmware ESP32

Versão **v1.3.0**. Firmware Arduino ESP32 dual-core, com comandos UDP,
configuração web, relógio NTP, logs SD, diagnóstico de crash, OTA e métricas
Prometheus.

## Acesso

- **Painel web:** `http://IP_DO_OCTOPUS/`, sempre na porta **80**.
- **UDP:** porta configurável, padrão **4210**. Envie `help` para listar comandos.
- **Prometheus:** `http://IP_DO_OCTOPUS:PORTA/metrics`, porta TCP independente,
  padrão **9120**, inicialmente desativado. Ative na aba Configurações.
- **OTA:** hostname `OCTOPUS_NODE`, porta padrão ArduinoOTA **3232**.
- **Sem Wi-Fi salvo:** conecte à rede `OCTOPUS_CONFIG_PORTAL` e abra o IP do AP.

## Configuração Prometheus

Na interface web, abra **Configurações → Prometheus**. Ative o endpoint,
informe o IP/hostname do servidor autorizado (opcional) e a porta do endpoint
**no Octopus**, depois clique em **Salvar Prometheus sem reiniciar**.
Os parâmetros ficam nas Preferences/NVS e são recuperados após reiniciar.
Não é necessário reenviar SSID ou senha para alterar Prometheus.

O Prometheus consulta o dispositivo pelo modelo pull. O campo de servidor
restringe a origem da coleta; não configura envio HTTP para a porta 9090.
Em branco, a coleta é permitida a todos os clientes que alcançarem a porta.
Consulte [PROMETHEUS.md](PROMETHEUS.md) para configurar o servidor e conhecer
as métricas e limitações de hardware.

Todas as métricas incluem `device="MAC_DO_OCTOPUS"`, permitindo selecionar
o equipamento no Grafana pelo mesmo padrão dos demais ESP32. Labels como
`pin`, `file`, `version`, `interface` e `reason` identificam cada série.
O schema comum segue o ESP32C6: reset como valor 1 e código no label, IP
por interface, flash relativa à partição e contadores HTTP separados de UDP.

## Hardware e métricas

Temperatura e umidade de Porto Alegre são consultadas no Open-Meteo por uma
tarefa independente, com cache para não bloquear os scrapes. Os labels seguem
o padrão da Central: `source="open_meteo",location="porto_alegre"` para
temperatura e `location="porto_alegre"` para umidade. O comando UDP `age` e
`esp_weather_data_age_seconds` informam a idade do último sucesso.
Atualiza a cada 15 minutos, tenta novamente após 1 minuto em caso de falha
e omite temperatura/umidade se o cache ultrapassar 30 minutos.

O firmware atual usa SD via VSPI, LEDs nos GPIOs 2 e 15 e botão de reset no
GPIO 0. As leituras digitais exportadas correspondem somente a esses pinos.
Não há sensores locais de energia, bateria, ambiente, relés, ADC ou cliente MQTT
implementados. Essas séries são omitidas até existir integração real,
seguindo o ESP32C6. A temperatura interna da CPU
é exportada separadamente da temperatura ambiente.

Capacidade e ocupação do SD são armazenadas em cache e atualizadas a cada
30 segundos, fora do processamento HTTP. As escritas de log atualizam seus
contadores imediatamente. O firmware não implementa montagem automática
após remoção ou inserção do cartão com o equipamento ligado.

## Logs e diagnóstico

`/log.txt` contém eventos gerais. `/udp_log.txt` registra comandos RX e
respostas TX com horário, uptime e cliente. Os logs rotacionam a 512 KiB.
`crash_info` arquiva no SD o core dump disponível e só apaga o original após
conferir a cópia. Detalhes e comandos em [COMANDOS.md](COMANDOS.md).

Os painéis **Log Records** e **Log Error Records** usam os counters
`sdcard_log_records_total` (registros gravados) e `sdcard_log_errors_total`
(falhas ao gravar registros), incluindo logs gerais e UDP, desde o boot.

## Compilação

Use Arduino IDE com o pacote **ESP32 by Espressif Systems**. O código usa APIs
do Arduino ESP32 3.x e foi preparado para ESP32 clássico com VSPI e dois
núcleos. A placa de referência de compilação é `esp32:esp32:esp32`.
Mantenha uma tabela de partições compatível com OTA e, para captura de crash,
uma partição `coredump` com core dump ELF em flash habilitado no SDK.
Uso de CPU exige `configGENERATE_RUN_TIME_STATS` e `configUSE_TRACE_FACILITY`;
se indisponíveis, `esp_cpu_usage_percent` é omitida.

Após o relatório TASK_WATCHDOG, foram reduzidos pontos de bloqueio no núcleo
de rádio: tamanho da imagem em cache, snapshot SD sem aguardar operações FAT
e descanso mínimo de um tick. O watchdog permanece ativo; a ausência de novos
resets precisa ser verificada na placa. O backtrace exige o ELF da versão que
falhou para determinar a função envolvida.

O diretório `.build/` contém somente configuração local e artefatos de
validação; não é necessário copiá-lo para a placa.
