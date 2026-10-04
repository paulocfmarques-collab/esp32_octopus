# ESP32 Octopus

## Visão geral

Este repositório reúne um conjunto de módulos em C++ para ESP32 com foco em conectividade, diagnóstico, logging, gestão de LEDs e OTA.

## Objetivo

O firmware atua como um controlador de rede e periféricos para dispositivos ESP32, com capacidade de:

- provisionar Wi‑Fi em modo AP/STA;
- receber comandos UDP;
- manter logs em cartão SD;
- reportar diagnósticos do sistema;
- controlar LEDs e reset por botão;
- sincronizar hora por NTP;
- receber atualizações OTA.

## Arquitetura

A arquitetura principal é composta por módulos separados, cada um com responsabilidade bem definida:

- `StorageManager` — acesso ao SD e persistência de logs
- `WifiPortalManager` — gestão de rede, portal de configuração e UDP
- `LedManager` — feedback visual por LEDs
- `TelemetryEngine` — processador de comandos e coleta de dados
- `NTPService` — sincronização de tempo

O fluxo principal está em `Octopus.ino`, que coordena os módulos e os processos em duas tasks do FreeRTOS.

## Módulos e arquivos

- `defines.h` — constantes compartilhadas de GPIO e configuração
- `StorageManager.h` / `StorageManager.cpp` — armazenamento em microSD
- `WifiPortalManager.h` / `WifiPortalManager.cpp` — Wi‑Fi, portal e UDP
- `LedManager.h` / `LedManager.cpp` — LEDs e comportamento visual
- `TelemetryEngine.h` / `TelemetryEngine.cpp` — leitura de status e execução de comandos
- `NTPService.h` / `NTPService.cpp` — hora do sistema via NTP
- `Octopus.ino` — entrypoint e setup do firmware
- `card_wifi.ino` — referência antiga/monolítica

## Riscos e observações

- O portal web e os comandos UDP atualmente não possuem autenticação.
- O uso em produção deve considerar isolação da rede, firewall e validação de entrada.
- O repositorio ainda pode evoluir com testes automatizados e documentação mais detalhada de integração.

## Histórico

### 0.1.0

- implementação inicial de Wi‑Fi, portal, UDP, SD e OTA;
- organização modular do firmware;
- suporte a diagnóstico e logs.

## Licença

MIT. Consulte o arquivo `LICENSE`.
