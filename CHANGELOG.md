# Changelog — Octopus

## 2026-10-19 — v1.3.0 — Clima e métricas de registros

- Adicionada consulta Open-Meteo para Porto Alegre em tarefa independente,
  cache de 15 minutos, nova tentativa após 1 minuto em falhas e expiração
  das leituras após 30 minutos sem sucesso.
- Exportadas `sensor_temperature_celsius` com `source="open_meteo"` e
  `location="porto_alegre"`, e `sensor_humidity_percent` com
  `location="porto_alegre"`, no mesmo padrão da Central.
- Incluídos `age` no UDP/help e `esp_weather_data_age_seconds` para a idade
  desde a última consulta bem-sucedida, sem acesso de rede no scrape.
- Adicionados `sdcard_log_records_total` e `sdcard_log_errors_total`, usando
  os contadores reais de registros gravados e falhas de append já existentes.
- Documentadas as consultas Grafana com `and on(instance, job)` e o modelo
  `Octopus`. Compilação e validação em hardware ficam com o usuário.

## 2026-10-31 — Reset dos contadores NVS

- Adicionado `reset_counters` ao comando UDP `help`; zera apenas boot e
  watchdog em `metric_counts`, mantendo a configuração Prometheus.
- Atualizados o guia de comandos e o catálogo consolidado.

Consolidado em **18/10/2026** a partir dos fontes, documentação e histórico
Git. A identificação atual do firmware é **v1.2.0**. Não foram criadas novas
versões ou tags por esta revisão documental.

## Revisão documental — 2026-10-18

- Incluído `name` no catálogo UDP; retorna `Octopus` para identificar a placa.
- Acrescentado o catálogo completo de comandos UDP ao
  [COMANDOS.md](COMANDOS.md), distinguindo a serial de logs de um console.
- Documentados limite/truncamento de entrada, streaming de arquivos,
  comandos não implementados e efeitos persistentes.
- Criado este histórico e incluído o projeto no
  [changelog geral](../../ESP32_Documents/changelog.md).
- Não houve mudança de firmware, compilação ou gravação nesta revisão.

## 2026-10-17 — Prometheus e disponibilidade

Referências: `117f642` e `a105a50`.

- Exportador Prometheus e configurações persistentes na web: ativação,
  servidor autorizado por IP/DNS e porta própria, padrão 9120.
- Coleta inicialmente desativada; painel na porta 80, OTA na 3232, sem
  permitir essas portas para o servidor de métricas.
- Métricas de identificação, Wi-Fi, CPU/tarefas, memória, Flash, GPIO,
  HTTP/UDP e SD; fontes sem integração omitidas.
- Padronizados MAC STA no label `device`, IP por interface, reset como
  gauge com label e contadores persistentes de boot/watchdog.
- Cache SD de capacidade/ocupação atualizado fora do scrape, com idade,
  disponibilidade e contadores de escrita.
- Respostas 403 detalham origem recebida/autorizada; falhas DNS retornam
  indisponibilidade em vez de liberar a origem.
- Após relato de TASK_WATCHDOG, reduzidos bloqueios por leitura de tamanho
  de imagem e snapshots SD; núcleo de rádio descansa ao menos um tick.
  Watchdog permanece ativo e a estabilidade precisa ser validada na placa.

## 2026-10-15 — crash e log UDP

Referência: `9121dcb`.

- `crash_info` consulta core dump ELF e salva relatório separado no SD.
- Cópia reaberta e conferida antes de apagar o dump interno; falhas preservam
  o original. Sem NTP, nomes usam `SEM_NTP` e uptime.
- Histórico RX/TX em `/udp_log.txt`, com origem, tamanho, resultado,
  data/hora e uptime; rotação em 512 KiB para `/udp_log_old.txt`.
- Streaming limitado ao tamanho inicial do arquivo, evitando leitura sem
  fim quando a própria resposta gera novos registros no log.

## Evolução anterior

- `38f2e77` (25/09): fontes e testes SD do projeto Octopus.
- `1eefcc2` (29/09): restauração da variante legada `card_wifi`.
- `eb0568c`, `93fc496`, `b33dd25` e `2dcbb82` (03–12/10): revisões dos
  módulos de rede, telemetria, armazenamento, LED e aplicação.

As variantes `Original/test_sd_card` e `card_wifi` permanecem separadas do
firmware modular atual. Backups não são tratados como releases aprovadas.
Detalhes de linhas/arquivos de cada etapa estão disponíveis no Git; catálogo
de métricas e configuração em [PROMETHEUS.md](PROMETHEUS.md).
