// Peep meme miner: Pump.fun trader for M5StickC Plus2
#include <M5Unified.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "market.h"
#include "net.h"
#include "proto.h"
#include "pump.h"
#include "timing.h"
#include "ui.h"
#include "wallet.h"

#include <Preferences.h>
#include <time.h>

enum State { S_SETUP, S_LIST, S_CONFIRM_BUY, S_POSITION, S_CONFIRM_SELL, S_RESULT, S_MENU, S_SETTINGS, S_MINER };
static State state = S_SETUP;

// ---- modes ----
enum Mode { MODE_MANUAL = 0, MODE_MINER = 1 };
static Mode mode = MODE_MANUAL;       // persisted in NVS "pump"/"mode"
static int menuCur = 0;
static int setCur = 0;
static const int HIST_N = 20;         // last N handled mints (persisted): never rebuy, stops A/B/C rotation
static String hist[HIST_N];               // hist[0] newest
static bool inHist(const String& m) { for (int i = 0; i < HIST_N; i++) if (hist[i] == m) return true; return false; }
static bool histEmpty() { for (int i = 0; i < HIST_N; i++) if (hist[i].length()) return false; return true; }
static void histPush(const String& m) {
  for (int i = HIST_N - 1; i > 0; i--) hist[i] = hist[i - 1];
  hist[0] = m;
}
static tmr::Stamp minerStart, lastCheck, lastBuy, lastMinerDraw, posFallbackStart;
static int minerBuys = 0;
static String lastMinedSym, minerErr;
static String seenSym;           // newest radar token seen by the last check (bought or not)
static int seenSigAge = -1;      // its signal age (s) at that check
static tmr::Stamp seenAt;

static const int MAXTOK = 20;
static Token toks[MAXTOK];
static int ntok = 0, sel = 0;
static tmr::Stamp lastActivity, lastList, lastPrice, lastPriceOk, resultAt;   // resultAt: miner auto-continues result screens
static double lastPriceNative = 0;
static State afterResult = S_LIST;

// Used by the serial "tokens" command (diagnostics): current card list.
void appTokens(JsonArray out) {
  for (int i = 0; i < ntok; i++) {
    JsonObject o = out.add<JsonObject>();
    o["sym"] = toks[i].sym;
    o["mint"] = toks[i].mint;
    o["mcap"] = toks[i].mcap;
    o["price_usd"] = toks[i].priceUsd;
    o["sig"] = toks[i].signals;
    o["age_s"] = toks[i].ageSec;
  }
}

int appRefresh();
static bool wifiConfigured();
static bool setupComplete() { return wifiConfigured() && wallet::has() && cfg.peepKey.length() > 0; }

static bool wifiConfigured() {
  for (int i = 0; i < 3; i++) if (cfg.ssid[i].length()) return true;
  return false;
}

static String setupProblem;

static void showSetup(const String& problem = "") {
  if (problem.length()) setupProblem = problem;
  ui::setup(wallet::pubStr(), wifiConfigured(), wallet::has(), cfg.peepKey.length() > 0, setupProblem);
  state = S_SETUP;
}

static void progress(const char* m) { ui::busy("TRADE", m); }

static void refreshList() {
  ui::busy("PUMP", "loading tokens...");
  int n = market::trending(toks, MAXTOK);
  ntok = n < 0 ? 0 : n;
  if (sel >= ntok) sel = 0;
  lastList.mark();
  String why;
  static const char* WHAT[] = {"radar", "thesis", "call", "any"};
  if (n == -2) why = "Add Peep key in Settings";
  else if (n == -3) why = "Peep key rejected";
  else if (!net::wifiUp()) why = "No Wi-Fi connection";
  else if (n < 0) why = "Peep API error: " + net::lastError;
  else if (n == 0) why = String("No ") + WHAT[constrain(cfg.sort, 0, 3)] + " signals right now";
  ui::list(toks, ntok, sel, n < 0 ? "ERR" : String(ntok), why);
}

