# Remote Vote Node — ESP32 + IR → Firebase

An ESP32 IoT voting terminal. Press a button on an NEC IR remote and the device posts a JSON vote record to Firebase Realtime Database over TLS (HTTPS).

## How it works

```text
IR Remote ──(NEC signal)──▶ ESP32 ──(HTTPS POST / TLS)──▶ Firebase Realtime DB
```

1. On boot the board connects to WiFi and syncs time via NTP (required for TLS certificate validation).
2. `loop()` polls the IR receiver without blocking.
3. When a recognised button is pressed, a JSON payload is POSTed to Firebase.
4. The device retries automatically on the next key press if the previous submission failed.

## Hardware

| Component             | Notes                              |
| --------------------- | ---------------------------------- |
| ESP32 dev board       | Any ESP32 with WiFi                |
| VS1838B IR receiver   | Or any NEC-compatible receiver     |
| NEC IR remote         | Standard cheap TV/learning remote  |

### Wiring

| IR Receiver Pin | ESP32 Pin |
| --------------- | --------- |
| VCC             | 3.3 V     |
| GND             | GND       |
| OUT             | GPIO 16   |

## Software Dependencies

Install via **Arduino IDE → Library Manager**:

| Library                                                                         | Version |
| ------------------------------------------------------------------------------- | ------- |
| [IRremote](https://github.com/Arduino-IRremote/Arduino-IRremote)                | 3.x     |
| ESP32 Arduino core (Espressif)                                                  | ≥ 2.0   |

## Project Structure

```text
remote-vote-firebase/
├── iot_remote_vote_firebase.ino  # Entry point: globals, setup(), loop()
├── config.h                      # WiFi + Firebase credentials (never commit)
├── certs.h                       # DigiCert Global Root G2 PEM
├── wifi_manager.h                # WiFi connection + NTP sync
├── ir_receiver.h                 # IR decoding + VoteChoice type
└── firebase.h                    # HTTPS POST to Firebase
```

## Setup

### 1. Configure credentials

Edit `config.h`:

```cpp
#define WIFI_SSID       "your-ssid"
#define WIFI_PASSWORD   "your-password"
#define FIREBASE_HOST   "YOUR_PROJECT_ID-default-rtdb.firebaseio.com"
#define FIREBASE_PATH   "/survey/answer.json"
```

> **Never commit `config.h` with real credentials.** Add it to `.gitignore`.

### 2. Paste the root CA certificate

Open `certs.h` and replace the placeholder with the full **DigiCert Global Root G2** PEM, available at:
[https://www.digicert.com/kb/digicert-root-certificates.htm](https://www.digicert.com/kb/digicert-root-certificates.htm)

The certificate is valid until **2038-01-15**.

### 3. Firebase Realtime Database

1. Create a project in [Firebase Console](https://console.firebase.google.com).
2. Enable **Realtime Database**.
3. Copy the database URL — it is your `FIREBASE_HOST`.
4. Set database rules to allow writes (or restrict to authenticated users).

### 4. Flash

- Board: **ESP32 Dev Module**
- Upload speed: 115200 baud
- Flash via Arduino IDE or PlatformIO.

## Button Mapping

| Remote Button | VoteChoice | NEC Code     |
| ------------- | ---------- | ------------ |
| 1             | ONE        | `0x00FFA25D` |
| 2             | TWO        | `0x00FF629D` |
| 3             | THREE      | `0x00FFE21D` |
| 4             | FOUR       | `0x00FF22DD` |

To add buttons or remap codes, update the constants in `ir_receiver.h`.

## Firebase Payload

```json
{ "tag": 100025, "choice": 2 }
```

| Field    | Description                                          |
| -------- | ---------------------------------------------------- |
| `tag`    | Device identifier (`DEVICE_TAG` in the main `.ino`)  |
| `choice` | Vote value: 1–4                                      |

Each POST creates a new child node under `FIREBASE_PATH` (Firebase `push` semantics via the REST API).

## Security Notes

- TLS is validated against DigiCert Global Root G2 — no `client.setInsecure()`.
- NTP sync is mandatory before any HTTPS call; the board will wait up to 15 s and warn if sync fails.
- WiFi credentials and the Firebase URL live only in `config.h`, which must not be committed.
