# Remote Vote Node — ESP32 + IR → Firebase

An ESP32 IoT voting terminal. Press a numbered button on an NEC IR remote and the device posts a signed JSON vote record to Firebase Realtime Database over TLS-validated HTTPS.

---

## Architecture

```text
┌────────────────────────────────────────────────────────┐
│                     Hardware Layer                      │
│                                                        │
│   NEC IR Remote ──(38 kHz modulated)──► VS1838B        │
│                                          (GPIO 16)     │
└────────────────────────────┬───────────────────────────┘
                             │ decoded NEC frame
                             ▼
┌────────────────────────────────────────────────────────┐
│                     ESP32 Firmware                      │
│                                                        │
│  setup()                                               │
│   ├── Serial.begin(115200)                             │
│   ├── setupWiFi()   → WiFiMulti connects + NTP sync    │
│   └── setupIR()     → IRrecv starts                    │
│                                                        │
│  loop()                                               │
│   ├── pollIR()      → decode NEC → VoteChoice enum     │
│   ├── ensureWiFi()  → reconnect if dropped             │
│   └── postVote()    → HTTPS POST to Firebase REST API  │
└────────────────────────────┬───────────────────────────┘
                             │ HTTPS / TLS
                             │ DigiCert Global Root G2
                             ▼
┌────────────────────────────────────────────────────────┐
│              Firebase Realtime Database                 │
│                                                        │
│  /survey/answer/{push_id}                              │
│    { "tag": 100025, "choice": 2 }                      │
│                                                        │
│  Firebase push() creates a unique child key per vote   │
└────────────────────────────────────────────────────────┘
```

---

## Tech Stack

| Layer | Technology | Why |
| ----- | ---------- | --- |
| MCU | ESP32 Dev Module | Dual-core 240 MHz, built-in WiFi/BT, sufficient RAM for TLS buffers (~20 KB) |
| IR decode | IRremote 3.x | Mature NEC protocol support with repeat-frame filtering; non-blocking `IrReceiver.decode()` |
| HTTP client | `WiFiClientSecure` + `HTTPClient` | Ships with the ESP32 Arduino core; supports custom root CA for proper TLS validation |
| Backend | Firebase Realtime Database REST API | Serverless, zero infrastructure, real-time fan-out for downstream dashboards |
| TLS certificate | DigiCert Global Root G2 (PEM) | Hard-coded root CA avoids `setInsecure()` while keeping firmware self-contained |
| Time sync | NTP (`configTime`) | TLS certificate date validation requires accurate system time |

---

## Project Structure

```text
remote-vote-firebase/
├── iot_remote_vote_firebase.ino   # Entry point: globals, setup(), loop()
├── config.h                       # WiFi SSID/password + Firebase host/path (never commit)
├── certs.h                        # DigiCert Global Root G2 PEM (valid until 2038-01-15)
├── wifi_manager.h                 # WiFi connect + NTP sync; ensureWiFi() for reconnects
├── ir_receiver.h                  # NEC decode, VoteChoice enum, button-code constants
└── firebase.h                     # postVote() — builds JSON, fires HTTPS POST
```

---

## System Flow

```text
Power-on
  │
  ▼ setup()
  ├─ WiFi connect (WiFiMulti, up to 30 s timeout)
  ├─ NTP sync (pool.ntp.org, UTC+7)  ← required for TLS cert date check
  └─ IR receiver start (GPIO 16)

loop() — non-blocking, runs continuously
  │
  ├─ pollIR()
  │    ├─ IrReceiver.decode() → NEC code → VoteChoice (ONE/TWO/THREE/FOUR)
  │    └─ returns NONE if no new, valid, non-repeat frame
  │
  ├─ [NONE] → continue (no action)
  │
  └─ [ONE/TWO/THREE/FOUR]
       ├─ ensureWiFi() → reconnect if association lost
       ├─ postVote(DEVICE_TAG=100025, choice)
       │    ├─ build JSON: { "tag": 100025, "choice": N }
       │    ├─ WiFiClientSecure with root CA from certs.h
       │    └─ HTTP POST to FIREBASE_HOST + FIREBASE_PATH
       └─ Serial.println("[Vote] Submitted" | "Failed — retry on next press")
```

---

## Hardware

