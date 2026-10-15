#include "proto.h"
#include <ArduinoJson.h>
#include <M5Unified.h>
#include <WiFi.h>
#include "config.h"
#include "net.h"
#include "market.h"
#include "addrs.h"
#include "pump.h"
#include "ui.h"
#include "wallet.h"

#define FW_VERSION "1.1.0"

void appTokens(JsonArray out);
int appRefresh();   // runs the list refresh, returns count (<0 error)

namespace proto {
static String line;

static void reply(JsonDocument& d) {
  serializeJson(d, Serial);
  Serial.print('\n');
}

static void ok(const char* id, JsonDocument& d) {
  d["ok"] = true;
  if (id) d["id"] = id;
  reply(d);
}

static void fail(const char* id, const char* msg) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = msg;
  if (id) d["id"] = id;
  reply(d);
}

static bool confirmOnDevice(const char* what) {
  uint32_t t0 = millis();
  ui::busy("CONFIRM", String(what) + " ? A=yes B=no");
  while (millis() - t0 < 15000) {
    M5.update();
    if (M5.BtnA.wasPressed()) return true;
    if (M5.BtnB.wasPressed()) return false;
    delay(20);
  }
  return false;
}

static void handle(const String& s) {
  JsonDocument in;
  if (deserializeJson(in, s)) return;  // ignore non-JSON noise
  const char* cmd = in["cmd"] | "";
  const char* id = in["id"].is<const char*>() ? in["id"].as<const char*>() : nullptr;
  JsonDocument d;

  if (!strcmp(cmd, "hello")) {
    d["fw"] = FW_VERSION;
    d["pubkey"] = wallet::pubStr();
    d["armed"] = cfg.armed;
    d["wallet"] = wallet::has();
    d["wifi"] = net::wifiUp();
    ok(id, d);
  } else if (!strcmp(cmd, "get_config")) {
    configToJson(d["config"].to<JsonObject>());
    d["pubkey"] = wallet::pubStr();
    ok(id, d);
  } else if (!strcmp(cmd, "set_config")) {
    configFromJson(in["config"].as<JsonObjectConst>());
    configSave();
    ui::setBrightness(cfg.brightness);
    configToJson(d["config"].to<JsonObject>());
    ok(id, d);
  } else if (!strcmp(cmd, "set_wallet")) {
    if (pos.open) { fail(id, "close position first"); return; }
    if (!wallet::importB58(in["secret_b58"] | "")) { fail(id, "invalid secret"); return; }
    d["pubkey"] = wallet::pubStr();
    ok(id, d);
  } else if (!strcmp(cmd, "gen_wallet")) {
    if (wallet::has() && !confirmOnDevice("Replace wallet")) { fail(id, "declined on device"); return; }
    wallet::generate();
    d["pubkey"] = wallet::pubStr();
    ok(id, d);
  } else if (!strcmp(cmd, "status")) {
    d["heap"] = ESP.getFreeHeap();
    d["battery_v"] = (M5.Power.getBatteryVoltage() / 1000.0f);
    d["wifi"] = net::wifiUp();
    d["rssi"] = WiFi.RSSI();
    d["position_open"] = pos.open;
    d["position"] = pos.sym;
    d["max_block"] = ESP.getMaxAllocHeap();
    d["psram"] = ESP.getPsramSize();
    d["psram_free"] = ESP.getFreePsram();
    d["min_heap"] = ESP.getMinFreeHeap();
    ok(id, d);
  } else if (!strcmp(cmd, "diag")) {
    // Fetch a few endpoints and report status, error text and heap, to find connection problems.
    struct Probe { const char* name; String url; const char* h1n; String h1v; };
    Probe probes[] = {
      {"peep", "https://peep-market-realtime.fly.dev/peep-partner-api/v1/signals", "Authorization", "Bearer " + cfg.peepKey},
      {"birdeye", "https://public-api.birdeye.so/defi/price?address=So11111111111111111111111111111111111111112", "X-API-KEY", cfg.birdeye},
      {"dexscreener", "https://api.dexscreener.com/token-boosts/top/v1", nullptr, ""},
      {"orquestra", "https://api.orquestra.dev/api/6i6q26bmm46b89xlxo1kv/instructions", nullptr, ""},
      {"rpc", cfg.rpc, nullptr, ""},
    };
    JsonArray arr = d["probes"].to<JsonArray>();
    d["heap_before"] = ESP.getFreeHeap();
    d["max_block_before"] = ESP.getMaxAllocHeap();
    for (auto& pr : probes) {
      JsonDocument f, r;
      f["success"] = true;                      // tiny filter: we only care about the transport result
      uint32_t t0 = millis();
      uint32_t mb = ESP.getMaxAllocHeap();
      int code = net::json(pr.url, r, &f, "GET", nullptr, pr.h1n, pr.h1v.c_str());
      JsonObject o = arr.add<JsonObject>();
      o["name"] = pr.name;
      o["code"] = code;
      o["ms"] = millis() - t0;
      o["max_block"] = mb;
      if (code < 0 || code >= 400) o["err"] = net::lastError;
    }
    d["wifi"] = net::wifiUp();
    d["rssi"] = WiFi.RSSI();
    d["ip"] = WiFi.localIP().toString();
    ok(id, d);
  } else if (!strcmp(cmd, "price")) {
    String mint = in["mint"] | "";
    double chain = 0, dex = 0;
    d["chain_ok"] = chainPrice(mint, chain);
    d["chain_px"] = chain;
    d["dex_ok"] = market::price(mint, dex);
    d["dex_px"] = dex;
    ok(id, d);
  } else if (!strcmp(cmd, "refresh")) {
    uint32_t t0 = millis();
    int n = appRefresh();
    d["count"] = n;
    d["ms"] = millis() - t0;
    if (n < 0) d["err"] = net::lastError;
    d["max_block"] = ESP.getMaxAllocHeap();
    d["min_heap"] = ESP.getMinFreeHeap();
    ok(id, d);
  } else if (!strcmp(cmd, "tokens")) {
    appTokens(d["tokens"].to<JsonArray>());
    d["degraded"] = false;
    ok(id, d);
  } else if (!strcmp(cmd, "wipe")) {
    if (!confirmOnDevice("ERASE ALL")) { fail(id, "declined on device"); return; }
    wallet::wipe();
    positionClear();
    cfg = Config();
    configSave();
    ok(id, d);
    delay(200);
    ESP.restart();
  } else if (!strcmp(cmd, "reboot")) {
    ok(id, d);
    delay(200);
    ESP.restart();
  } else {
    fail(id, "unknown cmd");
  }
}

void poll() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      line.trim();
      if (line.length() > 1 && line[0] == '{') handle(line);
      line = "";
    } else if (c != '\r') {
      if (line.length() < 3000) line += c;
    }
  }
}
}  // namespace proto
