from contextlib import asynccontextmanager
from datetime import datetime, timezone
import asyncio
import sqlite3
import requests

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse

ORION_URL = "http://localhost:1026"
IOT_AGENT_URL = "http://localhost:4041"
FIWARE_SERVICE = "smartpark"
FIWARE_SERVICEPATH = "/"
ENTITY_ID = "urn:ngsi-ld:ParkingSite:001"
ENTITY_TYPE = "ParkingSite"
DB_PATH = "smartpark.db"
POLL_SECONDS = 5
poll_task = None


def db_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    conn = db_connection()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS parking_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            slot1 TEXT, slot2 TEXT, slot3 TEXT,
            occupied_count INTEGER, available_count INTEGER,
            occupancy_rate REAL, entrance_gate TEXT, exit_gate TEXT,
            last_event TEXT, wifi_rssi INTEGER
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS parking_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            event TEXT NOT NULL,
            occupied_count INTEGER, available_count INTEGER,
            slot1 TEXT, slot2 TEXT, slot3 TEXT
        )
    """)
    conn.commit()
    conn.close()


def get_orion_entity():
    url = f"{ORION_URL}/v2/entities/{ENTITY_ID}"
    headers = {"fiware-service": FIWARE_SERVICE, "fiware-servicepath": FIWARE_SERVICEPATH}
    try:
        response = requests.get(url, headers=headers, timeout=5)
    except requests.RequestException as exc:
        raise HTTPException(status_code=503, detail=f"Cannot connect to Orion: {exc}")
    if response.status_code == 404:
        raise HTTPException(status_code=404, detail=f"Entity {ENTITY_ID} was not found")
    if not response.ok:
        raise HTTPException(status_code=502, detail=f"Orion returned HTTP {response.status_code}: {response.text}")
    return response.json()


def normalize_entity(entity):
    def value(name, default=None):
        attr = entity.get(name)
        return attr.get("value", default) if attr else default

    time_instant = value("TimeInstant")

    # Read the 3 physical slot states
    slots = {
        "slot1": value("slot1"),
        "slot2": value("slot2"),
        "slot3": value("slot3"),
    }

    # Recalculate stats from actual slot states (ignore device-reported
    # occupiedCount/availableCount/occupancyRate — the ESP32 has a slot4
    # defined in provisioning that skews the totals)
    TOTAL_SLOTS = 3
    occupied = sum(
        1 for s in slots.values()
        if s and str(s).lower() == "occupied"
    )
    available = TOTAL_SLOTS - occupied
    rate = round((occupied / TOTAL_SLOTS) * 100)

    return {
        "entityId": entity.get("id", ENTITY_ID),
        "entityType": entity.get("type", ENTITY_TYPE),
        "slots": slots,
        "occupiedCount": occupied,
        "availableCount": available,
        "occupancyRate": rate,
        "entranceGate": value("gateStatus", "closed"),
        "exitGate": value("exitGateStatus", "closed"),
        "lastEvent": value("lastEvent", "NONE"),
        "wifiRssi": value("wifiRssi"),
        "timeInstant": time_instant,
    }


def save_snapshot(status):
    now = datetime.now(timezone.utc).isoformat()
    s = status["slots"]
    conn = db_connection()

    conn.execute("""
        INSERT INTO parking_history (
            timestamp, slot1, slot2, slot3, occupied_count, available_count,
            occupancy_rate, entrance_gate, exit_gate, last_event, wifi_rssi
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        now, s["slot1"], s["slot2"], s["slot3"],
        status["occupiedCount"], status["availableCount"], status["occupancyRate"],
        status["entranceGate"], status["exitGate"], status["lastEvent"], status["wifiRssi"]
    ))

    previous = conn.execute(
        "SELECT last_event FROM parking_history ORDER BY id DESC LIMIT 1 OFFSET 1"
    ).fetchone()
    previous_event = previous["last_event"] if previous else None
    current_event = status["lastEvent"]

    if current_event and current_event != "NONE" and current_event != previous_event:
        conn.execute("""
            INSERT INTO parking_events (
                timestamp, event, occupied_count, available_count, slot1, slot2, slot3
            ) VALUES (?, ?, ?, ?, ?, ?, ?)
        """, (
            now, current_event, status["occupiedCount"], status["availableCount"],
            s["slot1"], s["slot2"], s["slot3"]
        ))

    conn.commit()
    conn.close()


