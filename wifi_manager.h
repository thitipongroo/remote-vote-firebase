#pragma once

#include <WiFi.h>
#include <WiFiMulti.h>
#include "config.h"

extern WiFiMulti wifiMulti;

static void syncClock() {
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // UTC+7 (Bangkok)
    Serial.print(F("[NTP] Waiting for time sync"));

    const unsigned long TIMEOUT_MS = 15000UL;
    unsigned long start = millis();
    time_t now = time(nullptr);

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

static void setupWiFi() {
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

// Returns true if connected (or successfully reconnected).
static bool ensureWiFi() {
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
