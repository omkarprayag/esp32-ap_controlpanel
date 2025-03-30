# ESP32-AP CONTROL PANEL

**Version:** v0.1.0  
**Author:** Omkar Prayag  
**Email:** [omkar@circuitveda.com](mailto:omkar@circuitveda.com)

---

## 📋 Project Description

This firmware runs a **dual-mode ESP32 web server** (Access Point + Station) with a built-in interactive dashboard for:

- ✅ Controlling one onboard **LED**
- ✅ Managing two **relay outputs**
- ✅ Real-time **internal temperature monitoring**
- ✅ Responsive web interface with **live chart**
- ✅ Data **export to CSV**
- ✅ Works in both **offline AP mode** and **online STA mode**

Designed for smart home, automation, and educational use cases.

---

## 🔧 Features

- 📡 **Dual Wi-Fi Mode**: ESP32 runs in both AP (192.168.1.1) and STA (connects to router) modes simultaneously.
- 💡 **GPIO Control**: Toggle onboard LED (GPIO 2) and two relays (e.g., GPIO 5).
- 🌡️ **Temperature Monitoring**: Internal sensor (not highly accurate) with real-time graph (Chart.js).
- ⚙️ **Live Dashboard**: JavaScript-powered UI with AJAX-based updates — no page reloads!
- 📈 **Chart Controls**:
  - Reset temperature graph
  - Toggle between auto and fixed Y-axis
  - Export recent data as `.csv` (client-side)
- 🔌 **Self-Hosted**: No cloud or internet dependency.

---

## 🖥️ Web Dashboard Preview

| Control Panel | Temperature Chart |
|---------------|--------------------|
| ![UI Preview](preview-ui.png) | ![Graph Preview](preview-graph.png) |

---

## 🚀 Getting Started

### 🔧 Prerequisites

- ESP32 Development Board (e.g., NodeMCU-32S, ESP32 DevKit v1)
- Arduino IDE or PlatformIO
- USB cable
- Serial Monitor (115200 baud)
- Optional: Wi-Fi access for Station mode testing

---

### ⚙️ Uploading the Code (Arduino IDE)

1. **Clone this repo** or download the `.zip`:
   ```bash
   git clone https://github.com/esp32-ap_controlpanel.git
   cd esp32-ap_controlpanel