| Component | Part | Notes |
| --------- | ---- | ----- |
| MCU | ESP32 Dev Module | Any ESP32 board with WiFi |
| IR receiver | VS1838B | 38 kHz carrier, 3.3 V compatible |
| Remote | NEC IR remote | Standard cheap TV / learning remote |

### Wiring

| IR Receiver Pin | ESP32 Pin |
| --------------- | --------- |
| VCC | 3.3 V |
| GND | GND |
| OUT | GPIO 16 |

---

## Button Mapping

| Remote Button | `VoteChoice` | NEC Code |
| ------------- | ------------ | -------- |
| 1 | `ONE` | `0x00FFA25D` |
| 2 | `TWO` | `0x00FF629D` |
| 3 | `THREE` | `0x00FFE21D` |
| 4 | `FOUR` | `0x00FF22DD` |

To remap codes: update the constants in `ir_receiver.h`.

---

## Firebase Payload

Each button press fires one `POST` to `FIREBASE_PATH` (e.g. `/survey/answer.json`). Firebase REST API with `.json` suffix uses **push semantics** — it creates a unique child key per request so votes never overwrite each other.

```json
{ "tag": 100025, "choice": 2 }
```

| Field | Description |
| ----- | ----------- |
| `tag` | Device identifier — set `DEVICE_TAG` in the main `.ino` |
| `choice` | Vote value: `1`–`4` matching the button pressed |

---

## Setup

### 1. Install dependencies

In **Arduino IDE → Library Manager**:

| Library | Version |
| ------- | ------- |
| IRremote (Arduino-IRremote) | 3.x |
| ESP32 Arduino core (Espressif) | ≥ 2.0 |

### 2. Configure credentials

Edit `config.h`:

```cpp
#define WIFI_SSID       "your-ssid"
#define WIFI_PASSWORD   "your-password"
#define FIREBASE_HOST   "YOUR_PROJECT_ID-default-rtdb.firebaseio.com"
#define FIREBASE_PATH   "/survey/answer.json"
```

> **Never commit `config.h` with real credentials.** It is already in `.gitignore`.

### 3. Paste the root CA certificate

Open `certs.h` and replace the placeholder with the full **DigiCert Global Root G2** PEM (valid until 2038-01-15).

### 4. Firebase Realtime Database

1. Create a project at [Firebase Console](https://console.firebase.google.com).
2. Enable **Realtime Database**.
3. Copy the database URL → this is your `FIREBASE_HOST`.
4. Set database rules to allow writes (or restrict to authenticated devices).

### 5. Flash

- **Board:** ESP32 Dev Module
- **Upload speed:** 115200 baud
- Flash via Arduino IDE or PlatformIO.

---

## Security Notes

| Topic | Implementation |
| ----- | -------------- |
| TLS validation | Root CA pinned in `certs.h` — no `setInsecure()` |
| NTP requirement | `configTime()` blocks up to 15 s; warns if sync fails — TLS will fail without it |
| Credential storage | WiFi credentials and Firebase URL live only in `config.h` (gitignored) |

---

## Tradeoffs

| Decision | Alternative | Reasoning |
| -------- | ----------- | --------- |
| HTTPS REST API | MQTT (mosquitto) | HTTPS works through most firewalls without port configuration; MQTT is lower overhead but needs a broker |
| NTP blocking in `setup()` | Skip NTP | TLS certificate date validation is non-negotiable; blocking is acceptable during startup only |
| Votes dropped on WiFi loss | EEPROM/SPIFFS queue | Simplicity first; a queue adds complexity and EEPROM wear — acceptable for a workshop voting device |
| Single root CA | System trust store | ESP32 has no OS trust store; a single pinned CA is the correct approach for embedded TLS |

---

## Scaling Considerations

| Concern | Approach |
| ------- | -------- |
| Multiple voting devices | Each device uses a unique `DEVICE_TAG`; Firebase handles concurrent writes with push keys |
| WiFi dropouts losing votes | Buffer votes in SPIFFS (JSON array); flush on reconnect |
| Higher throughput / lower latency | Switch to MQTT with an MQTT-to-Firebase bridge function |
| Device authentication | Provision Firebase custom tokens or use ESP32 unique chip ID as a lightweight device identity |
| Certificate expiry (2038) | Replace the PEM in `certs.h` and reflash before January 2038 |
