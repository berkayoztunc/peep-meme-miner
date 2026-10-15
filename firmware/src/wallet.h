#pragma once
#include <Arduino.h>
#include "core.h"

namespace wallet {
bool load();                              // read seed from NVS, derive pubkey
bool has();
const core::Pk& pub();
String pubStr();
bool generate();                          // new key from HW RNG, persisted, secret never leaves
bool importB58(const String& secret);     // 32-byte seed or 64-byte keypair, base58
bool sign(const uint8_t* msg, size_t n, uint8_t sig[64]);
void wipe();
}