int appRefresh() {
  int n = market::trending(toks, MAXTOK);
  ntok = n < 0 ? 0 : n;
  if (sel >= ntok) sel = 0;
  return n;
}

static void showResult(bool ok, const String& t, const String& a, const String& b, const String& c, State next) {
  ok ? ui::beepOk() : ui::beepErr();
  ui::result(ok, t, a, b, c);
  afterResult = next;
  resultAt.mark();
  state = S_RESULT;
}

static void doBuy() {
  Token t = toks[sel];
  ui::busy("BUY " + t.sym, "starting");
  String err, sig;
  bool ok = buy(t, cfg.armed, err, sig);
  if (!ok) { showResult(false, "BUY FAILED", err, "", "", S_LIST); return; }
  if (!cfg.armed) { showResult(true, "SIM OK", t.sym, "tx would succeed", "arm in Settings", S_LIST); return; }
  lastPriceNative = 0;
  lastPrice.clear();
  showResult(true, "BOUGHT " + t.sym, String(cfg.buySol, 3) + " SOL", sig.substring(0, 20), "", S_POSITION);
}

static void modeSave() {
  Preferences p;
  p.begin("pump", false);
  p.putInt("mode", (int)mode);
  String h;
  for (int i = 0; i < HIST_N; i++) if (hist[i].length()) h += (h.length() ? "," : "") + hist[i];
  p.putString("hist", h);
  p.end();
}

static void modeLoad() {
  Preferences p;
  p.begin("pump", true);
  mode = p.getInt("mode", 0) == 1 ? MODE_MINER : MODE_MANUAL;
  String h = p.getString("hist", p.getString("lastMint", ""));   // lastMint: old single-mint key
  for (int i = 0, from = 0; i < HIST_N; i++) {
    int c = h.indexOf(',', from);
    hist[i] = c < 0 ? h.substring(from) : h.substring(from, c);
    from = c < 0 ? h.length() : c + 1;
  }
  p.end();
}

static bool timeOk() { return time(nullptr) > 1700000000; }

// seconds the open position has been held
static int heldSec() {
  if (pos.openedAt && timeOk()) return (int)((uint32_t)time(nullptr) - pos.openedAt);
  if (!posFallbackStart.set()) posFallbackStart.mark();
  return posFallbackStart.ageSec();
}

