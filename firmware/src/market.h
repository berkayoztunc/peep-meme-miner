#pragma once
#include <Arduino.h>

struct Token {
  String mint, pair, sym, dex;   // dex: "pumpfun" / "pumpswap", resolved on-chain at trade time
  float priceUsd = 0, priceNative = 0;
  float ch5m = 0, ch1h = 0;
  float mcap = 0, liq = 0, vol5m = 0;
  int ageSec = -1;        // token age at the time of the snapshot, -1 unknown
  String signals;         // e.g. "radar 33m  thesis 18s"
  int sigAgeSec = -1;     // age of the matched signal
  uint8_t enrich = 0;
};

namespace market {
extern bool signalsDegraded;
// true: order by newest matching signal time instead of token creation time (miner).
extern bool sortBySignal;
// Token list from the Peep partner signals API only (pump.fun mints). cfg.sort selects the signal type:
// 0 radar, 1 thesis, 2 call, 3 any. Returns count, -2 no key, -3 key rejected, -1 API/network error.
int trending(Token* out, int n);
// Fallback price for an open position when on-chain read fails: DexScreener (no key).
bool price(const String& mint, double& priceNative);
}
