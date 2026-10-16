#pragma once
#include <Arduino.h>
#include "market.h"

namespace ui {
void begin();
void setBrightness(int pct);
void beepOk();
void beepErr();
void beepTick();
void beepUpdate();   // call every loop

void boot(const String& line1, const String& line2 = "");
// Main-screen setup checklist. `problem` = red line with the latest failure, if any.
void setup(const String& pubkey, bool wifiCfg, bool walletOk, bool peepOk, const String& problem);
void list(const Token* t, int n, int sel, const String& status, const String& emptyMsg = "");
void confirmBuy(const Token& t, float sol, bool armed);
void busy(const String& title, const String& line);
void position(const String& sym, const String& mint, double spentSol, double valueSol, double pnlPct,
              int tp, int sl, bool armed, bool stale);
void confirmSell(const String& sym, double pnlPct);
void result(bool ok, const String& title, const String& l1, const String& l2, const String& l3);

// Mode menu: items Manual / Miner / Settings, `cur` highlighted.
void menu(int cur);
// On-device miner settings: `labels[i]`/`values[i]` rows, cursor `cur`.
void settings(const String* labels, const String* values, int n, int cur);
// Miner idle screen (waiting for a new radar signal). Times in seconds, -1 = unknown.
void miner(int nextInSec, int intervalSec, int lastCheckAgoSec, const String& seenSym, int seenAgeSec,
           const String& boughtSym, int lastBuyAgoSec, int runSec, int buys, const String& clockUtc,
           const String& err);
// Miner open position: PnL plus hold time, time left to timeout, exit targets.
void minerPosition(const String& sym, double spentSol, double valueSol, double pnlPct, int heldSec, int leftSec,
                   int holdX, int slPct, int trailPct, double peakPct, double dropPct, bool stale);
}
