# 🌿 Estação de Monitoramento Ambiental Híbrida (BitDogLab)

![Status do Projeto](https://img.shields.io/badge/Status-Concluído-success?style=for-the-badge)
![Linguagem](https://img.shields.io/badge/C%2B%2B-Pico%20SDK-blue?style=for-the-badge&logo=cplusplus)
![OS](https://img.shields.io/badge/FreeRTOS-Kernel-orange?style=for-the-badge)
![Backend](https://img.shields.io/badge/Python-Flask-yellow?style=for-the-badge&logo=python)
![Hardware](https://img.shields.io/badge/Hardware-BitDogLab-red?style=for-the-badge)

> **Uma solução robusta de IoT industrial para monitoramento ambiental em tempo real, combinando a potência do Raspberry Pi Pico W com a eficiência do FreeRTOS.**

---

## 📋 Sobre o Projeto

Este projeto consiste em uma **Estação de Monitoramento Ambiental** desenvolvida para a placa de prototipagem educacional **BitDogLab**. O sistema coleta dados de múltiplos sensores (temperatura, umidade, luminosidade, som) e os processa utilizando um kernel de tempo real (**FreeRTOS**) para garantir estabilidade e preempção.

Os dados são exibidos localmente em um display OLED e matriz de LEDs, e transmitidos via Wi-Fi para um servidor web **Python (Flask)**, onde podem ser visualizados em um dashboard interativo.

---

## ✨ Funcionalidades Principais

- **⚡ Arquitetura Multitarefa (FreeRTOS):**
  - Tarefas independentes para leitura de sensores, gerenciamento de rede e controle de interface.
  - Uso de Filas (Queues) e Semáforos para comunicação segura entre tarefas.
- **📡 Conectividade Wi-Fi:**
  - Stack TCP/IP lwIP integrado.
  - Envio de dados via requisições HTTP POST JSON.
- **📊 Dashboard Web Interativo:**
  - Servidor Backend em Python com Flask.
  - Gráficos dinâmicos (Chart.js) para análise histórica.
- **🔊 Interface Local Rica:**
  - Feedback sonoro (Buzzer PWM).
  - Feedback visual (Matriz de LEDs RGB 5x5 e Display OLED SSD1306).
- **🛡️ Watchdog Timer:** Mecanismo de segurança para reinício automático em caso de falhas.

---

## 🛠️ Hardware Utilizado

| Componente              | Função                                    |
| :---------------------- | :---------------------------------------- |
| **Raspberry Pi Pico W** | Microcontrolador Dual-Core ARM Cortex-M0+ |
| **BitDogLab**           | Placa de Expansão Didática                |
| **Sensor DHT11/22**     | Temperatura e Umidade                     |
| **Microfone**           | Captura de intensidade sonora             |
| **LDR (Fotossensor)**   | Monitoramento de luminosidade             |
| **OLED SSD1306**        | Exibição de status local                  |
| **Matriz RGB WS2812B**  | Alertas visuais                           |

---

## 🚀 Como Executar

### Pré-requisitos

- [VS Code](https://code.visualstudio.com/) com extensão Raspberry Pi Pico.
- [Python 3.10+](https://www.python.org/).
- SDK do Pico C/C++ configurado.

### 1. Firmware (Pico W)

1.  Abra a pasta do firmware no VS Code.
2.  Configure o CMake e selecione o kit `GCC for arm-none-eabi`.
3.  Compile o projeto (Build).
4.  Copie o arquivo `.uf2` gerado para o Pico W mantendo o botão `BOOTSEL` pressionado.

### 2. Servidor (Dashboard)

1.  Navegue até a pasta `server`.
2.  Instale as dependências:
    ```bash
    pip install -r requirements.txt
    ```
3.  Inicie o servidor:
    ```bash
    python server.py
    ```
4.  Acesse `http://localhost:5000` no seu navegador.

---

## 📸 Galeria

|                                  Dashboard Web                                  |                                Dispositivo Físico                                 |
| :-----------------------------------------------------------------------------: | :-------------------------------------------------------------------------------: |
| ![Dashboard Screenshot](https://via.placeholder.com/400x200?text=Dashboard+Web) | ![BitDogLab Device](https://via.placeholder.com/400x200?text=Foto+do+Dispositivo) |

---

## 📄 Licença

Distribuído sob a licença MIT. Veja `LICENSE` para mais informações.

---

<div align="center">
  <sub>Desenvolvido com 💙 para o curso de Sistemas Embarcados</sub>
</div>
