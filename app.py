"""
VibroJaw — Flask Backend
Team: ChocoChip Circuits | MedTech Hack 2025

Run with:
    pip install flask flask-socketio
    python app.py

Then open: http://localhost:5000
"""

from flask import Flask, request, jsonify, render_template, send_from_directory
from flask_socketio import SocketIO, emit
from collections import deque
from datetime import datetime, timezone
import threading
import time
import random

# App setup
app = Flask(__name__)
app.config["SECRET_KEY"] = "vibrojaw-secret-2025"
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")

# In-memory storage (capped deques)
MAX_EVENTS    = 500
MAX_SUMMARIES = 200

event_store   = {}   # { userId: deque([ClenchEvent, ...]) }
summary_store = {}   # { userId: deque([SessionSummary, ...]) }
config_store  = {}   # { userId: DeviceConfig dict }

# Default device config
def default_config(user_id):
    return {
        "userId":             user_id,
        "threshold":          400,
        "vibrationEnabled":   True,
        "vibrationIntensity": 200,
        "vibrationDuration":  600,
    }

# Health status updates
def health_status(epm):
    """Events-per-minute → Calm / Elevated / Concerning"""
    if epm < 3:  return "Calm"
    if epm < 8:  return "Elevated"
    return "Concerning"

# Helpers methods
def get_events(user_id):
    return event_store.setdefault(user_id, deque(maxlen=MAX_EVENTS))

def get_summaries(user_id):
    return summary_store.setdefault(user_id, deque(maxlen=MAX_SUMMARIES))

def now_iso():
    return datetime.now(timezone.utc).isoformat()

def events_per_minute(user_id):
    """Count events in last 60 seconds of real timestamps."""
    events = get_events(user_id)
    cutoff = datetime.now(timezone.utc).timestamp() - 60
    count  = sum(
        1 for e in events
        if datetime.fromisoformat(e["timestamp"]).timestamp() > cutoff
    )
    return float(count)

def session_stats(user_id):
    events = list(get_events(user_id))
    total  = len(events)
    avg_i  = round(sum(e["intensity"] for e in events) / total, 1) if total else 0.0
    therapy = sum(1 for e in events if e["therapyTriggered"])
    epm    = events_per_minute(user_id)
    return {
        "totalEvents":   total,
        "avgIntensity":  avg_i,
        "therapyCount":  therapy,
        "currentEpm":    round(epm, 1),
        "currentStatus": health_status(epm),
    }

# ============================================================
#  PAGE ROUTES
# ============================================================
@app.route("/")
def index():
    return render_template("index.html")

@app.route("/dashboard")
def dashboard():
    user_id = request.args.get("userId", "esp32-001")
    cfg     = config_store.get(user_id, default_config(user_id))
    stats   = session_stats(user_id)
    return render_template("dashboard.html",
        userId=user_id,
        config=cfg,
        sessionStats=stats,
        simRunning=simulator.running,
        simScenario=simulator.scenario,
    )

@app.route("/about")
def about():
    return render_template("about.html")

@app.route("/api-docs")
def api_docs():
    return render_template("api_docs.html")

# ============================================================
#  ESP32 ENDPOINTS
# ============================================================
@app.route("/api/event", methods=["POST"])
def receive_event():
    body = request.get_json(force=True)
    user_id  = body.get("userId", "esp32-001")
    intensity = float(body.get("intensity", 0))
    duration  = int(body.get("durationMs", 0))
    therapy   = bool(body.get("therapyTriggered", False))

    event = {
        "userId":           user_id,
        "intensity":        round(intensity, 1),
        "durationMs":       duration,
        "therapyTriggered": therapy,
        "timestamp":        now_iso(),
    }

    get_events(user_id).appendleft(event)

    # Broadcast to dashboard via WebSocket
    socketio.emit("new_event", event)

    return jsonify(event), 200


@app.route("/api/summary", methods=["POST"])
def receive_summary():
    body = request.get_json(force=True)
    user_id    = body.get("userId", "esp32-001")
    count      = int(body.get("eventsInWindow", 0))
    avg_i      = float(body.get("avgIntensity", 0))
    window_ms  = int(body.get("windowMs", 5000))

    epm    = count * (60000.0 / window_ms) if window_ms > 0 else 0.0
    status = health_status(epm)

    summary = {
        "userId":          user_id,
        "eventsInWindow":  count,
        "avgIntensity":    round(avg_i, 1),
        "windowMs":        window_ms,
        "eventsPerMinute": round(epm, 1),
        "healthStatus":    status,
        "timestamp":       now_iso(),
    }

    get_summaries(user_id).appendleft(summary)
    socketio.emit("new_summary", summary)

    return jsonify(summary), 200


@app.route("/api/config", methods=["GET"])
def get_config():
    user_id = request.args.get("userId", "esp32-001")
    cfg = config_store.get(user_id, default_config(user_id))
    return jsonify(cfg), 200

# ============================================================
#  DASHBOARD ENDPOINTS
# ============================================================
@app.route("/api/readings", methods=["GET"])
def get_readings():
    user_id = request.args.get("userId", "esp32-001")
    limit   = int(request.args.get("limit", 50))
    events  = list(get_events(user_id))[:limit]
    return jsonify(events), 200


