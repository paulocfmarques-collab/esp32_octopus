# Changelog — Octopus

## 2026-10-22 — v1.4.3 — Interface moderna e destino SQL fixo

- Novos cartões e seções para status e configuração, campos alinhados, botões
  uniformes, mensagens com destaque e layout adaptado a celular.
- Estilos aplicados a todos os inputs e botões, inclusive elementos sem type
  explícito; fontes locais e recursos no firmware, sem dependências externas.
- Banco fixo LogsRede e tabela public.netscan; campo de banco removido e
  parâmetros enviados pelo cliente não podem mudar esse destino.
- Migração das prefs antigas preserva servidor/credenciais e adota LogsRede.
- Sem compilação/gravação nem validação de conexão no banco real.

## 2026-10-22 — v1.4.2 — Páginas separadas e confirmação PostgreSQL

- Status em / e configuração em /config, com navegação e HTML separado;
  templates em flash compartilhados sem montar o documento inteiro na RAM.
- Confirmação de senha alterada/mantida/removida após persistir e reler as prefs,
  sem expor a senha no status; resultado anterior de login invalidado ao salvar.
- Botão Salvar e testar login: autenticação e leitura assíncronas da tabela fixa
  public.netscan, com resultado no painel e log. Sem escrita durante o teste.
- Nome do banco identificado como distinto da tabela; tabela segue hardcoded.
- Mantido LED azul piscando a cada 250 ms durante netscan.
- Sem compilação/gravação nem validação no PostgreSQL real.

## 2026-10-22 — v1.4.1 — Data e hora nos eventos do log

- Prefixo automático de data/hora local e uptime em cada novo evento de log.txt,
  incluindo erros de acesso ao PostgreSQL; mensagens multilinha usam o mesmo instante.
- Consulta não bloqueante ao relógio; SEM_NTP e uptime enquanto a data não é válida.
- Preservados formato do log UDP, rotação, contadores e registros antigos.
- Sem compilação nem gravação do firmware.

## 2026-10-22 — v1.4.0 — Netscan periódico e PostgreSQL

- Agenda automática padrão de 15 minutos, ajustável na web e por comandos UDP,
  com persistência em prefs, sem sobrepor varreduras.
- PostgreSQL configurável na web: servidor 192.168.0.20, porta 5432, banco,
  usuário, senha e envio habilitado. Ativação depende de configurar o banco.
- Cliente PostgreSQL v3 TCP local, autenticação SCRAM-SHA-256/MD5 e esperas limitadas;
  PBKDF2 cooperativo para permitir execução das tarefas ociosas.
- UPSERT transacional no public.netscan com timestamp do banco; script de criação
  em sql/netscan.sql. MAC em minúsculas no SQL e no JSON.
- Falhas detalhadas de acesso/SQL no log SD, Serial e estado, sem senha; JSON
  continua disponível quando o banco falha. Não há fila histórica de reenvio.
- Sem compilação/gravação do firmware nem validação com o banco real.

## 2026-10-21 — v1.3.5 — Transferência de arquivos SD pelo painel

- Formulários web para download por caminho e upload para a raiz do SD.
- Download com Content-Disposition e tamanho inicial fixo, envio em blocos e
  espera de rede limitada; checkpoints apenas ao progredir na transferência.
- Upload temporário, verificação de tamanho, troca com backup e tratamento de
  envio interrompido; recusado durante netscan.
- Nomes e caminhos validados antes de abrir arquivos. Porta web continua 80.
- Sem compilação nem gravação do firmware.

## 2026-10-21 — v1.3.4 — Netscan

- Comandos `netscan` e `netscan_status`, com tarefa independente de prioridade baixa.
- LED azul pisca a cada 250 ms durante a operação e retoma o estado manual ao finalizar.
- Descoberta IPv4 local por ping/ARP, até 1022 hosts, RTT ICMP e MAC.
- JSON `/netscan.json` indexado por MAC, escrita temporária e backup do resultado anterior.
- Cache ARP sem ping é identificado como candidato, sem inventar tempo de resposta.
- Sem compilação ou gravação de firmware; validação no equipamento fica pendente.

## 2026-10-21 — v1.3.3 — Esperas do rádio e diagnóstico por etapa

- ELF de SHA256 `05d15c5be0b1d21002a98059bdd68540ca967603fbb71fcb5d60a55c2d0b97b6`
  confirmado; backtrace traduzido somente para espera ociosa de IDLE1.
  Não há evidência suficiente no resumo para atribuir a origem exata do bloqueio.
