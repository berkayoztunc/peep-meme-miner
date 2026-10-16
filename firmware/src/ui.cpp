#include "ui.h"
#include "config.h"
#include "logo_data.h"
#include <M5Unified.h>

namespace ui {
static M5Canvas spr;   // composed off-screen, pushed to the LCD in one go
static void T(const String& s, int x, int y, int font) {
  spr.setTextFont(font);
  spr.drawString(s.c_str(), x, y);
}
static const uint16_t UI_BG = TFT_BLACK;
static const uint16_t UI_CYAN = 0x07FF;
static const uint16_t UI_GREY = 0x8410;

void begin() {
  M5.Display.setRotation(1);
  M5.Speaker.setVolume(160);
  spr.setColorDepth(16);
  spr.setPsram(psramFound());          // 64 KB frame buffer: keep internal RAM free for TLS
  spr.createSprite(240, 135);
  spr.fillSprite(UI_BG);
}

void setBrightness(int pct) {
  M5.Display.setBrightness(map(constrain(pct, 0, 100), 0, 100, 16, 255));
}

static void tone(int f, int ms) { M5.Speaker.tone(f, ms); }
void beepOk() { tone(2400, 120); }
void beepErr() { tone(400, 500); }
void beepTick() { tone(1800, 25); }
void beepUpdate() {}

// Battery level for the top bar. Cached: read at most every 5 s.
static int batteryPct(bool& charging) {
  static uint32_t last = 0;
  static int lvl = -1;
  static bool chg = false;
  if (last == 0 || millis() - last > 5000) {
    last = millis();
    lvl = M5.Power.getBatteryLevel();
    chg = M5.Power.isCharging() == m5::Power_Class::is_charging;
  }
  charging = chg;
  return lvl;
}

static const uint16_t UI_ORANGE = 0xFD20;   // top bar colour

static void header(const String& title, uint16_t color = UI_ORANGE) {
  spr.fillRect(0, 0, 240, 18, color);
  spr.setTextColor(UI_BG, color);
  spr.setTextDatum(TL_DATUM);
  T(title, 4, 2, 2);
  // battery, right side of the bar: [icon] 87%
  bool chg = false;
  int lvl = batteryPct(chg);
  String s = lvl < 0 ? String("--") : String(lvl) + "%";
  if (chg) s = "+" + s;
  spr.setTextDatum(TR_DATUM);
  T(s, 214, 2, 2);
  spr.setTextDatum(TL_DATUM);
  // battery icon (x 218..236), fill proportional to level
  spr.drawRect(218, 4, 18, 10, UI_BG);
  spr.fillRect(236, 7, 2, 4, UI_BG);
  if (lvl > 0) spr.fillRect(220, 6, max(1, (14 * lvl) / 100), 6, UI_BG);
}
static void footer(const String& s) {
  spr.setTextColor(UI_GREY, UI_BG);
  spr.setTextDatum(BL_DATUM);
  T(s, 4, 134, 2);
}
static void push() { spr.pushSprite(&M5.Display, 0, 0); }

static String pct(double v) { return String(v >= 0 ? "+" : "") + String(v, 1) + "%"; }
static String fmtMc(float m) {
  if (m >= 1e6) return String(m / 1e6, 1) + "M";
  if (m >= 1e3) return String(m / 1e3, 0) + "k";
  return String(m, 0);
}

static String fmtAge(int s) {
  if (s < 0) return "?";
  if (s < 90) return String(s) + "s";
  if (s < 5400) return String(s / 60) + "m";
  if (s < 172800) return String(s / 3600) + "h";
  return String(s / 86400) + "d";
}

// Logo is the default on every screen: top-right under the header. Content stays left of it.
static const int LX = 182, LY = 24, LS = 52;     // logo box
static const int CW = 172;                       // content width left of the logo

static void drawLogo(int x, int y, int sz) {
  float sc = (float)sz / LOGO_W;
  spr.drawPng(LOGO_PNG, LOGO_PNG_LEN, x, y, sz, sz, 0, 0, sc, sc);
}
static void logo() { drawLogo(LX, LY, LS); }

// Word-wrap `s` inside maxW pixels. Returns the y after the last line.
static int wrapped(const String& s, int x, int y, int maxW, int font, int lineH, int maxLines) {
  spr.setTextFont(font);
  String line;
  int lines = 0;
  int i = 0;
  const int n = s.length();
  while (i < n && lines < maxLines) {
    int j = s.indexOf(' ', i);
    String word = j < 0 ? s.substring(i) : s.substring(i, j);
    i = j < 0 ? n : j + 1;
    while (word.length() > 0) {                       // hard-split words wider than the box
      String trial = line.length() ? line + " " + word : word;
      if (spr.textWidth(trial.c_str()) <= maxW) { line = trial; word = ""; break; }
      if (line.length()) {                             // flush the current line first
        spr.drawString(line.c_str(), x, y); y += lineH; lines++; line = "";
        if (lines >= maxLines) return y;
        continue;
      }
      int k = word.length();
      while (k > 1 && spr.textWidth(word.substring(0, k).c_str()) > maxW) k--;
      spr.drawString(word.substring(0, k).c_str(), x, y); y += lineH; lines++;
      word = word.substring(k);
      if (lines >= maxLines) return y;
    }
  }
  if (line.length() && lines < maxLines) { spr.drawString(line.c_str(), x, y); y += lineH; }
  return y;
}

void boot(const String& l1, const String& l2) {
  spr.fillSprite(UI_BG);
  header("PEEP MINER");
  logo();
  spr.setTextColor(TFT_WHITE, UI_BG);
  wrapped(l1, 8, 30, CW, 2, 18, 2);
  spr.setTextColor(UI_GREY, UI_BG);
  wrapped(l2, 8, 70, CW, 2, 18, 2);
  push();
}

void setup(const String& pubkey, bool wifiCfg, bool walletOk, bool peepOk, const String& problem) {
  spr.fillSprite(UI_BG);
  header("SETUP NEEDED", TFT_ORANGE);
  logo();
  auto row = [&](int y, bool ok, const char* name, const char* todo, bool optional) {
    spr.setTextColor(ok ? TFT_GREEN : (optional ? UI_GREY : TFT_ORANGE), UI_BG);
    T(ok ? "OK" : (optional ? "--" : "!!"), 6, y, 2);
    spr.setTextColor(TFT_WHITE, UI_BG);
    T(name, 34, y, 2);
    if (!ok) {
      spr.setTextColor(UI_GREY, UI_BG);
      T(todo, 34, y + 16, 1);
    }
  };
  row(22, wifiCfg, "Wi-Fi", "add SSID + password", false);
  row(50, walletOk, "Wallet", "generate or import", false);
  row(78, peepOk, "Peep key", "needed for the token list", false);
  if (problem.length() > 0) {
    spr.setTextColor(TFT_RED, UI_BG);
    wrapped(problem, LX - 2, LY + LS + 6, 240 - LX, 1, 10, 3);
  }
  spr.setTextColor(UI_CYAN, UI_BG);
  if (pubkey.length() > 0) T(pubkey.substring(0, 22) + "..", 6, 108, 1);
  footer("USB > Settings page");
  push();
}

static String fmtPrice(float p) {
  if (p <= 0) return "?";
  if (p >= 0.01f) return "$" + String(p, 4);
  char b[16];
  snprintf(b, sizeof b, "$%.2e", p);
  return String(b);
}

// One token per screen. B = next, Power = back, A = buy. Data: Peep partner signals only.
void list(const Token* t, int n, int sel, const String& status, const String& emptyMsg) {
  static const char* MODE[] = {"RADAR", "THESIS", "CALL", "ALL"};
  spr.fillSprite(UI_BG);
  header(String("PEEP ") + MODE[constrain(cfg.sort, 0, 3)]);
  logo();
  if (n == 0) {
    spr.setTextColor(emptyMsg.length() ? TFT_ORANGE : UI_GREY, UI_BG);
    wrapped(emptyMsg.length() ? emptyMsg : String("no tokens found"), 8, 30, CW, 2, 18, 3);
    footer("Hold B: refresh");
    push();
    return;
  }
  const Token& c = t[sel];
  spr.setTextColor(TFT_WHITE, UI_BG);
  T(c.sym, 8, 22, 4);
  T("MC $" + fmtMc(c.mcap), 8, 52, 2);
  spr.setTextColor(UI_GREY, UI_BG);
  T(fmtPrice(c.priceUsd), 8, 70, 2);
  T("age " + fmtAge(c.ageSec), 8, 88, 2);

  // under the logo: counter
  spr.setTextColor(TFT_WHITE, UI_BG);
  spr.setTextDatum(TC_DATUM);
  T(String(sel + 1) + "/" + String(n), LX + LS / 2, LY + LS + 6, 2);
  spr.setTextDatum(TL_DATUM);

  // signal tags: matched type first, freshest first
  spr.setTextColor(UI_ORANGE, UI_BG);
  wrapped(c.signals, 8, 108, 232, 1, 10, 1);
  footer("A:Buy B:Next Pwr:Back");
  push();
}

void confirmBuy(const Token& t, float sol, bool armed) {
  spr.fillSprite(UI_BG);
  header(t.sym, armed ? UI_ORANGE : TFT_YELLOW);
  logo();
  spr.setTextColor(TFT_WHITE, UI_BG);
  T("MC $" + fmtMc(t.mcap), 8, 24, 2);
  spr.setTextColor(UI_GREY, UI_BG);
  T(fmtPrice(t.priceUsd), 8, 42, 2);
  T("age " + fmtAge(t.ageSec), 8, 60, 2);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(UI_CYAN, UI_BG);
  T("BUY " + String(sol, 3) + " SOL", 120, 90, 4);
  spr.setTextColor(armed ? TFT_GREEN : TFT_YELLOW, UI_BG);
  T(armed ? "LIVE" : "SIMULATE ONLY", 120, 112, 2);
  spr.setTextDatum(TL_DATUM);
  footer("A:Confirm  B:Cancel");
  push();
}

void busy(const String& title, const String& line) {
  spr.fillSprite(UI_BG);
  header(title);
  logo();
  spr.setTextColor(TFT_WHITE, UI_BG);
  wrapped(line, 8, 52, CW, 2, 18, 3);
  push();
}

void position(const String& sym, const String& mint, double spent, double value, double p, int tp, int sl,
              bool armed, bool stale) {
  (void)mint;
  spr.fillSprite(UI_BG);
  header(sym + (armed ? "" : " (sim)"), p >= 0 ? TFT_GREEN : TFT_RED);
  logo();
  spr.setTextColor(UI_GREY, UI_BG);
  T("in  " + String(spent, 4) + " SOL", 8, 22, 2);
  T("now " + String(value, 4) + " SOL", 8, 40, 2);
  String s = pct(p);
  spr.setTextColor(p >= 0 ? TFT_GREEN : TFT_RED, UI_BG);
  T(s, 8, 62, s.length() > 7 ? 4 : 6);          // shrink the big number if it would hit the logo
  spr.setTextColor(UI_GREY, UI_BG);
  String l = stale ? "price stale  " : "";
  if (tp) l += "TP " + String(tp) + "%  ";
  if (sl) l += "SL -" + String(sl) + "%";
  T(l, 8, 112, 1);
  footer("A: SELL  (open position)");
  push();
}

void confirmSell(const String& sym, double p) {
  spr.fillSprite(UI_BG);
  header("SELL " + sym, TFT_ORANGE);
  logo();
  String s = pct(p);
  spr.setTextColor(p >= 0 ? TFT_GREEN : TFT_RED, UI_BG);
  T(s, 8, 36, s.length() > 7 ? 4 : 6);
  spr.setTextColor(TFT_WHITE, UI_BG);
  T("Sell everything?", 8, 96, 2);
  footer("A:Yes sell  B:Back");
  push();
}

void result(bool ok, const String& title, const String& l1, const String& l2, const String& l3) {
  spr.fillSprite(UI_BG);
  header(title, ok ? TFT_GREEN : TFT_RED);
  logo();
  spr.setTextColor(TFT_WHITE, UI_BG);
  int y = wrapped(l1, 8, 24, CW, 2, 18, 3);
  y = wrapped(l2, 8, y + 4, CW, 2, 18, 2);
  spr.setTextColor(UI_GREY, UI_BG);
  wrapped(l3, 8, y + 4, CW, 1, 10, 2);
  footer("A/B: continue");
  push();
}

static String fmtDur(int s) {
  if (s < 0) return "?";
  char b[12];
  if (s >= 3600) snprintf(b, sizeof b, "%dh%02dm", s / 3600, (s / 60) % 60);
  else snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
  return String(b);
}

void menu(int cur) {
  static const char* ITEMS[] = {"MANUAL", "MINER", "SETTINGS"};
  spr.fillSprite(UI_BG);
  header("MINER MENU");
  logo();
  for (int i = 0; i < 3; i++) {
    int y = 24 + i * 32;
    if (i == cur) {
      spr.fillRect(4, y - 2, CW, 30, UI_CYAN);
      spr.setTextColor(UI_BG, UI_CYAN);
    } else {
      spr.setTextColor(TFT_WHITE, UI_BG);
    }
    T(ITEMS[i], 12, y, 4);
  }
  spr.setTextColor(UI_GREY, UI_BG);
  footer("A:Select B:Next Pwr:Prev");
  push();
}

void settings(const String* labels, const String* values, int n, int cur) {
  spr.fillSprite(UI_BG);
  header("MINER SETTINGS");
  int first = constrain(cur - 2, 0, max(0, n - 5));
  for (int r = 0; r < 5 && first + r < n; r++) {
    int i = first + r, y = 22 + r * 21;
    bool on = i == cur;
    if (on) spr.fillRect(0, y - 1, 240, 20, UI_CYAN);
    spr.setTextColor(on ? UI_BG : TFT_WHITE, on ? UI_CYAN : UI_BG);
    T(labels[i], 6, y, 2);
    spr.setTextDatum(TR_DATUM);
    T(values[i], 232, y, 2);
    spr.setTextDatum(TL_DATUM);
  }
  spr.setTextColor(UI_GREY, UI_BG);
  footer("A:Up Pwr:Down B:Next");
  push();
}

void miner(int nextIn, int interval, int lastCheckAgo, const String& seenSym, int seenAge, const String& boughtSym,
           int lastBuyAgo, int runSec, int buys, const String& clockUtc, const String& err) {
  spr.fillSprite(UI_BG);
  header("MINER ON", TFT_GREEN);
  logo();
  spr.setTextColor(TFT_WHITE, UI_BG);
  T("next " + String(max(0, nextIn)) + "s /" + String(interval) + "s", 8, 20, 2);
  spr.setTextColor(UI_GREY, UI_BG);
  T("checked " + (lastCheckAgo < 0 ? String("-") : fmtDur(lastCheckAgo) + " ago"), 8, 36, 2);
  spr.setTextColor(UI_CYAN, UI_BG);
  T("seen " + (seenSym.length() ? seenSym + (seenAge >= 0 ? " " + fmtDur(seenAge) : String("")) : String("-")), 8, 52, 2);
  spr.setTextColor(UI_GREY, UI_BG);
  T("bought " + (boughtSym.length() ? boughtSym + " " + fmtDur(lastBuyAgo) : String("-")), 8, 68, 2);
  T("run " + fmtDur(runSec) + "  buys " + String(buys), 8, 84, 2);
  if (err.length()) {
    spr.setTextColor(TFT_RED, UI_BG);
    wrapped(err, 8, 102, 232, 1, 10, 2);
  } else {
    spr.setTextColor(UI_CYAN, UI_BG);
    T(clockUtc.length() ? "UTC " + clockUtc : String("clock not synced"), 8, 102, 2);
  }
  footer("Hold Pwr: menu");
  push();
}

void minerPosition(const String& sym, double spent, double value, double p, int held, int left, int holdX,
                   int sl, int trail, double peakPct, double dropPct, bool stale) {
  spr.fillSprite(UI_BG);
  header(sym + " MINER", p >= 0 ? TFT_GREEN : TFT_RED);
  logo();
  spr.setTextColor(UI_GREY, UI_BG);
  T("in " + String(spent, 3) + " now " + String(value, 3), 8, 22, 2);
  String s = pct(p);
  spr.setTextColor(p >= 0 ? TFT_GREEN : TFT_RED, UI_BG);
  T(s, 8, 44, s.length() > 7 ? 4 : 6);
  spr.setTextColor(TFT_WHITE, UI_BG);
  T("held " + fmtDur(held) + "  left " + fmtDur(max(0, left)), 8, 92, 2);
  spr.setTextColor(UI_CYAN, UI_BG);
  T(String(stale ? "stale " : "") + "pk " + pct(peakPct) + " dn " + String(max(0.0, dropPct), 0) + "/" +
        (trail > 0 ? String(trail) : String("off")) + "% " + String(holdX) + "x -" + String(sl) + "%",
    8, 110, 1);
  footer("A: SELL now");
  push();
}
}  // namespace ui
