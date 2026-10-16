#include "market.h"
#include "config.h"
#include "net.h"
#include "timing.h"
#include <math.h>
#include <ArduinoJson.h>
#include <algorithm>

namespace market {

bool signalsDegraded = false;
bool minerMode = false;

// ---------------- Peep partner signals ----------------
static String ageStr(long s) {
  if (s < 0) return "?";
  if (s < 90) return String(s) + "s";
  if (s < 5400) return String(s / 60) + "m";
  if (s < 172800) return String(s / 3600) + "h";
  return String(s / 86400) + "d";
}

static int peepSignals(Token* out, int n) {
  if (cfg.peepKey.length() == 0) return -2;
  JsonDocument f, r;
  f["generatedAtMs"] = true;
  f["degraded"] = true;
  JsonObject o = f["tokens"][0].to<JsonObject>();
  o["mint"] = true; o["symbol"] = true; o["marketCapUsd"] = true; o["priceUsd"] = true; o["createdAtMs"] = true;
  o["sources"][0]["source"] = true;
  o["sources"][0]["atMs"] = true;
  String bearer = "Bearer " + cfg.peepKey;
  int code = net::json("https://peep-market-realtime.fly.dev/peep-partner-api/v1/signals", r, &f, "GET", nullptr,
                       "Authorization", bearer.c_str());
  if (code == 401 || code == 403) { net::lastError = "Peep key rejected"; return -3; }
  if (code != 200) return -1;
  signalsDegraded = r["degraded"] | false;
  uint64_t gen = r["generatedAtMs"] | (uint64_t)0;
  struct Row { Token t; uint64_t last; uint64_t created; };
  std::vector<Row> rows;
  for (JsonObjectConst t : r["tokens"].as<JsonArrayConst>()) {
    String mint = t["mint"] | "";
    if (mint.length() < 32 || !mint.endsWith("pump")) continue;   // pump.fun mints only
    Row w;
    w.t.mint = mint;
    {   // the LCD fonts are ASCII only: keep printable ASCII, fall back to the mint prefix
      String raw = t["symbol"] | "";
      String clean;
      for (size_t i = 0; i < raw.length() && clean.length() < 10; i++) {
        uint8_t ch = (uint8_t)raw[i];
        if (ch >= 32 && ch < 127) clean += (char)ch;
      }
      clean.trim();
      w.t.sym = clean.length() ? clean : mint.substring(0, 5);
    }
    w.t.mcap = t["marketCapUsd"] | 0.0f;
    w.t.priceUsd = t["priceUsd"] | 0.0f;
    uint64_t created = t["createdAtMs"] | (uint64_t)0;
    w.created = created;
    if (gen && created && gen >= created) w.t.ageSec = (int)((gen - created) / 1000);
    // Signal type filter by mode: 0 radar, 1 thesis, 2 call, 3 any. Ordered by the matched signal time.
    static const char* WANT[] = {"radar", "thesis", "call", ""};
    const char* want = minerMode ? "" : WANT[constrain(cfg.sort, 0, 3)];
    w.last = 0;
    if (minerMode) {
      if (w.t.mcap < tmr::MIN_MCAP_USD || w.t.mcap > tmr::MAX_MCAP_USD) continue;
      if (w.t.ageSec > tmr::MAX_TOKEN_AGE_SEC) continue;
    }
    bool hasRadar = false, hasThesis = false, hasCall = false;
    float score = 0;
    std::vector<std::pair<uint64_t, String>> s;
    for (JsonObjectConst x : t["sources"].as<JsonArrayConst>()) {
      uint64_t at = x["atMs"] | (uint64_t)0;
      String name = x["source"] | "?";
      bool match = want[0] == 0 || name == want;
      if (match && at > w.last) w.last = at;
      s.push_back({at, name});
      float wt = name == "radar" ? tmr::W_RADAR : name == "thesis" ? tmr::W_THESIS : name == "call" ? tmr::W_CALL : 0;
      if (wt > 0 && gen >= at) {
        score += wt * powf(0.5f, (float)((gen - at) / 1000) / tmr::SCORE_HALFLIFE_SEC);
        (name == "radar" ? hasRadar : name == "thesis" ? hasThesis : hasCall) = true;
      }
    }
    if (w.last == 0) continue;                       // token has no signal of the wanted type
    int kinds = hasRadar + hasThesis + hasCall;
    for (int k = 1; k < kinds; k++) score *= tmr::SCORE_MULTI;
    w.t.score = score;
    if (minerMode && score < tmr::MIN_SCORE) continue;
    std::sort(s.begin(), s.end(), [want](const std::pair<uint64_t, String>& a, const std::pair<uint64_t, String>& b) {
      bool am = want[0] && a.second == want, bm = want[0] && b.second == want;
      if (am != bm) return am;
      return a.first > b.first;
    });
    for (size_t i = 0; i < s.size() && i < 3; i++) {
      long ago = gen > s[i].first ? (long)((gen - s[i].first) / 1000) : 0;
      if (i) w.t.signals += "  ";
      w.t.signals += s[i].second + " " + ageStr(ago);
      if (i == 0) w.t.sigAgeSec = (int)ago;
    }
    rows.push_back(w);
  }
  // newest token first (createdAtMs desc); tokens without a creation time go last, then by signal time
  const bool byScore = minerMode;
  std::sort(rows.begin(), rows.end(), [byScore](const Row& a, const Row& b) {
    if (byScore && a.t.score != b.t.score) return a.t.score > b.t.score;
    if (a.created != b.created) return a.created > b.created;
    return a.last > b.last;
  });
  int c = min((int)rows.size(), n);
  for (int i = 0; i < c; i++) out[i] = rows[i].t;
  return c;
}


int trending(Token* out, int n) { return peepSignals(out, n); }

// ---------------- fallback position price (DexScreener, no key) ----------------
// Only SOL-quoted pairs: priceNative of a USDC/other-quoted pair is in the wrong unit.
bool price(const String& mint, double& priceNative) {
  JsonDocument f, r;
  JsonObject o = f[0].to<JsonObject>();
  o["priceNative"] = true;
  o["dexId"] = true;
  o["quoteToken"]["address"] = true;
  o["liquidity"]["usd"] = true;
  if (net::json("https://api.dexscreener.com/tokens/v1/solana/" + mint, r, &f) != 200) return false;
  double best = 0, bestLiq = -1;
  for (JsonObjectConst p : r.as<JsonArrayConst>()) {      // deepest SOL-quoted pump pool wins
    if (strcmp(p["quoteToken"]["address"] | "", "So11111111111111111111111111111111111111112")) continue;
    const char* dex = p["dexId"] | "";
    if (strcmp(dex, "pumpfun") && strcmp(dex, "pumpswap")) continue;
    double liq = p["liquidity"]["usd"] | 0.0;
    double px = p["priceNative"].as<double>();
    if (px > 0 && liq > bestLiq) { best = px; bestLiq = liq; }
  }
  if (best <= 0) return false;
  priceNative = best;
  return true;
}
}  // namespace market