- Adicionado `build_opt.h` com limites HTTP de linha 750 ms e cabeçalhos
  1500 ms, aplicados à biblioteca WebServer pelo recipe Arduino ESP32.
- Rádio tem checkpoints por operação e alimentação cooperativa em HTTP
  limitada aos primeiros 2 s; watchdog global continua em 5 s, sem alimentar
  outra tarefa ou relaxar panic/idle. Mantido descanso de um tick no yield.
- Esperas pelos mutex UDP e log limitadas a 100 ms. Contenção que impede
  gravar um registro entra nos contadores de erro, sem fingir sucesso.
- Última etapa do rádio preservada em RTC_NOINIT e incluída no `crash_info`
  após watchdog. Sem gravações NVS em cada iteração.
- Compilação/reconstrução da biblioteca e teste em hardware ficam com o usuário.

## 2026-10-21 — Formato parseável do help

- Padronizada a resposta de `help` com o cabeçalho `COMANDOS` e um comando
  por linha, sem descrições nem instruções de transporte misturadas.
- Atualizado o guia de comandos.

## 2026-10-20 — v1.3.2 — Cooperação do parser HTTP com OTA

- ELF localizado em `AppData/Local/arduino/sketches`, SHA256
  `baea6c00fb9e4cda8c4c0497d09f76651d0fe1c8697de43bb4ce41e0c96fd49e`,
  compatível com o prefixo `baea6c00f` do relatório.
- Backtrace traduzido: rádio em `readLineWithLimit`/`yield` durante parsing
  HTTP, enquanto o watchdog expirou para `TaskOtaCore0` de menor prioridade.
- Adicionado `CooperativeYield.cpp`: override local do hook fraco Arduino,
  bloqueando um tick somente para o rádio; demais tarefas usam `vPortYield`.
- Preservados watchdog de 5 segundos, callbacks OTA, prioridades e limites
  HTTP. Nenhum watchdog de outra tarefa é alimentado pelo hook.
- Compilação e teste de carga/OTA na placa ficam com o usuário.

## 2026-10-20 — Correção de panic no boot

- Backtrace decodificado com o ELF de SHA256 `2bed5e647`: travamento em
  `ledcAttach`, chamado por `LedManager::begin`, antes da inicialização NVS.
- Substituído PWM de 1 Hz por `esp_timer` com callback em tarefa para
  alternar o GPIO azul a cada 500 ms, sem usar o driver LEDC.
- Temporizador parado e removido na transição para operação; sincronização
  impede que um callback pendente reacenda o azul.
- `reset_counters` continua sem solicitar reinício. Compilação e teste
  físico da correção ficam com o usuário.

## 2026-10-20 — Watchdog durante OTA

- Tarefa OTA passa a alimentar seu watchdog nos callbacks de início,
  progresso, fim e erro, pois `ArduinoOTA.handle()` executa o upload
  sincronamente e não retorna ao laço durante a transferência.
- Removida reconfiguração global que relaxava timeout, monitoramento
  das tarefas idle e panic durante o OTA.
- Percentual calculado sem divisão por zero e impresso somente quando muda.
- Compilação e teste de upload/interrupção ficam com o usuário.

## 2026-10-20 — LEDs de inicialização e operação

- Azul pisca a cada 500 ms durante o boot usando temporizador, sem
  depender das esperas de inicialização de SD, Wi-Fi ou NTP.
- Ao iniciar a tarefa de operação, azul apaga e verde alterna a cada
  1 segundo. Comandos manuais do azul preservados.
- Removidas escritas concorrentes do Wi-Fi no LED de boot.
- Compilação e teste na placa ficam com o usuário.

## 2026-10-19 — v1.3.1 — Diagnóstico do transporte UDP

- Adicionada `esp_udp_listener_up` a partir do resultado real de abertura
  da porta UDP e da criação do mutex do socket.
- Incluído `esp_udp_packets_received_total` antes da leitura/enfileiramento,
  separado dos comandos efetivamente concluídos.
- Incluídos `esp_udp_replies_sent_total` e `esp_udp_send_errors_total` por
  tentativa de resposta, exigindo escrita completa e `endPacket()` aceito.
- Falhas de envio continuam no agregado `esp_udp_errors_total`, sem contagem
  duplicada por tentativa. Counters somente em RAM, reiniciados no boot.
- Atualizados os guias com semântica e consultas; compilação fica com o usuário.

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
