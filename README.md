# ESP32-AP_CONTROL PANEL

**Version:** v0.0.1  
**Author:** Omkar Prayag  
**Email:** [omkar@circuitveda.com](mailto:omkar@circuitveda.com)

## 📋 Project Description

This firmware hosts a responsive web dashboard on an ESP32 that allows real-time control and configuration of:

- ✅ One onboard **LED**
- ✅ Two **Relay channels**
- ✅ Dynamic **GPIO mapping**
- ✅ Live internal **temperature monitoring**
- ✅ Toggle controls with **live feedback**
- ✅ No page reloads thanks to AJAX integration

Designed for smart home, automation, and remote control use cases.

---

## 🔧 Features

- 🔌 **SoftAP Mode**: Connect directly to the ESP32 (SSID: `SmartHome`, Password: `12345678`)
- ⚙️ **Dynamic GPIO Configuration**: Re-assign GPIOs for LED, Relay 1, and Relay 2 from the web UI.
- 🔄 **Live Toggle Buttons**: Change device states with instant status feedback.
- 🌡️ **Real-Time Temperature**: Displays internal ESP32 temperature (approximate).
- 🌐 **Pure Web-Based Interface**: No mobile app or external server needed.

---

## 📷 Web Dashboard Preview

| Control Panel | GPIO Configuration |
|---------------|---------------------|
| ![UI Preview](preview-ui.png) | ![GPIO Form](preview-gpio.png) |

---

## 🚀 Getting Started

### Prerequisites

- ESP32 board (e.g., NodeMCU-32S)
- PlatformIO or Arduino IDE
- Optional: USB to Serial debugger

### Setup (PlatformIO)

1. Clone this repository:
   ```bash
   git clone https://github.com/esp32-ap_controlpanel.git
   cd esp32-ap_controlpanel
