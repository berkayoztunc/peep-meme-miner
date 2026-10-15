#pragma once
#include <Arduino.h>
#include "market.h"

struct Position {
  bool open = false;
  String mint, sym, pair, dex;
  uint64_t spentLamports = 0;
  uint64_t tokens = 0;   // raw units
  int decimals = 6;
  uint32_t openedAt = 0;  // epoch seconds at buy (0 = clock unknown)
};
extern Position pos;

typedef void (*ProgressFn)(const char* msg);
extern ProgressFn onProgress;

void positionLoad();
void positionSave();
void positionClear();

// armed=false -> simulate only (sig = "SIMULATED").
// solOverride > 0 replaces cfg.buySol (still capped by cfg.maxSol).
bool buy(const Token& t, bool armed, String& err, String& sig, float solOverride = 0);
bool sell(bool armed, String& err, String& sig, int64_t& deltaLamports);

// Exact on-chain price (SOL per whole token) from curve or pool reserves. No API quota used.
bool chainPrice(const String& mint, double& priceNative);

double positionValueSol(double priceNative);
double positionPnlPct(double priceNative);
