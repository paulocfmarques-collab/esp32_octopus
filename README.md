# ESP32 Octopus

Projeto de firmware para ESP32 com provisionamento Wi‑Fi, portal de configuração, UDP, armazenamento em microSD, diagnósticos, LEDs, sincronização NTP e OTA.

## Visão geral

O ESP32 Octopus transforma um ESP32 em um controlador de rede e automação com:

- conexão Wi‑Fi em modo estação com fallback para AP de configuração;
- portal web local para salvar credenciais de rede;
- escuta de comandos UDP na porta 4210;
- diagnóstico de hardware e rede;
- armazenamento e rastreio de logs em microSD;
- LEDs de status e botão de reset;
- sincronização de relógio via NTP;
- atualização de firmware via OTA.

## Status do projeto

O repositório está em estágio funcional de protótipo embarcado, com arquitetura modular e foco em manutenção e extensibilidade.

## Estrutura atual do repositório

```text
esp32_octopus/
├── README.md
├── LICENSE
├── CHANGELOG.md
├── CONTRIBUTING.md
├── SECURITY.md
├── .gitignore
├── defines.h
├── StorageManager.h
├── StorageManager.cpp
├── WifiPortalManager.h
├── WifiPortalManager.cpp
├── LedManager.h
├── LedManager.cpp
├── TelemetryEngine.h
├── TelemetryEngine.cpp
├── NTPService.h
├── NTPService.cpp
├── Octopus.ino
├── card_wifi.ino
├── docs/
│   └── architecture.md
└── ...
```

## Módulos principais

- `defines.h` — pinos e constantes globais
- `StorageManager.*` — inicialização do cartucho SD, logs e manipulação de arquivos
- `WifiPortalManager.*` — Wi‑Fi, portal de configuração, HTTP e UDP
- `LedManager.*` — controle de LEDs e feedback visual
- `TelemetryEngine.*` — comandos, diagnósticos e respostas ao cliente UDP
- `NTPService.*` — sincronização de hora via NTP
- `Octopus.ino` — ponto de entrada do firmware e orquestração FreeRTOS/OTA
- `card_wifi.ino` — esboço legado/monolítico para referência

## Configuração e execução

### Requisitos

- Arduino IDE 2.x ou ambiente compatível com ESP32
- suporte para placas ESP32 instalado
- cabo USB de dados
- módulo microSD compatível
- rede Wi‑Fi 2.4 GHz

### Como compilar

1. Instale o suporte para ESP32 no Arduino IDE.
2. Abra `Octopus.ino`.
3. Escolha a placa ESP32 correta.
4. Conecte a placa por USB.
5. Ajuste os pinos de hardware em `defines.h` se necessário.
6. Compile e faça upload.

### Porta e protocolo

O firmware escuta comandos UDP na porta `4210` usando texto simples.
Alguns comandos documentados incluem:

- `NET_INFO`
- `CPU`
- `TEMP`
- `RAM`
- `FLASH`
- `UPTIME`
- `LED_ON`
- `LED_OFF`
- `LED_BLINK:500`
- `RESET_WIFI`

## Segurança

A arquitetura atual ainda não implementa autenticação para portal ou comandos UDP. Para ambiente de produção, recomenda-se:

- usar rede confiável;
- bloquear portas em roteadores/firewalls;
- adicionar autenticação antes de uso em produção;
- revisar logs e credenciais armazenadas.

## Documentação adicional

- [docs/architecture.md](docs/architecture.md)

## Contribuição

Consulte [CONTRIBUTING.md](CONTRIBUTING.md) para regras de contribuição, padrão de commits e fluxo de PR.

## Licença

Este projeto está licenciado sob a [MIT License](LICENSE).

## Notas

O projeto foi estruturado para ser usado como base de protótipo, aprendizagem e extensão de firmware embarcado em ESP32. O código está em evolução modular e pode ser ampliado com testes automatizados, monitoramento, autenticação e APIs estruturadas.
