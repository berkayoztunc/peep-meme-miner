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
constexpr int MAX_SIG_AGE_SEC = 3600;          // newest signal older than this: wait for a new token
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
