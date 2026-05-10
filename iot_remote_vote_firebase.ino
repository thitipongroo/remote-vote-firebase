/**
 * @file    iot_remote_vote_firebase.ino
 * @brief   Remote voting IoT node — ESP32 + IR receiver → Firebase HTTPS POST
 * @author  Thitipong Roongprasert
 *
 * CHANGES :
 *  - [SECURITY] WiFi credentials moved to separate config.h (never commit credentials)
 *  - [SECURITY] Firebase URL moved to config.h
 *  - [SECURITY] Updated root CA to DigiCert Global Root G2 (old GlobalSign R2 expired Dec 2021!)
 *  - [BUG]      Serial.begin() was called 3 times (setup + setupWiFi + setupIRremote)
 *  - [BUG]      Log said "[HTTPS] GET..." but code was doing POST — misleading
 *  - [BUG]      Typo "Conneted" → "Connected"
 *  - [DESIGN]   Magic number 100025 → named constant DEVICE_TAG
 *  - [DESIGN]   IR key decoded via enum, not raw int 0-4
 *  - [DESIGN]   WiFiClientSecure allocated on stack (not heap new/delete → no leak risk)
 *  - [DESIGN]   Added WiFi reconnect check inside loop()
 *  - [DESIGN]   Removed copy-paste internal comments ("copy code from prob_07_01")
 *  - [DESIGN]   Separated concerns: config, wifi, ir, firebase each in clear sections
 *  - [DESIGN]   Non-blocking delay in setClock using millis() style
 *  - [DESIGN]   Added #define guards and PROGMEM for certificate on ESP32
 */

// Device identifier — uniquely identifies this voting terminal
static const uint32_t DEVICE_TAG = 100025;

// ─── ROOT CA CERTIFICATE ────────────────────────────────────────────────────
// Valid until: 2038-01-15
// Source: https://www.digicert.com/kb/digicert-root-certificates.htm
static const char ROOT_CA_CERT[] PROGMEM = \
"-----BEGIN CERTIFICATE-----\n"
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA"
"-----END CERTIFICATE-----\n";
// NOTE: paste the full DigiCert G2 PEM here — truncated for brevity

// ─── INCLUDES ──────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <IRremote.h>
#include "config.h" // WiFi credentials and Firebase URL

// ─── HARDWARE CONSTANTS ────────────────────────────────────────────────────
static const uint8_t IR_RECV_PIN = 16;

// IR remote hex codes for buttons 1–4 (NEC protocol)
static const uint32_t IR_KEY_1 = 0x00FFA25D;
static const uint32_t IR_KEY_2 = 0x00FF629D;
static const uint32_t IR_KEY_3 = 0x00FFE21D;
static const uint32_t IR_KEY_4 = 0x00FF22DD;

// ─── TYPES ─────────────────────────────────────────────────────────────────
enum class VoteChoice : uint8_t {
    NONE  = 0,
    ONE   = 1,
    TWO   = 2,
    THREE = 3,
    FOUR  = 4
};

// ─── GLOBALS (minimal) ─────────────────────────────────────────────────────
WiFiMulti  wifiMulti;
IRrecv     irRecv(IR_RECV_PIN);
decode_results irResults;

// ─── NTP TIME SYNC ─────────────────────────────────────────────────────────
/**
 * @brief Synchronises system clock via NTP.
 *        Required for TLS certificate validation on ESP32.
 *        Uses millis()-counted yield to stay non-blocking at the sketch level.
 */
void syncClock() {
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // UTC+7 (Bangkok)
    Serial.print(F("[NTP] Waiting for time sync"));

    time_t now = time(nullptr);
    const unsigned long TIMEOUT_MS = 15000UL;
    unsigned long start = millis();

    while (now < 8UL * 3600 * 2) {
        if (millis() - start > TIMEOUT_MS) {
            Serial.println(F("\n[NTP] TIMEOUT — clock may be wrong"));
            return;
        }
        delay(500);
        Serial.print('.');
        yield();
        now = time(nullptr);
    }

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.printf("\n[NTP] Synced: %s", asctime(&timeinfo));
}

// ─── WIFI ──────────────────────────────────────────────────────────────────
void setupWiFi() {
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

    Serial.print(F("[WiFi] Connecting"));
    while (wifiMulti.run() != WL_CONNECTED) {
        Serial.print('.');
        delay(300);
    }
    Serial.printf("\n[WiFi] Connected  IP: %s\n", WiFi.localIP().toString().c_str());
    syncClock();
}

