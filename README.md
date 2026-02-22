# VibroJaw — Web App + Arduino Code
Wearable vibroacoustic therapy for Temporomandibular Joint (TMJ) Disorder: detection and treatment in one device! <br>
**Team:** ChocoChip Circuits | MedTech Hack 2025-26 | Health Monitoring Track | Georgia Tech

Live Web-App: https://vibroacoustic-therapy-device-production.up.railway.app/dashboard

## Problem Statement
Temporomandibular Joint Disorder (TMD) affects an estimated **11-12 million adults** in the United States, causing pain and dysfunction in the jaw joint and surrounding masticatory muscles. Its causes are diverse and often unclear (could be due to stress, genetics, bruxism, and clenching), making it difficult to treat with a single approach.

The most common form of TMD is **pain in the masticatory muscles**, the muscles responsible for chewing. Current clinical approaches fall short in two key ways:

- **Lab-based devices are intrusive *and* inaccessible**: existing research prototypes are intraoral (placed inside the mouth), complex to fit, and not designed for universal use outside a clincial setting
- **No current market solution combines detection and treatment**: dental guards physically prevent clenching but provide no biofeedback; Botox injections ($300-$500/session) temporarily paralyze the masseter but offer no real-time awareness

## Relevant Background Research
Vibroacoustic therapy applied to the jaw has peer-reviewed clincial support:

- A randomized, double-blind placebo-controlled study of **32 TMD patients** found local vibratory stimulation significantly **decreased TMJ pain** (Serritella et al., 2020)
- Vibration has been shown to **decrease activity in masticatory muscles**, directly targeting the root cause of the most common TMD symptom (Maejima et al., 2024)

For the full literature review, see Research.md!

## Our Solution
**VibroJaw** is a non-intrusive wearable headband that detects masticatory muscle contraction and delivers immediate vibrotactile biofeedback to relive TMJ tension. No clinic visit required!

**How It Works**:
1. Detect: a force-based strain sensor on the temporalis muscle registers contraction during a TMD episode
2. Classify: The ESP32 applies a frequency-band filter and rolling average to distinguish clenching from normal facial movement
3. Respond: An ERM vibration motor delivers targeted vibroacoustic therapy within 200 ms, relieving muscle tension

## Technologies Used
- Hardware: ESP32 S3 DevKitC, force-based strain/piezo sensor, ERM vibration motor (DC 3V 12000rpm)
- Firmware: Arduino (C++)
- Backend: Python 3, Flask, Flask-SocketIO
- Frontend: HTML/CSS/Javascript, Chart.js
- Deployment: Railway (HTTPS)
- WebSocket for client-server communication

## Setup

```bash
# 1. Install Python 3.10+ if you haven't already
# 2. Open a terminal in this folder (after cloning the repo) and run:

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
The dashboard includes a built-in simulator that runs the full system:

1. Open `/dashboard`
2. In the **Demo Simulator** section, choose a scenario:
   - Mild: infrequent, low-intensity clenches
   - Moderate: typical TMD pattern
   - Severe: frequent, high-intensity clenches
4. Click **▶ Start**: events begin streaming to the chart and table
5. Adjust sliders and click **↑ Push Config to Device** — config saves instantly
6. Click **■ Stop** to end the simulation

## ESP32 Integration

1. Open `vibrojaw_esp32/vibrojaw_esp32.ino` in Arduino IDE
2. Set your WiFi credentials and your **computer's local IP** as `SERVER_BASE`
   - On Windows: run `ipconfig` → look for IPv4 Address (e.g. `192.168.1.42`)
   - On Mac/Linux: run `ifconfig` → look for `inet` under your WiFi interface
   - Set: `const char* SERVER_BASE = "http://192.168.1.42:5000";`
3. Install Arduino libraries via Library Manager:
4. Install the ESP32 board package via Tools -> Boards Manager:
   - Search `esp32` -> install esp32 by Espressif Systems
   - Select board: ESP32S3 Dev Module
5. Select your COM port and click **Upload**

## REST API Reference
Full interactive reference available at `/api-docs`. Quick summary:

|**Method** | **Endpoint** | **Description**
|-----|-------------|---------|
| `POST` | `/api/event` | ESP32 posts a single clench event
| `POST` | `/api/summary` | ESP32 posts a 5-second batch summary
| `GET` | `/api/config` | ESP32 fetches current device config
| `PUT` | `/api/config` | Dashboard pushes updated config
| `GET` | `/api/readings` | Fetch recent event log
| `GET` | `/api/stats` | Fetch session statistics
| `DELETE` | `/api/data` | Clear session data
| `POST` | `/api/simulator/start` | Start demo simulator
| `POST` | `/api/simulator/stop` | Stop demo simulator

## Team Members
- Yadiel Narvaez Hernandez
- Madhumitha Rangaraj
- Reyna Torrado-Rivera
- Grace Wang
- Ka Yi Zheng

## Acknowledgments
- MedTech Hack Organizers
- Georgia Tech
