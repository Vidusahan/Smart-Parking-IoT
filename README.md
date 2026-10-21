# SmartPark IoT

A connected smart-parking prototype built with an ESP32, ultrasonic and infrared sensors, servo-controlled gates, MQTT, FIWARE, and a FastAPI web dashboard.

SmartPark monitors parking-space occupancy in real time, automatically controls vehicle entry and exit gates, publishes telemetry to an IoT platform, and provides historical data and remote gate controls through a browser-based dashboard.

> **Project status:** This repository contains a working prototype and reference implementation for a three-space smart-parking system. Hardware pin assignments, network addresses, and FIWARE identifiers may need to be adjusted for your deployment.

## Features

- Real-time monitoring of three parking spaces using ultrasonic distance sensors.
- Entrance and exit vehicle detection using infrared sensors.
- Automatic entrance access control based on available spaces.
- Automatic exit-gate operation when a vehicle is detected.
- Servo-driven gates with timed automatic closing.
- 16×2 I2C LCD status display for local feedback.
- Wi-Fi connectivity with automatic reconnection attempts.
- MQTT telemetry for parking, sensor, gate, device, and event data.
- FIWARE integration using Orion Context Broker and the JSON IoT Agent.
- FastAPI dashboard with:
  - Current parking status.
  - Historical occupancy data.
  - Parking-event history.
  - Manual entrance and exit gate controls.
  - Health-check endpoint.
- Optional QuantumLeap, CrateDB, and Grafana services for time-series storage and visualization.

## System architecture

![SmartPark IoT system architecture](docs/SmartPark%20IoT%20System%20Architecture.png)

The system is divided into four layers:

1. **Edge device** – The ESP32 reads the parking and vehicle sensors, controls the LCD and servos, and communicates through Wi-Fi and MQTT.
2. **Messaging and context layer** – Mosquitto transports MQTT messages, while the FIWARE IoT Agent translates device data and commands for Orion.
3. **Application layer** – The FastAPI service reads the parking entity from Orion, stores snapshots and events in SQLite, serves the dashboard, and sends gate commands.
4. **Visualization layer** – The dashboard displays live status and history. QuantumLeap, CrateDB, and Grafana can be used for longer-term time-series analysis.

### Data flow

```text
ESP32 sensors
    │
    ├── MQTT telemetry ──> Mosquitto ──> FIWARE IoT Agent ──> Orion Context Broker
    │                                                               │
    │                                                               └──> FastAPI ──> Web dashboard / SQLite history
    │
    └── MQTT commands <── FIWARE IoT Agent <── Orion <── FastAPI gate-control API
```

## Repository structure

```text
.
├── FastAPI/
│   ├── main.py                  # API, dashboard serving, polling, and SQLite history
│   ├── requirements.txt         # Python dependencies
│   └── static/                  # Dashboard frontend assets
├── firmware/
│   └── SmartPark_Firmware/
│       └── SmartPark_Firmware.ino
├── fiware/
│   ├── docker-compose.yml       # FIWARE and supporting services
│   ├── mosquitto/               # MQTT broker configuration
│   └── provisioning/             # Service, device, subscription, and test scripts
├── docs/
│   ├── SmartPark IoT System Architecture.png
│   └── SmartPark ESP32 Circuit Wiring Diagram.png
└── README.md
```

## Hardware

The firmware is designed for an ESP32 with:

- 3 ultrasonic sensors for parking slots.
- 2 IR sensors for entrance and exit detection.
- 2 servo motors for the entrance and exit gates.
- 16×2 I2C LCD.
- Wi-Fi network access.

The current firmware pin mapping is:

| Component | ESP32 pins |
|---|---:|
| Slot 1 ultrasonic sensor | Trigger 5, Echo 18 |
| Slot 2 ultrasonic sensor | Trigger 19, Echo 21 |
| Slot 3 ultrasonic sensor | Trigger 22, Echo 23 |
| Entrance IR sensor | 27 |
| Exit IR sensor | 32 |
| Entrance servo | 13 |
| Exit servo | 14 |
| LCD I2C | SDA 16, SCL 17 |

For the wiring reference, see [`docs/SmartPark ESP32 Circuit Wiring Diagram.png`](docs/SmartPark%20ESP32%20Circuit%20Wiring%20Diagram.png).

### Occupancy detection

Each slot uses hysteresis to reduce sensor flicker:

- Distance at or below **12 cm**: slot becomes occupied.
- Distance at or above **18 cm**: slot becomes free.

Adjust these values in `firmware/SmartPark_Firmware/SmartPark_Firmware.ino` to match the physical installation.

## Prerequisites

Install the following before starting:

- Docker and Docker Compose.
- Python 3.9 or newer.
- Arduino IDE or PlatformIO with ESP32 board support.
- An ESP32 board and the components listed above.
- A Wi-Fi network reachable by both the ESP32 and the host running the services.

The firmware uses these Arduino libraries:

- `LiquidCrystal_I2C`
- `ESP32Servo`
- `PubSubClient`

`Wire` and `WiFi` are provided by the ESP32 Arduino core.

## Quick start

### 1. Start the FIWARE stack

From the repository root:

```bash
cd fiware
docker compose up -d
```

Check service status and logs:

```bash
docker compose ps
docker compose logs -f iot-agent
```

The default services expose:

