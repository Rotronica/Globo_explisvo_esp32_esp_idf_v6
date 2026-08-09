# Controlador de Globos Explosivos

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/Framework-ESP--IDF-brightgreen)](https://docs.espressif.com/projects/esp-idf/)
[![MCU](https://img.shields.io/badge/MCU-ESP32--C3-orange)](https://www.espressif.com/en/products/socs/esp32-c3)
[![BLE Stack](https://img.shields.io/badge/BLE-NimBLE-yellow)](https://github.com/apache/mynewt-nimble)
[![License](https://img.shields.io/badge/License-MIT-green)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-0.1.0-blue)](https://github.com/yourusername/control-globos)
[![Status](https://img.shields.io/badge/status-Development-red)]()

## 📋 Descripción

Sistema de control BLE para activación remota de filamento de nicromio mediante PWM, diseñado para aplicaciones de globos explosivos. Utiliza MOSFET IRL530N con etapa driver en configuración push-pull (2N3906/2N3904).

## 🔧 Hardware

- **Microcontrolador**: ESP32-C3 Super Mini (compatible con ESP32 38 pines)
- **Mosfet**: IRL530N con driver 2N3906/2N3904
- **Carga**: Filamento de nicromio
- **Alimentación**: 2x18650 (8.4V) con step-down a 5V

## 📱 Servicios BLE

| UUID | Característica | Función |
|------|---------------|---------|
| `1234` | Servicio | Servicio principal de control |
| `5678` | CMD | Comandos ON/OFF (0x00=OFF, 0x22=ON) |
| `9ABC` | PWM | Control de potencia 0-100% |

## Dependencias y Agradecimientos

Este proyecto utiliza las siguientes librerías de código abierto:

- **[esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp)** - Un wrapper C++ moderno para el stack BLE NimBLE. Licencia: MIT.
- **[ESP-IDF](https://github.com/espressif/esp-idf)** - El framework de desarrollo oficial para el ESP32. Licencia: Apache 2.0.

Agradecemos a los desarrolladores de estas librerías por su excelente trabajo.

## 🚀 Instalación

```bash
# Clonar el repositorio
git clone https://github.com/yourusername/control-globos.git

# Configurar el target
idf.py set-target esp32c3

# Compilar y flashear
idf.py build flash monitor
```

### 📱 Uso con nRF Connect
1. Abre nRF Connect en tu celular
2. Escanea y conecta a "Control-Globos"
3. Escribe en la característica CMD (5678):
    - 0x00 = Apagar sistema
    - 0x22 = Encender sistema (usando última potencia)
4. Escribe en la característica PWM (9ABC):
    - Valor de 0 a 100 para ajustar potencia

### 📄 Licencia
```
MIT License

Copyright (c) 2026 Rodrigo Calle Condori

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### 👤 Autor
Rodrigo Calle Condori
GitHub: @Rotronica
Estado: 🟡 En desarrollo - Pruebas con nRF Connect