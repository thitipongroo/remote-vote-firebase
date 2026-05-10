#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <IRremote.h>

#include "config.h"
#include "certs.h"
#include "wifi_manager.h"
#include "ir_receiver.h"
#include "firebase.h"

static const uint32_t DEVICE_TAG   = 100025;
static const uint8_t  IR_RECV_PIN  = 16;

WiFiMulti      wifiMulti;
IRrecv         irRecv(IR_RECV_PIN);
decode_results irResults;

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
            Serial.println(ok ? F("[Vote] Submitted") : F("[Vote] Failed — retry on next press"));
        } else {
            Serial.println(F("[Vote] No WiFi — vote dropped"));
        }
    }
}
