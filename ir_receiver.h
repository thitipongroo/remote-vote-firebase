#pragma once

#include <IRremote.h>

// NEC protocol hex codes for buttons 1–4
static const uint32_t IR_KEY_1 = 0x00FFA25D;
static const uint32_t IR_KEY_2 = 0x00FF629D;
static const uint32_t IR_KEY_3 = 0x00FFE21D;
static const uint32_t IR_KEY_4 = 0x00FF22DD;

enum class VoteChoice : uint8_t {
    NONE  = 0,
    ONE   = 1,
    TWO   = 2,
    THREE = 3,
    FOUR  = 4
};

extern IRrecv        irRecv;
extern decode_results irResults;

static void setupIR() {
    irRecv.enableIRIn();
    Serial.println(F("[IR] Receiver ready"));
}

// Non-blocking IR poll. Returns decoded choice or NONE.
static VoteChoice pollIR() {
    if (!irRecv.decode(&irResults)) return VoteChoice::NONE;

    uint32_t code = irResults.value;
    Serial.printf("[IR] Received: 0x%08X (%d bits)\n", code, irResults.bits);
    irRecv.resume();

    if (code == IR_KEY_1) { Serial.println(F("[IR] Key 1")); return VoteChoice::ONE;   }
    if (code == IR_KEY_2) { Serial.println(F("[IR] Key 2")); return VoteChoice::TWO;   }
    if (code == IR_KEY_3) { Serial.println(F("[IR] Key 3")); return VoteChoice::THREE; }
    if (code == IR_KEY_4) { Serial.println(F("[IR] Key 4")); return VoteChoice::FOUR;  }

    Serial.println(F("[IR] Unrecognised code — ignoring"));
    return VoteChoice::NONE;
}
