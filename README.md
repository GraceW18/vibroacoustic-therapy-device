# VibroJaw — Web App (Flask / Community Edition Compatible)

**Team:** ChocoChip Circuits | MedTech Hack 2025 | Health Monitoring Track

---

## Setup (one time)

```bash
# 1. Install Python 3.10+ if you haven't already
# 2. Open a terminal in this folder and run:

pip install flask flask-socketio

# That's it. No IntelliJ plugins needed.
```

---

## Run the server

```bash
python app.py
```

Then open **http://localhost:5000** in your browser.

---

## Pages

| URL | Description |
|-----|-------------|
| `/` | Landing page |
| `/dashboard` | Live monitoring dashboard |
| `/about` | About the device & research |
| `/api-docs` | Interactive REST API reference |

---

## Demo (no hardware needed)

1. Open `/dashboard`
2. Choose a scenario (Mild / Moderate / Severe) in the **Demo Simulator** section
3. Click **▶ Start** — the chart and event table will update live via Socket.IO
4. Adjust sliders and click **↑ Push Config to Device** — config saves instantly
5. Click **■ Stop** to stop the simulator

---

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

---

## Health Status Thresholds

| Status | Events per minute |
|--------|-------------------|
| 🟢 Calm | < 3 EPM |
| 🟡 Elevated | 3–8 EPM |
| 🔴 Concerning | > 8 EPM |

---

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
