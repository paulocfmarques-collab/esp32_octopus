# ESP32 Wi-Fi Card

<div align="center">

**A configurable ESP32 network controller with Wi-Fi provisioning, UDP commands, SD-card logging, diagnostics, status LEDs, and OTA updates.**

[![Platform](https://img.shields.io/badge/platform-ESP32-E7352C?logo=espressif&logoColor=white)](https://www.espressif.com/en/products/socs/esp32)
[![Language](https://img.shields.io/badge/language-C%2B%2B-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Framework](https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Protocol](https://img.shields.io/badge/protocol-Wi--Fi%20%7C%20UDP-1677FF?logo=wifi&logoColor=white)](https://www.arduino.cc/reference/en/libraries/wifi/)
[![Storage](https://img.shields.io/badge/storage-microSD-4CAF50)](https://www.sdcard.org/)

</div>

> **Project status:** functional embedded prototype in active modularization. The repository now includes multiple source files for Wi-Fi, storage, telemetry, LED control, NTP, and OTA management, with `Octopus.ino` as the main firmware entry point.

---

## Contents

- [Overview](#overview)
- [Features](#features)
- [System architecture](#system-architecture)
- [Hardware and wiring](#hardware-and-wiring)
- [Provisioning workflow](#provisioning-workflow)
- [UDP command protocol](#udp-command-protocol)
- [Installation and upload](#installation-and-upload)
- [Testing the device](#testing-the-device)
- [Storage and logs](#storage-and-logs)
- [LED and button behavior](#led-and-button-behavior)
- [Source files](#source-files)
- [Project structure](#project-structure)
- [Known limitations and next steps](#known-limitations-and-next-steps)
- [License](#license)

## Overview

**ESP32 Wi-Fi Card** turns an ESP32 board into a small, remotely controlled device that can:

- connect to a previously saved Wi-Fi network;
- expose a temporary configuration portal when credentials are unavailable;
- receive control and diagnostic commands over UDP on port `4210`;
- control the blue status LED remotely;
- read device, memory, flash, network, temperature, MAC, reset, and uptime information;
- persist operational logs and exchange text files through a microSD card;
- synchronize time using NTP and support wireless firmware updates over OTA.

The Wi-Fi credentials are stored in the ESP32 non-volatile storage using the `Preferences` library. If the saved network cannot be used, the device starts an access point named `ESP32_CONFIG` and serves a small configuration page. In the current repository layout, the firmware is split across several source files instead of a single monolithic sketch, which makes the logic easier to maintain and extend.

## Features

| Area | Capability |
| --- | --- |
| Wi-Fi | Station mode with stored credentials and access-point provisioning fallback |
| Configuration | HTTP form at `/` with `POST /salvar` |
| Control | UDP command listener on port `4210` |
| Diagnostics | CPU, RAM, flash, reset reason, uptime, MAC, and network information |
| Storage | Custom SPI microSD initialization and append-only `/log.txt` |
| Feedback | Green heartbeat LED, blue connection/command LED, and reset button |
| Recovery | `RESET_WIFI` command or physical reset button clears credentials and restarts provisioning |
| Time sync | NTP initialization and local time handling |
| Updates | OTA upload support with background task handling |

## System architecture

### Runtime block diagram

```mermaid
flowchart LR
    A[Power on] --> B[Initialize serial and microSD]
    B --> C{Saved Wi-Fi credentials?}
    C -- No --> D[Start AP: ESP32_CONFIG]
    D --> E[HTTP configuration portal]
    E --> F[Save SSID and password]
    F --> G[Restart]
    C -- Yes --> H[Connect in STA mode]
    H -- Connected --> I[Start UDP listener :4210]
    H -- Failed --> D
    I --> J[Receive UDP command]
    J --> K[Execute command]
    K --> L[Send UDP response]
    K --> M[Append event to /log.txt]
    I --> N[Heartbeat and asynchronous LED blink]
    H --> O[Background NTP and OTA tasks]
```

### Firmware responsibilities

```mermaid
mindmap
  root((ESP32 Wi-Fi Card))
    Connectivity
      Wi-Fi station mode
      SoftAP provisioning
      UDP port 4210
      OTA updates
    Control
      LED_ON
      LED_OFF
      LED_PISCA
      LED_BLINK
      RESET_WIFI
    Diagnostics
      TEMP
      CPU
      RAM
      FLASH
      INIT
      UPTIME
      MAC
      NET_INFO
    Storage
      microSD over VSPI
      /log.txt
      /teste.txt
      Serial transfer
      UDP transfer
    Modules
      StorageManager
      WifiPortalManager
      LedManager
      TelemetryEngine
      NTPService
```

## Hardware and wiring

The current firmware defines the following pins. Use a 3.3 V-compatible ESP32 board and a level-shifted microSD interface when required by the module you are using.

### Pin map

| Function | ESP32 GPIO | Firmware symbol | Notes |
| --- | ---: | --- | --- |
| microSD chip select | `13` | `SD_CS` | Custom VSPI chip-select pin |
| microSD MOSI | `23` | `SD_MOSI` | VSPI data out |
| microSD MISO | `19` | `SD_MISO` | VSPI data in |
| microSD clock | `18` | `SD_SCK` | VSPI clock |
| Blue LED | `2` | `LED_AZUL` | Connection and command feedback |
| Green LED | `15` | `LED_VERDE` | Heartbeat, toggles every second |
| Reset/configuration button | `0` | `BOTAO_RESET` | Input pull-up; active LOW |

### Wiring schematic

```mermaid
flowchart TB
    ESP[ESP32 board]
    SD[microSD module]
    BLUE[Blue LED + resistor]
    GREEN[Green LED + resistor]
    BTN[Push button]

    ESP -- GPIO13 / CS --> SD
    ESP -- GPIO23 / MOSI --> SD
    ESP -- GPIO19 / MISO --> SD
    ESP -- GPIO18 / SCK --> SD
    ESP -- GPIO2 --> BLUE
    ESP -- GPIO15 --> GREEN
    ESP -- GPIO0 --> BTN
    BTN --> GND[(GND)]
    SD --> VCC[3.3 V and GND]
```

> **Hardware note:** the repository does not currently include a board photograph, PCB layout, or a specified ESP32 board model. Add a real assembly photo to `docs/images/` when available so the build can be reproduced more reliably.

### Visual pinout sketch

```mermaid
graph LR
    subgraph ESP32["ESP32"]
        G13[GPIO13]
        G23[GPIO23]
        G19[GPIO19]
        G18[GPIO18]
        G2[GPIO2]
        G15[GPIO15]
        G0[GPIO0]
    end

    subgraph MicroSD["Módulo microSD"]
        CS[CS]
        MOSI[MOSI]
        MISO[MISO]
        SCK[SCK]
    end

    subgraph Componentes["Periféricos e Botões"]
        B_LED[Blue LED]
        G_LED[Green LED]
        BTN[Reset / Boot Button]
        GND((GND))
    end

    %% Conexões do microSD
    G13 --- CS
    G23 --- MOSI
    G19 --- MISO
    G18 --- SCK

    %% Conexões dos LEDs
    G2  --- B_LED
    G15 --- G_LED

    %% Conexão do Botão de Reset
    G0  --- BTN
    BTN --- GND

    %% Estilos visuais
    style ESP32 fill:#2c3e50,stroke:#34495e,stroke-width:2px,color:#fff
    style MicroSD fill:#27ae60,stroke:#2ecc71,stroke-width:2px,color:#fff
    style Componentes fill:#7f8c8d,stroke:#95a5a6,stroke-width:2px,color:#fff
    style GND fill:#c0392b,stroke:#e74c3c,color:#fff

```

## Provisioning workflow

1. Flash the sketch and open the serial monitor at `115200` baud.
2. On first boot, or after a failed connection, connect a phone or computer to the Wi-Fi network **`ESP32_CONFIG`**.
3. Open the access-point IP address printed in the serial monitor.
4. Enter the target SSID and password and submit the form.
5. The device stores the credentials in the `wifi` Preferences namespace and restarts.
6. On successful connection, the station IP is printed to serial and UDP listening begins on port `4210`.

To return to provisioning mode, hold the GPIO0 button active LOW while the device is connected, or send `RESET_WIFI` over UDP.

## UDP command protocol

The firmware listens for one text command per UDP datagram and replies to the sender's IP address and source port. Commands are case-sensitive.

### Device control

| Command | Description | Example |
| --- | --- | --- |
| `LED_ON` | Turns the blue LED on continuously. | `LED_ON` |
| `LED_OFF` | Turns the blue LED off. | `LED_OFF` |
| `LED_PISCA:<count>:<ms>` | Performs a finite number of blue LED flashes. | `LED_PISCA:5:250` |
| `LED_BLINK:<ms>` | Starts asynchronous blinking at the selected interval. | `LED_BLINK:500` |
| `RESET_WIFI` | Clears saved Wi-Fi credentials, logs the event, and restarts. | `RESET_WIFI` |

### Diagnostics

| Command | Response |
| --- | --- |
| `TEMP` | ESP32 CPU temperature reading |
| `CPU` | Chip model, revision, core count, CPU frequency, and free heap |
| `RAM` | Free heap, minimum free heap, and maximum allocatable heap |
| `FLASH` | Flash size/speed, sketch size, and free sketch space |
| `INIT` | ESP reset reason |
| `UPTIME` | Milliseconds since boot |
| `MAC` | Wi-Fi MAC address |
| `NET_INFO` | IP, gateway, subnet mask, RSSI, and SSID |

### File operations

| Command | Description |
| --- | --- |
| `TESTE_SERIAL` | Reads `/teste.txt` to the serial monitor. |
| `LOG_SERIAL` | Reads `/log.txt` to the serial monitor. |
| `TESTE_UDP` | Sends `/teste.txt` to the current UDP client. |
| `LOG_UDP` | Sends `/log.txt` to the current UDP client. |
| `DEL_LOG` | Deletes `/log.txt`. |
| `LOG_EXIST` | Reports whether `/log.txt` exists. |

### Minimal UDP test client

Replace `ESP32_IP` with the address printed by the ESP32:

```bash
printf 'NET_INFO' | nc -u -w1 ESP32_IP 4210
printf 'LED_BLINK:250' | nc -u -w1 ESP32_IP 4210
printf 'CPU' | nc -u -w1 ESP32_IP 4210
```

The exact `nc` flags vary between Linux, macOS, and Windows builds. Any UDP client capable of sending plain UTF-8 text to port `4210` can be used.

## Installation and upload

### Requirements

- Arduino IDE 2.x or another ESP32 Arduino-compatible build environment;
- ESP32 board support package installed;
- a USB data cable;
- a microSD card and compatible module wired to the pins above;
- a 2.4 GHz Wi-Fi network for station mode.

### Arduino IDE

1. Install the Espressif ESP32 board package through **Boards Manager**.
2. Open `Octopus.ino` (or `card_wifi.ino` for the legacy monolithic sketch).
3. Select the ESP32 board that matches your hardware and the correct serial port.
4. Insert a FAT/FAT32 microSD card if file features are required.
5. Upload the sketch.
6. Open **Serial Monitor** at `115200` baud.

The sketch uses these headers, which are provided by the ESP32 Arduino core or standard Arduino ecosystem:

- `WiFi.h`
- `WiFiUdp.h`
- `WebServer.h`
- `Preferences.h`
- `esp_system.h`
- `SPI.h`
- `SD.h`
- `ArduinoOTA.h`

## Testing the device

A practical bring-up sequence is:

1. Confirm `Cartão SD montado com sucesso.` appears in the serial monitor.
2. Confirm the `ESP32_CONFIG` access point appears when no credentials are stored.
3. Submit credentials and verify that the device restarts.
4. Confirm the station IP is printed after a successful connection.
5. Send `NET_INFO`, `CPU`, and `UPTIME` over UDP.
6. Send `LED_ON`, `LED_OFF`, and `LED_BLINK:500` and verify blue LED behavior.
7. Send `LOG_EXIST` and `LOG_SERIAL` to validate SD logging.
8. Test `RESET_WIFI` only when you are ready to provision the device again.

## Storage and logs

The SD card is initialized with a custom VSPI instance:

- `SCK`: GPIO18
- `MISO`: GPIO19
- `MOSI`: GPIO23
- `CS`: GPIO13

The firmware appends operational messages to `/log.txt`, including received commands, responses, Wi-Fi reset events, and connection information. The file `/teste.txt` is used by the file-transfer functions and can be read back through UDP or serial commands. The repository's `StorageManager` abstracts this behavior and centralizes SD-card file access.

## LED and button behavior

| Indicator | Normal behavior |
| --- | --- |
| Green LED, GPIO15 | Toggles every 1 second as a heartbeat while the main loop runs |
| Blue LED, GPIO2 | Blinks during Wi-Fi connection attempts; can be controlled over UDP |
| GPIO0 button | When held LOW during an active Wi-Fi connection, clears stored credentials and restarts the configuration portal |

## Source files

The project is organized into a small set of focused modules:

- `defines.h` — shared pin assignments and global configuration constants.
- `StorageManager.h` / `StorageManager.cpp` — SD-card initialization, file creation, logging, and append operations.
- `WifiPortalManager.h` / `WifiPortalManager.cpp` — Wi-Fi connection logic, access-point portal, HTTP configuration page, and UDP listener.
- `LedManager.h` / `LedManager.cpp` — LED state management, blinking, and user feedback routines.
- `TelemetryEngine.h` / `TelemetryEngine.cpp` — command parsing, diagnostics, and device telemetry responses.
- `NTPService.h` / `NTPService.cpp` — NTP synchronization and time handling.
- `Octopus.ino` — main firmware entry point, multi-core task orchestration, and OTA setup.
- `card_wifi.ino` — legacy or reference sketch kept in the repository for comparison.

## Project structure

```mermaid
graph TD
    %% Nós principais / Pastas
    Root["📂 Raiz do Projeto"]
    Docs["📂 docs/"]

    %% Arquivos da Raiz
    Readme["📄 README.md<br><small>Documentação, pinos e protocolos</small>"]
    Defines["📄 defines.h<br><small>Definições de pinos e constantes</small>"]
    Octopus["📄 Octopus.ino<br><small>Firmware principal (Multitask & OTA)</small>"]
    CardWifi["📄 card_wifi.ino<br><small>Legado / Monolítico para referência</small>"]
    License["📄 LICENSE<br><small>Licença do projeto (opcional)</small>"]

    %% Módulos de Código (Agrupados por interface/lógica)
    subgraph Modulos ["Módulos de Código (C++)"]
        direction LR
        subgraph Led ["Gerenciamento de LEDs"]
            LedH["LedManager.h"] <--> LedCpp["LedManager.cpp"]
        end
        
        subgraph Storage ["Cartão SD e Logs"]
            StorageH["StorageManager.h"] <--> StorageCpp["StorageManager.cpp"]
        end
        
        subgraph Wifi ["Portal & Conexão Wi-Fi"]
            WifiH["WifiPortalManager.h"] <--> WifiCpp["WifiPortalManager.cpp"]
        end
        
        subgraph Telemetry ["Diagnósticos & Comandos"]
            TelemetryH["TelemetryEngine.h"] <--> TelemetryCpp["TelemetryEngine.cpp"]
        end
        
        subgraph NTP ["Sincronização de Tempo"]
            NtpH["NTPService.h"] <--> NtpCpp["NTPService.cpp"]
        end
    end

    %% Arquivos da pasta Docs
    DocsInfo["📷 Fotos do hardware e diagramas futuros"]

    %% Conexões da Árvore
    Root --> Readme
    Root --> Defines
    Root --> Octopus
    Root --> CardWifi
    Root --> Modulos
    Root --> License
    Root --> Docs
    Docs --> DocsInfo

    %% Estilização
    classDef folder fill:#2c3e50,stroke:#34495e,stroke-width:2px,color:#fff;
    classDef file fill:#ecf0f1,stroke:#bdc3c7,stroke-width:1px,color:#2c3e50;
    classDef module fill:#f39c12,stroke:#d35400,stroke-width:1px,color:#fff;
    
    class Root,Docs folder;
    class Readme,Defines,Octopus,CardWifi,License,DocsInfo file;
    class LedH,LedCpp,StorageH,StorageCpp,WifiH,WifiCpp,TelemetryH,TelemetryCpp,NtpH,NtpCpp module;

```

## Known limitations and next steps

- **No authentication:** the HTTP provisioning portal and UDP command protocol do not authenticate clients. Use this firmware only on a trusted network or add authentication before deployment.
- **Single-sketch architecture:** split networking, storage, provisioning, command parsing, and hardware control into modules as the project grows.
- **No automated test suite:** add a host-side command parser test strategy and hardware-in-the-loop smoke tests.
- **Board-specific assumptions:** document the exact ESP32 board, SD module, LED polarity, and electrical protection used by the final assembly.
- **Protocol framing:** commands are currently plain text datagrams with no request ID or structured error format. A versioned JSON or compact binary protocol could improve interoperability.
- **Documentation photos:** add assembly, wiring, and enclosure photographs under `docs/images/` and link them here once the physical prototype is finalized.

## License

No license has been declared yet. Until a license is added to the repository, all rights remain with the copyright holder. Add a `LICENSE` file before distributing or reusing the firmware publicly.

---

<div align="center">

Made for ESP32 prototyping, remote hardware control, and modular firmware development.

</div>
