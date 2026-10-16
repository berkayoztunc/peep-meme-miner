// All UI/miner timers and intervals in one place.
#pragma once
#include <Arduino.h>

namespace tmr {

// intervals (ms unless noted)
constexpr uint32_t PRICE_REFRESH_MS = 4000;    // position price poll
constexpr uint32_t PRICE_STALE_MS = 15000;     // no good price for this long: treated as stale
constexpr uint32_t LIST_REFRESH_MS = 90000;    // manual token list auto refresh
constexpr uint32_t RESULT_AUTO_MS = 2500;      // miner result screen auto-continue
constexpr uint32_t MINER_REDRAW_MS = 1000;     // miner screen clock tick
constexpr uint32_t SHAKE_DEBOUNCE_MS = 1500;   // min gap between shake-triggered checks
constexpr uint32_t BOOT_WINDOW_MS = 1500;      // serial window at boot for the settings page
constexpr uint32_t IDLE_UNIT_MS = 60000;       // cfg.idleMin is in minutes
constexpr uint32_t PEAK_SAVE_MS = 30000;      // persist a new position peak at most this often
// Miner candidate scoring (market.cpp). score = sum over signals of weight * 0.5^(age / half-life),
// times SCORE_MULTI for every extra distinct signal type (radar+thesis+call confirm each other).
constexpr float W_RADAR = 3.0f, W_THESIS = 2.0f, W_CALL = 1.0f;
constexpr float SCORE_HALFLIFE_SEC = 900.0f;   // a signal loses half its weight every 15 min
constexpr float SCORE_MULTI = 1.5f;
constexpr float MIN_SCORE = 0.6f;              // below this the signal is too weak / too old to buy
constexpr int MAX_TOKEN_AGE_SEC = 5400;        // ignore tokens launched more than 90 min ago
constexpr float MIN_MCAP_USD = 4500;           // ~launch floor: no traction yet
constexpr float MAX_MCAP_USD = 80000;          // too late, upside gone
constexpr float SHAKE_G = 2.2f;

// Timestamp helper: unset until mark() is called, so "never happened" is explicit (not millis()==0).
struct Stamp {
  uint32_t t = 0;
  bool set() const { return t != 0; }
  void mark() { t = millis() ? millis() : 1; }
  void clear() { t = 0; }
  uint32_t ageMs() const { return millis() - t; }
  int ageSec() const { return set() ? (int)(ageMs() / 1000) : -1; }   // -1 = never
  bool due(uint32_t ms) const { return !set() || ageMs() >= ms; }     // never counts as due
};

}  // namespace tmr