static String clockUtc() {
  if (!timeOk()) return "";
  time_t t = time(nullptr);
  struct tm tm;
  gmtime_r(&t, &tm);
  char b[12];
  snprintf(b, sizeof b, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
  return String(b);
}

static State homeState() { return mode == MODE_MINER ? S_MINER : S_LIST; }

static void doSell() {
  posFallbackStart.clear();
  ui::busy("SELL " + pos.sym, "starting");
  String err, sig;
  int64_t delta = 0;
  double spent = (double)pos.spentLamports / 1e9;
  String sym = pos.sym;
  // an open position is real money: selling is always live, whatever the Armed flag says now
  bool ok = sell(true, err, sig, delta);
  if (!ok) { showResult(false, "SELL FAILED", err, "", "keep position, retry with A", pos.open ? S_POSITION : homeState()); return; }
  double got = (double)delta / 1e9;  // net of tx fees
  double pnl = spent > 0 ? (got / spent - 1.0) * 100.0 : 0;
  positionClear();
  showResult(true, "SOLD " + sym, "in " + String(spent, 4) + " out " + String(got + 0.0, 4),
             "PnL " + String(pnl >= 0 ? "+" : "") + String(pnl, 1) + "%", sig.substring(0, 20), homeState());
}

// ---- miner ----
static void minerBuy(const Token& t) {
  ui::busy("BUY " + t.sym, "starting");
  String err, sig;
  bool ok = buy(t, true, err, sig, cfg.minerBuySol);   // miner is always live
  if (!ok) { showResult(false, "BUY FAILED", err, t.sym, "", S_MINER); return; }
  minerBuys++;
  lastMinedSym = t.sym;
  lastBuy.mark();
  lastPriceNative = 0;
  lastPrice.clear();
  posFallbackStart.clear();
  showResult(true, "BOUGHT " + t.sym, String(min(cfg.minerBuySol, cfg.maxSol), 3) + " SOL", sig.substring(0, 20), "", S_POSITION);
}

// One radar check: buy the newest radar token unless it is the one already handled.
static void minerCheck() {
  lastCheck.mark();
  minerErr = "";
  if (!net::wifiUp()) net::wifiConnect();
  ui::busy("MINER", "checking radar...");
  market::minerMode = true;           // all signal types, scored + filtered, best candidate first
  int n = market::trending(toks, MAXTOK);
  market::minerMode = false;
  ntok = n < 0 ? 0 : n;
  sel = 0;
  if (n == -2) { minerErr = "Add Peep key"; return; }
  if (n == -3) { minerErr = "Peep key rejected"; return; }
  if (n < 0) { minerErr = "API: " + net::lastError; return; }
  if (n == 0) { minerErr = market::signalsDegraded ? "Peep API degraded" : "no strong signal"; return; }
  bool baseline = histEmpty();        // first ever run: remember what exists, don't buy a stale signal
  int pick = -1;
  for (int i = 0; i < n; i++) {       // best-scored token we have not handled yet
    if (baseline) { histPush(toks[i].mint); continue; }
    if (!inHist(toks[i].mint)) { pick = i; break; }
  }
  seenSym = toks[0].sym;              // shown on the miner screen even when not bought
  seenSigAge = toks[0].sigAgeSec;
  seenAt.mark();
  if (baseline) { modeSave(); return; }
  if (pick < 0) return;               // nothing new (all candidates already handled)
  Token t = toks[pick];
  histPush(t.mint);
  modeSave();                         // saved before buying: a failed buy is never retried in a loop
  ui::beepOk();
  minerBuy(t);
}

static void drawMiner() {
  lastMinerDraw.mark();
  int interval = cfg.minerIntervalSec;
  int checkedAgo = lastCheck.ageSec();
  int nextIn = checkedAgo >= 0 ? interval - checkedAgo : 0;
  int seenAge = (seenSym.length() && seenSigAge >= 0) ? seenSigAge + seenAt.ageSec() : -1;   // signal age, now
  ui::miner(nextIn, interval, checkedAgo, seenSym, seenAge, lastMinedSym, lastBuy.ageSec(), minerStart.ageSec(),
            minerBuys, clockUtc(), minerErr);
}

// ---- on-device miner settings ----
static const int SET_N = 8;
static const int SET_ROWS = 6;   // editable rows; the rest are Save / Exit
static const char* SET_LABEL[SET_N] = {"Buy SOL", "Hold sell", "Stop loss", "ATH down sell", "Timeout", "Check every", "Save", "Exit"};

template <size_t N>
static int cycleInt(int cur, const int (&vals)[N], int dir) {
  size_t best = 0;
  for (size_t i = 1; i < N; i++) if (abs(vals[i] - cur) < abs(vals[best] - cur)) best = i;
  return vals[(best + N + dir) % N];
}

static void settingsChange(int row, int dir) {
  static const int HOLD[] = {2, 3, 4}, SL[] = {20, 30, 50}, TR[] = {0, 10, 20, 30, 40, 50}, TO[] = {2, 5, 10, 15, 20, 30, 60}, IV[] = {10, 30, 60};
  switch (row) {
    case 0: {
      float hi = max(0.01f, cfg.maxSol), v = roundf((cfg.minerBuySol + dir * 0.01f) * 100) / 100;
      cfg.minerBuySol = v > hi ? 0.01f : (v < 0.01f ? hi : v);
      break;
    }
    case 1: cfg.minerHoldX = cycleInt(cfg.minerHoldX, HOLD, dir); break;
    case 2: cfg.minerSlPct = cycleInt(cfg.minerSlPct, SL, dir); break;
    case 3: cfg.minerTrailPct = cycleInt(cfg.minerTrailPct, TR, dir); break;
    case 4: cfg.minerTimeoutMin = cycleInt(cfg.minerTimeoutMin, TO, dir); break;
    case 5: cfg.minerIntervalSec = cycleInt(cfg.minerIntervalSec, IV, dir); break;
  }
}

static void drawSettings() {
  String vals[SET_N], labs[SET_N];
  for (int i = 0; i < SET_N; i++) labs[i] = SET_LABEL[i];
  vals[0] = String(cfg.minerBuySol, 2);
  vals[1] = String(cfg.minerHoldX) + "x";
  vals[2] = "-" + String(cfg.minerSlPct) + "%";
  vals[3] = cfg.minerTrailPct > 0 ? "-" + String(cfg.minerTrailPct) + "%" : String("off");
  vals[4] = String(cfg.minerTimeoutMin) + " min";
  vals[5] = String(cfg.minerIntervalSec) + " s";
  ui::settings(labs, vals, SET_N, setCur);
}

static void showMenu() {
  state = S_MENU;
  ui::menu(menuCur);
}

static void updatePosition(bool force) {
  if (!force && !lastPrice.due(tmr::PRICE_REFRESH_MS)) return;
  lastPrice.mark();
  double p;
  if (chainPrice(pos.mint, p) || market::price(pos.mint, p)) { lastPriceNative = p; lastPriceOk.mark(); }
  bool stale = lastPriceNative == 0 || lastPriceOk.due(tmr::PRICE_STALE_MS);
  double spent = (double)pos.spentLamports / 1e9;
  double val = positionValueSol(lastPriceNative);
  double pnl = lastPriceNative > 0 ? positionPnlPct(lastPriceNative) : 0;
  if (mode == MODE_MINER) {
    // exits: stop loss / hold-x target / timeout, first wins. Timeout sells even if the price is stale.
    int held = heldSec(), left = cfg.minerTimeoutMin * 60 - held;
    // ATH down sell: track the highest price since buy, sell when price falls minerTrailPct below it
    static bool peakDirty = false;
    static tmr::Stamp peakSave;
    if (!stale && lastPriceNative > pos.peakPrice) { pos.peakPrice = lastPriceNative; peakDirty = true; }
    if (peakDirty && peakSave.due(tmr::PEAK_SAVE_MS)) { positionSave(); peakSave.mark(); peakDirty = false; }
    double peakPnl = pos.peakPrice > 0 ? positionPnlPct(pos.peakPrice) : 0;
    double drop = (pos.peakPrice > 0 && lastPriceNative > 0) ? (1.0 - lastPriceNative / pos.peakPrice) * 100.0 : 0;
    bool trailHit = cfg.minerTrailPct > 0 && !stale && drop >= cfg.minerTrailPct;
    ui::minerPosition(pos.sym, spent, val, pnl, held, left, cfg.minerHoldX, cfg.minerSlPct, cfg.minerTrailPct, peakPnl, drop, stale);
    bool hit = left <= 0 || (!stale && (pnl <= -cfg.minerSlPct || pnl >= (cfg.minerHoldX - 1) * 100.0)) || trailHit;
    if (hit) { ui::beepOk(); doSell(); }
    return;
  }
  ui::position(pos.sym, pos.mint, spent, val, pnl, cfg.tpPct, cfg.slPct, cfg.armed, stale);
  if (cfg.armed && !stale) {
    if ((cfg.tpPct > 0 && pnl >= cfg.tpPct) || (cfg.slPct > 0 && pnl <= -cfg.slPct)) {
      ui::beepOk();
      doSell();
    }
  }
}

#define DBG(s) do { Serial.print("# "); Serial.println(s); Serial.flush(); } while (0)
void setup() {
  auto m5cfg = M5.config();
  M5.begin(m5cfg);
  Serial.begin(115200);
  DBG("m5 begin ok");
  ui::begin();
  DBG("ui begin ok");
  configLoad();
  DBG("config ok");
  wallet::load();
  DBG("wallet ok");
  positionLoad();
  DBG("position ok");
  ui::setBrightness(cfg.brightness);
  DBG("brightness ok");
  onProgress = progress;
  net::idleHook = []() { proto::poll(); };
  modeLoad();
  ui::boot("Peep meme miner", "v" FW_VERSION);
  // short window so the Settings page can reach us right after the USB reset
  for (uint32_t t0 = millis(); millis() - t0 < tmr::BOOT_WINDOW_MS;) { proto::poll(); delay(10); }
  lastActivity.mark();

  DBG("entering setup check");
  if (!setupComplete()) {
    showSetup(!wifiConfigured() ? "Wi-Fi not set" : (!wallet::has() ? "Wallet missing" : "Peep key missing"));
    return;
  }
  ui::boot("Wi-Fi...", cfg.ssid[0]);
  if (!net::wifiConnect()) { showSetup("Wi-Fi failed: check name/pass"); return; }
  net::syncTime();
  if (pos.open) { state = S_POSITION; updatePosition(true); return; }
  showMenu();
}

void loop() {
  M5.update();
  ui::beepUpdate();
  proto::poll();

  bool a = M5.BtnA.wasPressed();
  bool b = M5.BtnB.wasPressed();
  uint8_t pw = M5.BtnPWR.wasHold() ? 1 : (M5.BtnPWR.wasClicked() ? 2 : 0);  // 1 long, 2 short
  if (a || b || pw) { lastActivity.mark(); ui::beepTick(); }

  // An open position pins the UI to the position screen (and the setup screen if offline setup is lost).
  if (pos.open && state != S_POSITION && state != S_CONFIRM_SELL && state != S_RESULT && state != S_SETUP) {
    state = S_POSITION;
    updatePosition(true);
  }

  switch (state) {
    case S_SETUP:
      if (setupComplete()) {       // Settings page finished: reboot straight into the app
        ui::boot("Setup done", "starting...");
        delay(900);
        ESP.restart();
      }
      {
        static int lastSig = -1;
        int sig = (wifiConfigured() ? 1 : 0) | (wallet::has() ? 2 : 0) | (cfg.peepKey.length() ? 4 : 0);
        if (sig != lastSig || a || pw == 2) {   // redraw whenever the Settings page changes something
          lastSig = sig;
          setupProblem = !wifiConfigured() ? "Wi-Fi not set" : (!wallet::has() ? "Wallet missing" : (!cfg.peepKey.length() ? "Peep key missing" : ""));
          showSetup();
        }
      }
      break;

    case S_MENU:
      if (b) { menuCur = (menuCur + 1) % 3; ui::menu(menuCur); }
      if (pw == 2) { menuCur = (menuCur + 2) % 3; ui::menu(menuCur); }
      if (a) {
        if (menuCur == 0) { mode = MODE_MANUAL; modeSave(); refreshList(); state = S_LIST; }
        else if (menuCur == 1) {
          mode = MODE_MINER; modeSave();
          minerStart.mark(); lastCheck.clear(); minerErr = "";
          state = S_MINER;
          drawMiner();
        } else { setCur = 0; state = S_SETTINGS; drawSettings(); }
      }
      break;

    case S_SETTINGS:
      if (b) { setCur = (setCur + 1) % SET_N; drawSettings(); }
      if (pw == 2 && setCur < SET_ROWS) { settingsChange(setCur, -1); drawSettings(); }
      if (a) {
        if (setCur < SET_ROWS) { settingsChange(setCur, +1); drawSettings(); }
        else {
          if (setCur == SET_ROWS) { configSave(); ui::beepOk(); } else configLoad();   // Exit discards edits
          showMenu();
        }
      }
      break;

    case S_MINER:
      if (!minerStart.set()) minerStart.mark();             // resumed after boot/menu: "run" counts from now
      if (pw == 1) { showMenu(); pw = 0; break; }           // hold power: back to menu
      {
        // shake = manual refresh: check now and restart the interval timer
        static tmr::Stamp lastShake;
        if (M5.Imu.update() && lastShake.due(tmr::SHAKE_DEBOUNCE_MS)) {
          auto d = M5.Imu.getImuData();
          float g = sqrtf(d.accel.x * d.accel.x + d.accel.y * d.accel.y + d.accel.z * d.accel.z);
          if (g > tmr::SHAKE_G) { lastShake.mark(); ui::beepTick(); lastCheck.clear(); }
        }
      }
      if (lastCheck.due((uint32_t)cfg.minerIntervalSec * 1000UL)) {
        minerCheck();
        if (state == S_MINER) drawMiner();
      } else if (lastMinerDraw.due(tmr::MINER_REDRAW_MS)) {
        drawMiner();
      }
      break;

    case S_LIST:
      if (M5.BtnA.wasHold()) { showMenu(); break; }
      if (b && ntok) { sel = (sel + 1) % ntok; ui::list(toks, ntok, sel, String(ntok)); }                 // next
      if (pw == 2 && ntok) { sel = (sel + ntok - 1) % ntok; ui::list(toks, ntok, sel, String(ntok)); }  // back
      if (M5.BtnB.wasHold() || (lastList.due(tmr::LIST_REFRESH_MS) && net::wifiUp())) refreshList();
      if (a && ntok) {
        ui::confirmBuy(toks[sel], min(cfg.buySol, cfg.maxSol), cfg.armed);
        state = S_CONFIRM_BUY;
      }
      if (cfg.idleMin > 0 && lastActivity.ageMs() > (uint32_t)cfg.idleMin * tmr::IDLE_UNIT_MS) {
        ui::boot("Idle: powering off", "press power to start");
        delay(800);
        M5.Power.powerOff();   // full power off; press the power button to start
      }
      break;

    case S_CONFIRM_BUY:
      if (M5.BtnA.wasHold()) { showMenu(); break; }   // hold A in the list: back to menu
      if (b) { ui::list(toks, ntok, sel, String(ntok)); state = S_LIST; }
      if (a) { doBuy(); }
      break;

    case S_POSITION:
      if (!pos.open) {
        if (mode == MODE_MINER) { state = S_MINER; lastMinerDraw.clear(); }
        else { state = S_LIST; refreshList(); }
        break;
      }
      updatePosition(false);
      if (a) { ui::confirmSell(pos.sym, lastPriceNative > 0 ? positionPnlPct(lastPriceNative) : 0); state = S_CONFIRM_SELL; }
      // B intentionally does nothing: the position screen stays until the token is sold.
      break;

    case S_CONFIRM_SELL:
      if (b) { state = S_POSITION; updatePosition(true); }
      if (a) doSell();
      break;

    case S_RESULT:
      // miner: no click needed, result shows 2.5 s then continues on its own
      if (a || b || (mode == MODE_MINER && resultAt.ageMs() > tmr::RESULT_AUTO_MS)) {
        state = pos.open ? S_POSITION : afterResult;
        if (state == S_LIST) { if (ntok == 0) refreshList(); else ui::list(toks, ntok, sel, String(ntok)); }
        else if (state == S_MINER) { lastMinerDraw.clear(); }
        else if (state == S_POSITION) {
          if (pos.open) updatePosition(true);
          else if (mode == MODE_MINER) { state = S_MINER; lastMinerDraw.clear(); }
          else { state = S_LIST; refreshList(); }
        }
      }
      break;
  }

  if (pw == 1) M5.Power.powerOff();  // long power press
  delay(10);
}