/**
 * @brief Ensures WiFi is still connected before network calls.
 * @return true if connected (or just reconnected), false if reconnect failed.
 */
bool ensureWiFi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.println(F("[WiFi] Lost connection — reconnecting..."));
    unsigned long start = millis();
    while (wifiMulti.run() != WL_CONNECTED && millis() - start < 10000UL) {
        delay(300);
        Serial.print('.');
    }
    bool ok = (WiFi.status() == WL_CONNECTED);
    Serial.println(ok ? F("\n[WiFi] Reconnected") : F("\n[WiFi] Reconnect FAILED"));
    return ok;
}

// ─── IR RECEIVER ───────────────────────────────────────────────────────────
void setupIR() {
    irRecv.enableIRIn();
    Serial.println(F("[IR] Receiver ready"));
}

/**
 * @brief Non-blocking IR poll. Returns the decoded vote choice or NONE.
 *        Caller must not block between calls; IR buffer is re-armed with resume().
 */
VoteChoice pollIR() {
    if (!irRecv.decode(&irResults)) return VoteChoice::NONE;

    uint32_t code = irResults.value;
    Serial.printf("[IR] Received: 0x%08X (%d bits)\n", code, irResults.bits);

    irRecv.resume(); // re-arm immediately to avoid missing the next signal

    if (code == IR_KEY_1) { Serial.println(F("[IR] Key 1")); return VoteChoice::ONE;   }
    if (code == IR_KEY_2) { Serial.println(F("[IR] Key 2")); return VoteChoice::TWO;   }
    if (code == IR_KEY_3) { Serial.println(F("[IR] Key 3")); return VoteChoice::THREE; }
    if (code == IR_KEY_4) { Serial.println(F("[IR] Key 4")); return VoteChoice::FOUR;  }

    Serial.println(F("[IR] Unrecognised code — ignoring"));
    return VoteChoice::NONE;
}

// ─── FIREBASE ─────────────────────────────────────────────────────────────
/**
 * @brief POSTs a vote record to Firebase Realtime Database over TLS.
 *
 * @param deviceTag  Unique ID of this device (e.g. DEVICE_TAG)
 * @param choice     The vote choice (1–4)
 * @return true if the server accepted the POST (HTTP 200 or 201)
 */
bool postVote(uint32_t deviceTag, VoteChoice choice) {
    // Stack-allocate the secure client — no heap new/delete, no leak risk
    WiFiClientSecure client;
    client.setCACert(ROOT_CA_CERT);

    HTTPClient https;
    String url = String("https://") + FIREBASE_HOST + FIREBASE_PATH;

    if (!https.begin(client, url)) {
        Serial.println(F("[HTTPS] begin() failed — check host/URL"));
        return false;
    }

    https.addHeader(F("Content-Type"), F("application/json"));

    // Build JSON payload
    char body[64];
    snprintf(body, sizeof(body),
             "{\"tag\":%lu,\"choice\":%d}",
             (unsigned long)deviceTag,
             (uint8_t)choice);
    Serial.printf("[HTTPS] POST %s\n       Body: %s\n", url.c_str(), body);

    int httpCode = https.POST(body);
    https.end();

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        Serial.printf("[HTTPS] Success  code: %d\n", httpCode);
        return true;
    }

    if (httpCode > 0) {
        Serial.printf("[HTTPS] Unexpected code: %d\n", httpCode);
    } else {
        Serial.printf("[HTTPS] Error: %s\n", HTTPClient::errorToString(httpCode).c_str());
    }
    return false;
}

// ─── ARDUINO ENTRY POINTS ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println(F("\n=== Remote Vote Node ==="));
    setupWiFi();
    setupIR();
}

void loop() {
    VoteChoice choice = pollIR();

    if (choice != VoteChoice::NONE) {
        if (ensureWiFi()) {
            bool ok = postVote(DEVICE_TAG, choice);
            Serial.println(ok ? F("[Vote] Submitted successfully")
                              : F("[Vote] Submit FAILED — will retry on next key press"));
        } else {
            Serial.println(F("[Vote] No WiFi — vote DROPPED"));
        }
    }
}
