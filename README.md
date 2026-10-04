# 🐙 ESP32 Octopus

> Firmware modular e extensível para ESP32 com provisioning Wi‑Fi, portal de configuração, diagnósticos em tempo real e atualização OTA.

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Arduino](https://img.shields.io/badge/Arduino-ESP32-00979D?logo=arduino)](https://www.espressif.com/en/products/socs/esp32)
[![Status](https://img.shields.io/badge/Status-Functional%20Prototype-brightgreen)]()

[Visão Geral](#visão-geral) • [Instalação](#-instalação) • [Documentação](#-documentação) • [Contribuição](#-contribuição) • [Licença](#licença)

</div>

---

## 📋 Visão Geral

ESP32 Octopus transforma seu microcontrolador ESP32 em um **controlador de rede e automação** completo e profissional:

| Recurso | Descrição |
|---------|-----------|
| 🌐 **Wi‑Fi Inteligente** | Modo estação com fallback automático para AP de configuração |
| 🔧 **Portal Web** | Interface local para gerenciar credenciais de rede |
| 📡 **Protocolo UDP** | Escuta comandos na porta 4210 com respostas em tempo real |
| 💾 **Armazenamento** | Logs persistidos em cartão microSD com rastreio completo |
| 🔍 **Diagnósticos** | Hardware, rede, CPU, memória e temperatura monitorados |
| 💡 **Indicadores LED** | Status visual e botão de reset integrados |
| ⏰ **Sincronização NTP** | Relógio preciso via protocolo NTP |
| 🔄 **OTA Updates** | Atualizar firmware sem cabo USB |

---

## 🚀 Começa Rápido

### Requisitos

- Arduino IDE 2.x+ ou plataforma compatível
- [ESP32 Board Support](https://github.com/espressif/arduino-esp32) instalado
- Cabo USB de dados
- Cartão microSD (compatível)
- Rede Wi‑Fi 2.4 GHz

### Instalação (3 passos)

```bash
# 1. Clone o repositório
git clone https://github.com/paulocfmarques-collab/esp32_octopus.git
cd esp32_octopus

# 2. Abra em Arduino IDE
# → Arquivo > Abrir > Octopus.ino

# 3. Configure e envie
# → Selecione Ferramentas > Placa > ESP32
# → Conecte via USB
# → Pressione Upload ⬆️
```

**Dica:** Ajuste os pinos de hardware em `defines.h` se necessário para seu setup.

---

## 📚 Documentação

### Estrutura do Projeto

```
esp32_octopus/
├── 📄 README.md                      # Este arquivo
├── 📄 LICENSE                        # MIT License
├── 📄 CHANGELOG.md                   # Histórico de versões
├── 📄 CONTRIBUTING.md                # Guia de contribuição
├── 📄 SECURITY.md                    # Política de segurança
│
├── 🔧 defines.h                      # Pinos e constantes globais
├── 🔧 Octopus.ino                    # Firmware principal + orquestração
├── 🔧 card_wifi.ino                  # Referência legada
│
├── 📦 StorageManager.*               # Gerenciamento SD + logs
├── 📦 WifiPortalManager.*            # Wi‑Fi, portal HTTP + UDP
├── 📦 LedManager.*                   # Controle de LEDs
├── 📦 TelemetryEngine.*              # Comandos + diagnósticos
├── 📦 NTPService.*                   # Sincronização de hora
│
└── 📋 docs/
    └── architecture.md               # Diagramas e fluxo
```

### Módulos Principais

| Módulo | Responsabilidade |
|--------|------------------|
| **StorageManager** | Inicialização SD, escrita/leitura de logs, persistência de dados |
| **WifiPortalManager** | Conexão Wi‑Fi, portal de configuração, HTTP e UDP listener |
| **LedManager** | Controle de LEDs de status, feedback visual e animações |
| **TelemetryEngine** | Processamento de comandos UDP, respostas e diagnósticos |
| **NTPService** | Sincronização de relógio via NTP, timestamp preciso |
| **Octopus.ino** | Orquestração FreeRTOS, OTA, setup/loop e lifecycle |

### Comandos UDP Suportados

Envie comandos de texto via UDP na porta `4210`:

```
NET_INFO      → Informações de rede (IP, SSID, etc)
CPU           → Uso de CPU
TEMP          → Temperatura do chip
RAM           → Uso de memória RAM
FLASH         → Espaço de armazenamento flash
UPTIME        → Tempo desde o último boot
LED_ON        → Acender LED
LED_OFF       → Apagar LED
LED_BLINK:500 → Piscar LED a cada 500ms
RESET_WIFI    → Resetar configurações Wi‑Fi
```

**Exemplo de cliente UDP (Python):**
```python
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(b"NET_INFO", ("192.168.1.100", 4210))
print(sock.recv(1024).decode())
```

---

## ⚙️ Configuração Avançada

### Pinagem (defines.h)

Ajuste estes valores conforme sua placa:

```cpp
#define SD_CS_PIN        5      // Chip Select do cartão SD
#define LED_PIN          2      // Pino do LED de status
#define BUTTON_PIN       0      // Pino do botão de reset
#define TEMP_SENSOR_PIN  34     // Pino do sensor de temperatura (ADC)
```

### WiFi Portal

1. Se o ESP32 não conseguir conectar a nenhuma rede conhecida, ativa um **Access Point (AP)**
2. Conecte a rede `ESP32_Octopus` (sem senha)
3. Acesse `http://192.168.4.1` no navegador
4. Configure SSID e senha
5. Reinicie o dispositivo

### Logging e Diagnósticos

Todos os eventos são salvos em `logs.txt` no cartão microSD:

```
[2026-10-04 14:32:15] Wi-Fi conectado: rede1 (192.168.1.100)
[2026-10-04 14:32:20] NTP sincronizado
[2026-10-04 14:33:45] Comando recebido: NET_INFO
[2026-10-04 14:35:10] LED ligado por 1000ms
```

---

## 🔐 Segurança

⚠️ **Status Atual:** Arquitetura sem autenticação. Para **produção**, implemente:

- ✅ Use redes **Wi‑Fi confiáveis** e criptografadas
- ✅ **Bloqueie a porta 4210** no roteador/firewall
- ✅ Implemente **autenticação UDP** (token/HMAC)
- ✅ **Altere credenciais padrão** regularmente
- ✅ **Revise logs** periodicamente
- ✅ Use HTTPS no portal web (adicionar certificado)

Veja [SECURITY.md](SECURITY.md) para mais detalhes.

---

## 🔄 Atualização OTA

O firmware suporta atualizações **over-the-air** sem precisar de cabo USB:

1. Prepare seu novo `.bin` (Sketch > Exportar binário compilado)
2. Abra o portal web (192.168.x.x)
3. Clique em "Update Firmware"
4. Selecione o arquivo `.bin`
5. Aguarde 30-60 segundos enquanto reinicia

---

## 🛠️ Desenvolvimento

### Stack Tecnológico

- **Linguagem:** Arduino C/C++ (com extensões ESP32)
- **RTOS:** FreeRTOS (nativo do ESP32)
- **Rede:** lwIP stack, mDNS
- **Armazenamento:** SPIFFS (flash) + SDMMC (cartão)
- **Sincronização:** NTP (SNTP)

### Como Contribuir

1. Faça um **fork** do repositório
2. Crie uma branch: `git checkout -b feat/sua-feature`
3. Commit com mensagens claras: `git commit -m "feat: descrição"`
4. Push: `git push origin feat/sua-feature`
5. Abra um **Pull Request** descrevendo sua mudança

Consulte [CONTRIBUTING.md](CONTRIBUTING.md) para padrões de código e fluxo de PR.

### Testes Locais

```bash
# Build no Arduino IDE
# → Sketch > Compilar (Ctrl+R)

# Monitor serial (para debug)
# → Ferramentas > Monitor Serial (115200 baud)

# Teste UDP
python3 -c "import socket; s=socket.socket(); \
s.sendto(b'NET_INFO', ('192.168.1.x', 4210)); \
print(s.recv(1024))"
```

---

## 📈 Roadmap

- [ ] Autenticação HMAC para UDP
- [ ] Web dashboard com gráficos em tempo real
- [ ] Suporte a MQTT para integração IoT
- [ ] Sistema de plugins/extensões
- [ ] Testes unitários automatizados
- [ ] Documentação em inglês
- [ ] Exemplo de integração com Home Assistant

---

## 📞 Suporte

| Canal | Link |
|-------|------|
| **Issues** | [GitHub Issues](../../issues) |
| **Discussions** | [GitHub Discussions](../../discussions) |
| **Docs** | [docs/](docs/) |
| **Changelog** | [CHANGELOG.md](CHANGELOG.md) |

---

## 📄 Licença

Este projeto está licenciado sob a [MIT License](LICENSE).

Você é livre para:
- ✅ Usar em projetos comerciais e pessoais
- ✅ Modificar e distribuir
- ✅ Usar em código privado

Contanto que inclua a licença original.

---

## 🎓 Notas Finais

ESP32 Octopus foi desenvolvido como **base modular para prototipagem, aprendizado e extensão de firmware embarcado**. O código evolui continuamente e é estruturado para ser:

- 🧩 **Modular** — módulos independentes e bem definidos
- 📖 **Documentado** — comentários claros e README rico
- 🔧 **Extensível** — fácil adicionar novas funcionalidades
- 🏗️ **Escalável** — arquitetura preparada para crescimento

---

<div align="center">

**Feito com ❤️ por [Paulo Marques](https://github.com/paulocfmarques-collab)**

⭐ Se este projeto foi útil, considere deixar uma estrela!

</div>
