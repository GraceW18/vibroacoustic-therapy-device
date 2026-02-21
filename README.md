# VibroJaw — Web App (Flask / IntelliJ IDEA Community Edition)
**Team:** ChocoChip Circuits | MedTech Hack 2025-26 | Health Monitoring Track

## Problem Statement
IP (discuss issues of current technologies + target audience)

## Relevant Background Research
To be added (provide file path of research findings)

## Our Solution
IP (discuss features of our product)

## Technologies Used
- Hardware:
- Backend:
- Frontend:
- Database:
- Etc:

## Setup

```bash
# 1. Install Python 3.10+ if you haven't already
# 2. Open a terminal in this folder and run:

pip install flask flask-socketio

```

## Run the server

```bash
python app.py
```

Then open **http://localhost:5000** in your browser.

## Pages

| URL | Description |
|-----|-------------|
| `/` | Landing page |
| `/dashboard` | Live monitoring dashboard |
| `/about` | About the device & research |
| `/api-docs` | Interactive REST API reference |

## Demo (no hardware needed)

1. Open `/dashboard`
2. Choose a scenario (Mild / Moderate / Severe) in the **Demo Simulator** section
3. Click **▶ Start** — the chart and event table will update live via Socket.IO
4. Adjust sliders and click **↑ Push Config to Device** — config saves instantly
5. Click **■ Stop** to stop the simulator

## ESP32 Integration

1. Open `vibrojaw_esp32/vibrojaw_esp32.ino` in Arduino IDE
2. Set your WiFi credentials and your **computer's local IP** as `SERVER_BASE`
   - On Windows: run `ipconfig` → look for IPv4 Address (e.g. `192.168.1.42`)
   - On Mac/Linux: run `ifconfig` → look for `inet` under your WiFi interface
   - Set: `const char* SERVER_BASE = "http://192.168.1.42:5000";`
3. Install Arduino libraries via Library Manager:
   - `ArduinoJson` by Benoit Blanchon
   - `LiquidCrystal_I2C` by Frank de Brabander
   - Board: `esp32` by Espressif (via Boards Manager)
4. Flash to ESP32 S3 — it connects to WiFi, starts monitoring,
   and data appears on the dashboard in real time.

## Team Members
- Yadiel Narvaez Hernandez
- Madhumitha Rangaraj
- Reyna Torrado-Rivera
- Grace Wang
- Ka Yi Zheng

## License
To be configured

## Acknowledgments
- MedTech Hack Organizers
- Georgia Tech

## File Structure

```
vibrojaw_flask/
├── app.py                  ← Flask server (run this)
├── requirements.txt        ← pip dependencies
├── README.md
├── templates/
│   ├── index.html          ← Landing page
│   ├── dashboard.html      ← Live dashboard
│   ├── about.html          ← About page
│   └── api_docs.html       ← Interactive API reference
├── static/
│   ├── css/style.css       ← All styles
│   └── js/dashboard.js     ← Socket.IO client + Chart.js
└── ../vibrojaw_esp32/
    └── vibrojaw_esp32.ino  ← Arduino sketch for ESP32 S3
```
