#pragma once

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "certs.h"
#include "config.h"
#include "ir_receiver.h"

// POSTs {"tag":<deviceTag>,"choice":<choice>} to Firebase Realtime Database.
// Returns true on HTTP 200/201.
static bool postVote(uint32_t deviceTag, VoteChoice choice) {
    WiFiClientSecure client;
    client.setCACert(ROOT_CA_CERT);

    HTTPClient https;
    String url = String("https://") + FIREBASE_HOST + FIREBASE_PATH;

    if (!https.begin(client, url)) {
        Serial.println(F("[HTTPS] begin() failed — check FIREBASE_HOST/PATH"));
        return false;
    }

    https.addHeader(F("Content-Type"), F("application/json"));

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
