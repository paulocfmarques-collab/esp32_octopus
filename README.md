# Octopus — firmware ESP32

Versão **v1.4.3**. Firmware Arduino ESP32 dual-core, com comandos UDP,
configuração web, relógio NTP, logs SD, diagnóstico de crash, OTA e métricas
Prometheus.

Duas páginas na porta 80: **Status** em `/` e **Configuração** em `/config`.
Na configuração, **Salvar e testar login** verifica as credenciais persistidas
e a leitura da tabela fixa `public.netscan` em segundo plano. O painel confirma
se a senha foi alterada, mantida ou removida; informa se existe senha salva,
sem exibi-la. Banco fixo **LogsRede**, tabela fixa **public.netscan**; nenhum
desses nomes precisa ser preenchido. Configurações antigas preservam servidor
e credenciais, adotando LogsRede automaticamente ao iniciar.

Interface com cartões, formulários em colunas, botões padronizados, mensagens
de sucesso/erro destacadas e adaptação para celular, sem fontes ou recursos externos.

Cada evento em `/log.txt` inclui data/hora local no fuso configurado e uptime.
Antes da sincronização do relógio, usa `[SEM_NTP]` e uptime, sem inventar uma data.

Netscan automático a cada **15 minutos** por padrão, com intervalo de 1 a 1440
minutos ajustável na web ou por `netscan_interval:15`. Use `netscan_auto:on` ou
`netscan_auto:off` para ativar/desativar. Configurações são persistidas nas prefs.
PostgreSQL configurável no painel (padrão **192.168.0.20:5432**, envio inicialmente
desativado até configurar as credenciais e habilitar o envio). Resultados usam MAC em minúsculas,
UPSERT por MAC e `CURRENT_TIMESTAMP`. Falhas são registradas no `/log.txt` e na
Serial. Consulte [POSTGRESQL.md](POSTGRESQL.md) para preparar o banco.

O painel na porta 80 tem a seção **Arquivos do cartão SD**: informe um caminho
para baixar, ou selecione um arquivo do computador para enviar à raiz do SD.
Exemplo de download direto: `http://IP_DO_OCTOPUS/sd/download?arquivo=/netscan.json`.
O upload mantém o nome escolhido e substitui um destino existente após receber
e verificar o tamanho completo. Aguarde o `netscan` terminar antes de enviar.

`netscan` procura equipamentos na sub-rede Wi-Fi em segundo plano e salva
`/netscan.json` no SD, com chaves MAC, IP e tempo de resposta ICMP. Consulte
`netscan_status` para acompanhar e `sd_read:/netscan.json` para ler o resultado.
Durante a varredura, o LED azul pisca a cada 250 ms e depois retoma o estado anterior.
Requer SD e Wi-Fi; limite de 1022 hosts. Entradas apenas ARP são candidatas,
sem confirmação de disponibilidade. Veja os detalhes em [COMANDOS.md](COMANDOS.md).

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

O diagnóstico UDP inclui `esp_udp_listener_up` (abertura da porta),
`esp_udp_packets_received_total` (datagramas recebidos),
`esp_udp_replies_sent_total` (respostas completas aceitas pela pilha local) e
`esp_udp_send_errors_total` (tentativas de envio que falharam). Os counters
reiniciam no boot; envio aceito não comprova entrega no cliente remoto.

## Hardware e métricas

O LED azul (GPIO 2) pisca durante o boot, alternando a cada 500 ms,
por `esp_timer` em contexto de tarefa, sem PWM/LEDC, inclusive enquanto
SD, Wi-Fi e NTP estão inicializando. O LED verde
(GPIO 15) permanece apagado nesse período. Ao iniciar a tarefa de operação,
o azul apaga e o verde acende, passando a alternar a cada 1 segundo como
heartbeat. Isso também indica operação do portal de configuração, não
necessariamente conexão à internet. Os comandos manuais do LED azul
continuam disponíveis após o boot.

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

O upload ArduinoOTA é síncrono: seus callbacks de início, progresso, fim e
erro alimentam o watchdog da própria tarefa OTA, além da alimentação no
laço de espera. A proteção mantém o timeout de 5 segundos e os dois núcleos
monitorados, sem desabilitar panic nem alterar globalmente o watchdog
durante o upload. Um upload longo com progresso não deve ser confundido
com uma tarefa travada; confirme o comportamento em hardware, inclusive
após interrupção do envio.

O crash de 20/10/2026 com SHA `baea6c00f` foi decodificado no ELF correspondente:
o rádio esperava uma linha HTTP no `WebServer`, enquanto o watchdog da OTA
expirava. O `yield()` padrão do SDK mantém o rádio pronto e não garante execução
da OTA de menor prioridade. `CooperativeYield.cpp` substitui o hook fraco:
somente na tarefa de rádio, `yield()` bloqueia por um tick. As demais tarefas
mantêm o comportamento original; watchdog, limites HTTP e callbacks OTA seguem
ativos. Validar na placa com painel, scrapes e clientes HTTP lentos.

Na v1.3.3, `build_opt.h` limita linhas HTTP a 750 ms e a fase de cabeçalhos
a 1500 ms, usando as opções de compilação do Arduino ESP32 3.3.12. Mantenha
esse arquivo junto do `.ino`; ele contém flags, não deve ser incluído com
`#include`. Os limites também precisam ser aplicados à biblioteca WebServer;
faça uma reconstrução completa se a IDE reutilizar seus objetos antigos.

Durante HTTP cooperativo, somente o próprio rádio alimenta seu watchdog,
por no máximo dois segundos por operação. Depois disso a alimentação para,
mantendo o watchdog de cinco segundos capaz de detectar uma espera travada.
Não há alimentação cooperativa durante DNS, espera de mutex ou escrita no SD.
Mutex UDP e aquisição do mutex de log têm espera de até 100 ms. Se o log
não conseguir o mutex nesse prazo, o registro não é gravado e a falha entra
nos contadores de erro de log, em vez de prender a tarefa indefinidamente.

O `crash_info` inclui a última etapa do rádio preservada em RTC (HTTP painel,
HTTP métricas, DNS, recepção UDP ou log SD). Isso auxilia o diagnóstico quando
o resumo do core dump mostra apenas uma tarefa idle. O marcador começa a ser
útil após gravar esta versão; ele não reconstrói a etapa de firmwares antigos.

O diretório `.build/` contém somente configuração local e artefatos de
validação; não é necessário copiá-lo para a placa.