@app.route("/api/summaries", methods=["GET"])
def get_summaries_route():
    user_id  = request.args.get("userId", "esp32-001")
    limit    = int(request.args.get("limit", 60))
    summaries = list(get_summaries(user_id))[:limit]
    return jsonify(summaries), 200


@app.route("/api/stats", methods=["GET"])
def get_stats():
    user_id = request.args.get("userId", "esp32-001")
    return jsonify(session_stats(user_id)), 200


@app.route("/api/config", methods=["PUT"])
def update_config():
    cfg = request.get_json(force=True)
    user_id = cfg.get("userId", "esp32-001")
    config_store[user_id] = cfg
    return jsonify(cfg), 200


@app.route("/api/data", methods=["DELETE"])
def clear_data():
    user_id = request.args.get("userId", "esp32-001")
    event_store.pop(user_id, None)
    summary_store.pop(user_id, None)
    return jsonify({"message": f"Data cleared for {user_id}"}), 200

# ============================================================
#  SIMULATOR
# ============================================================
class Simulator:
    def __init__(self):
        self.running  = False
        self.scenario = "MODERATE"
        self._thread  = None
        self._stop    = threading.Event()
        # per-window counters
        self._w_count = 0
        self._w_isum  = 0.0
        self._last_summary = time.time()

    def start(self, scenario="MODERATE"):
        if self.running:
            self.stop()
        self.scenario  = scenario
        self.running   = True
        self._stop.clear()
        self._w_count  = 0
        self._w_isum   = 0.0
        self._last_summary = time.time()

        # Clear old data
        event_store.pop(SIM_USER, None)
        summary_store.pop(SIM_USER, None)

        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self.running = False
        self._stop.set()

    def _run(self):
        while not self._stop.is_set():
            delay = self._next_delay()
            # Wait in small increments so stop() is responsive
            for _ in range(int(delay * 10)):
                if self._stop.is_set():
                    return
                time.sleep(0.1)

            if self._stop.is_set():
                return

            self._emit_event()

            # Every 5 seconds, emit summary
            if time.time() - self._last_summary >= 5.0:
                self._emit_summary()
                self._last_summary = time.time()

    def _emit_event(self):
        intensity = self._rand_intensity()
        duration  = self._rand_duration()
        cfg       = config_store.get(SIM_USER, default_config(SIM_USER))
        therapy   = cfg.get("vibrationEnabled", True)

        event = {
            "userId":           SIM_USER,
            "intensity":        round(intensity, 1),
            "durationMs":       duration,
            "therapyTriggered": therapy,
            "timestamp":        now_iso(),
        }

        get_events(SIM_USER).appendleft(event)
        self._w_count += 1
        self._w_isum  += intensity
        socketio.emit("new_event", event)

    def _emit_summary(self):
        avg_i  = round(self._w_isum / self._w_count, 1) if self._w_count else 0.0
        epm    = self._w_count * 12.0   # 12 × 5s = 1 min
        status = health_status(epm)

        summary = {
            "userId":          SIM_USER,
            "eventsInWindow":  self._w_count,
            "avgIntensity":    avg_i,
            "windowMs":        5000,
            "eventsPerMinute": round(epm, 1),
            "healthStatus":    status,
            "timestamp":       now_iso(),
        }

        get_summaries(SIM_USER).appendleft(summary)
        socketio.emit("new_summary", summary)
        self._w_count = 0
        self._w_isum  = 0.0

    def _next_delay(self):
        ranges = {
            "MILD":     (8.0,  20.0),
            "MODERATE": (3.0,  10.0),
            "SEVERE":   (1.5,   5.0),
        }
        lo, hi = ranges.get(self.scenario, (3.0, 10.0))
        return lo + random.random() * (hi - lo)

    def _rand_intensity(self):
        ranges = {
            "MILD":     (25, 55),
            "MODERATE": (40, 75),
            "SEVERE":   (60, 95),
        }
        lo, hi = ranges.get(self.scenario, (40, 75))
        return lo + random.random() * (hi - lo)

    def _rand_duration(self):
        ranges = {
            "MILD":     (80,  280),
            "MODERATE": (150, 550),
            "SEVERE":   (300, 900),
        }
        lo, hi = ranges.get(self.scenario, (150, 550))
        return lo + int(random.random() * (hi - lo))


SIM_USER   = "simulator"
simulator  = Simulator()


@app.route("/api/simulator/start", methods=["POST"])
def start_sim():
    body     = request.get_json(force=True) or {}
    scenario = body.get("scenario", "MODERATE")
    simulator.start(scenario)
    return jsonify({"running": True, "scenario": scenario, "userId": SIM_USER}), 200


@app.route("/api/simulator/stop", methods=["POST"])
def stop_sim():
    simulator.stop()
    return jsonify({"running": False}), 200


@app.route("/api/simulator/status", methods=["GET"])
def sim_status():
    return jsonify({
        "running":  simulator.running,
        "scenario": simulator.scenario,
        "userId":   SIM_USER,
    }), 200


# ============================================================
#  ENTRY POINT
# ============================================================
if __name__ == "__main__":
    print("=" * 50)
    print("  VibroJaw Server starting...")
    print("  Open: http://localhost:5000")
    print("=" * 50)
    socketio.run(app, host="0.0.0.0", port=5000, debug=True, allow_unsafe_werkzeug=True)