| Service | URL or port |
|---|---|
| Mosquitto MQTT | `1883` |
| Orion Context Broker | `http://localhost:1026` |
| FIWARE IoT Agent | `http://localhost:4041` |
| CrateDB | `http://localhost:4200` |
| QuantumLeap | `http://localhost:8668` |
| Grafana | `http://localhost:3000` |

### 2. Provision FIWARE

The provisioning scripts create the service group, register the device, and configure the subscription used by SmartPark.

On Linux/macOS:

```bash
cd fiware/provisioning
chmod +x *.sh
./provision.sh
./check.sh
```

On Windows PowerShell:

```powershell
cd fiware/provisioning
.\provision.ps1
```

The scripts assume the FIWARE services are available on the default local ports. Review the scripts before running them if your deployment uses a different host, service name, or port.

### 3. Configure and flash the ESP32

Before uploading the firmware, update the Wi-Fi and MQTT settings in `SmartPark_Firmware.ino`:

```cpp
const char* WIFI_SSID = "your-wifi-name";
const char* WIFI_PASSWORD = "your-wifi-password";
const char* MQTT_BROKER = "your-mqtt-host";
const int MQTT_PORT = 1883;
```

Use the machine's LAN IP address for `MQTT_BROKER` when the MQTT broker is running in Docker and the ESP32 is connecting over the network. Do not commit real Wi-Fi passwords, tokens, or private IP addresses to the repository.

Then select the appropriate ESP32 board and port in Arduino IDE and upload:

```text
firmware/SmartPark_Firmware/SmartPark_Firmware.ino
```

Open the Serial Monitor at **115200 baud** to verify Wi-Fi, MQTT, sensor, and gate activity.

### 4. Start the FastAPI dashboard

Create a virtual environment and install the dependencies:

```bash
cd FastAPI
python -m venv .venv
```

Activate it on Linux/macOS:

```bash
source .venv/bin/activate
```

Activate it on Windows PowerShell:

```powershell
.venv\Scripts\Activate.ps1
```

Install and run the API:

```bash
pip install -r requirements.txt
uvicorn main:app --reload
```

Open the dashboard at [http://127.0.0.1:8000](http://127.0.0.1:8000).

The API stores local history in `FastAPI/smartpark.db`. This file is generated at runtime and should not be committed.

## API endpoints

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/` | Serves the web dashboard. |
| `GET` | `/health` | Returns API health and timestamp. |
| `GET` | `/api/parking/status` | Returns the current normalized parking state from Orion. |
| `GET` | `/api/parking/history?limit=100` | Returns recent occupancy snapshots. |
| `GET` | `/api/parking/events?limit=20` | Returns recent parking events. |
| `POST` | `/api/gates/entrance/open` | Opens the entrance gate through the FIWARE command path. |
| `POST` | `/api/gates/entrance/close` | Closes the entrance gate. |
| `POST` | `/api/gates/exit/open` | Opens the exit gate. |
| `POST` | `/api/gates/exit/close` | Closes the exit gate. |

Interactive API documentation is available at `/docs` while the FastAPI server is running.

## MQTT topics

The ESP32 publishes retained status messages on topics including:

- `smartpark/parking/status`
- `smartpark/sensors/slots`
- `smartpark/gates/status`
- `smartpark/device/status`
- `smartpark/events/entrance`
- `smartpark/events/exit`

FIWARE integration uses the configured command and telemetry topics in the firmware and provisioning files. Keep the topic names, service name, entity ID, and provisioning configuration synchronized when changing the deployment.

## Troubleshooting

### Dashboard cannot connect to Orion

- Confirm the FIWARE stack is running with `docker compose ps`.
- Check Orion logs with `docker compose logs -f orion`.
- Confirm the Orion URL, FIWARE service, service path, and entity ID in `FastAPI/main.py`.

### ESP32 cannot connect to MQTT

- Make sure the MQTT broker is reachable from the ESP32's network.
- Use the host machine's LAN IP rather than `localhost`.
- Confirm port `1883` is open and Mosquitto is running.
- Check the Serial Monitor for Wi-Fi and MQTT connection messages.

### Occupancy readings are unstable

- Recheck sensor alignment and power supply.
- Adjust `OCCUPIED_THRESHOLD_CM` and `FREE_THRESHOLD_CM`.
- Keep adequate spacing between ultrasonic sensors to reduce interference.

### Gates move in the wrong direction

Change `GATE_OPEN_ANGLE` and `GATE_CLOSED_ANGLE` in the firmware, and verify the servo power and ground connections.

## Security notes

This prototype is intended for a trusted development network. Before deploying it outside a local test environment:

- Remove hard-coded credentials from firmware.
- Use environment variables or a secrets manager for backend configuration.
- Add authentication and authorization to the gate-control endpoints.
- Restrict CORS origins instead of allowing all origins.
- Secure MQTT with authentication and TLS.
- Avoid exposing Orion, MQTT, MongoDB, CrateDB, or Grafana directly to the public internet.

## Contributing

1. Fork the repository.
2. Create a feature branch.
3. Make and test your changes on hardware or with the FIWARE test scripts.
4. Update documentation when changing wiring, topics, endpoints, or provisioning.
5. Open a pull request with a clear description of the change.

## License

No license has been specified for this repository yet. Unless a license is added, the project remains under the repository owner's default copyright and reuse permissions.
