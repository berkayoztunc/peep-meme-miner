#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "solana.h"

namespace orq {
static const char* PUMP = "6i6q26bmm46b89xlxo1kv";  // PumpFun bonding curve
static const char* AMM = "74u1dnw30q7qbo0y56zlt";    // Pump AMM (PumpSwap)

// POST /api/{project}/instructions/{name}/build. `req` holds accounts, args, feePayer.
// Returns the raw instruction + recent blockhash chosen by Orquestra.
bool build(const char* project, const char* ixName, JsonDocument& req,
           sol::Ix& out, core::Pk& blockhash, String& err);
}