async def polling_loop():
    while True:
        try:
            save_snapshot(normalize_entity(get_orion_entity()))
        except Exception as exc:
            print(f"[history] polling error: {exc}")
        await asyncio.sleep(POLL_SECONDS)


@asynccontextmanager
async def lifespan(app):
    global poll_task
    init_db()
    poll_task = asyncio.create_task(polling_loop())
    print(f"SmartPark history collector started ({POLL_SECONDS}s interval)")
    yield
    if poll_task:
        poll_task.cancel()
        try:
            await poll_task
        except asyncio.CancelledError:
            pass


app = FastAPI(title="SmartPark IoT Dashboard API", version="3.0.0", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"], allow_credentials=True,
    allow_methods=["*"], allow_headers=["*"]
)


@app.get("/")
def root():
    return FileResponse("static/index.html")


@app.get("/health")
def health():
    return {"status": "ok", "timestamp": datetime.now(timezone.utc).isoformat()}


@app.get("/api/parking/status")
def parking_status():
    return normalize_entity(get_orion_entity())


@app.get("/api/parking/history")
def parking_history(limit: int = 100):
    limit = max(1, min(limit, 1000))
    conn = db_connection()
    rows = conn.execute("""
        SELECT timestamp, slot1, slot2, slot3, occupied_count,
               available_count, occupancy_rate, entrance_gate,
               exit_gate, last_event, wifi_rssi
        FROM parking_history ORDER BY id DESC LIMIT ?
    """, (limit,)).fetchall()
    conn.close()
    return {"count": len(rows), "data": [dict(r) for r in reversed(rows)]}


@app.get("/api/parking/events")
def parking_events(limit: int = 20):
    limit = max(1, min(limit, 200))
    conn = db_connection()
    rows = conn.execute("""
        SELECT timestamp, event, occupied_count, available_count, slot1, slot2, slot3
        FROM parking_events ORDER BY id DESC LIMIT ?
    """, (limit,)).fetchall()
    conn.close()
    return {"count": len(rows), "data": [dict(r) for r in rows]}

# =========================================================
# GATE CONTROL
# =========================================================

def send_gate_command(command_name: str, command_value: str):
    """
    Send a gate command via Orion → IoT Agent → MQTT pipeline.

    Orion receives an actionType=update with type=command.
    The IoT Agent detects it's a registered command and publishes
    {"<command_name>": "<command_value>"} to the device's MQTT cmd topic.
    The ESP32 parses it as e.g. "gate:open" or "exitGate:close".
    """
    url = f"{ORION_URL}/v2/op/update"

    headers = {
        "fiware-service": FIWARE_SERVICE,
        "fiware-servicepath": FIWARE_SERVICEPATH,
        "Content-Type": "application/json",
    }

    payload = {
        "actionType": "update",
        "entities": [
            {
                "id": ENTITY_ID,
                "type": ENTITY_TYPE,
                command_name: {
                    "type": "command",
                    "value": command_value
                }
            }
        ]
    }

    try:
        response = requests.post(
            url,
            headers=headers,
            json=payload,
            timeout=5
        )
    except requests.RequestException as exc:
        raise HTTPException(
            status_code=503,
            detail=f"Cannot connect to Orion: {exc}"
        )

    if not response.ok:
        raise HTTPException(
            status_code=502,
            detail=f"Gate command failed: HTTP {response.status_code} — {response.text}"
        )

    return {
        "success": True,
        "command": command_name,
        "value": command_value,
        "message": f"Command '{command_name}:{command_value}' dispatched to ESP32 via MQTT"
    }


@app.post("/api/gates/entrance/open")
def open_entrance_gate():
    return send_gate_command("gate", "open")


@app.post("/api/gates/entrance/close")
def close_entrance_gate():
    return send_gate_command("gate", "close")


@app.post("/api/gates/exit/open")
def open_exit_gate():
    return send_gate_command("exitGate", "open")


@app.post("/api/gates/exit/close")
def close_exit_gate():
    return send_gate_command("exitGate", "close")

app.mount("/static", StaticFiles(directory="static"), name="static")
