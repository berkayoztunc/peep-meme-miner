#pragma once
#include <Arduino.h>
#include <vector>
#include "core.h"
#include "txbuild.h"

namespace sol {
using core::Pk;
using core::Bytes;

// ---- RPC ----
// found=false when account does not exist. Reads data[off, off+len).
bool getAccount(const Pk& a, size_t off, size_t len, Bytes& data, bool& found, Pk* owner = nullptr);
bool getBalance(const Pk& a, uint64_t& lamports);
bool tokenBalance(const Pk& ata, uint64_t& amount, int* decimals = nullptr);  // amount=0 if account missing
bool sendTx(const Bytes& wire, String& sig, String& err);
bool simulateTx(const Bytes& wire, String& err, String& logs);
bool confirm(const String& sig, uint32_t timeoutMs, String& err);

// ---- tx ----
// message -> signed wire bytes
bool signWire(const Bytes& message, Bytes& wire);
}
