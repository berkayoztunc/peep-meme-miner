#include "config.h"
#include <Preferences.h>

Config cfg;

void configLoad() {
  Preferences p;
  p.begin("pump", true);
  for (int i = 0; i < 3; i++) {
    cfg.ssid[i] = p.getString(("ssid" + String(i)).c_str(), "");
    cfg.pass[i] = p.getString(("pass" + String(i)).c_str(), "");
  }
  cfg.rpc = p.getString("rpc", cfg.rpc);
  cfg.birdeye = p.getString("birdeye", "");
  cfg.peepKey = p.getString("peep", "");
  cfg.buySol = p.getFloat("buySol", cfg.buySol);
  cfg.maxSol = p.getFloat("maxSol", cfg.maxSol);
  cfg.slipBps = p.getInt("slipBps", cfg.slipBps);
  cfg.prioMicro = p.getUInt("prio", cfg.prioMicro);
  cfg.cuLimit = p.getUInt("cuLimit", cfg.cuLimit);
  cfg.tpPct = p.getInt("tp", cfg.tpPct);
  cfg.slPct = p.getInt("sl", cfg.slPct);
  cfg.minLiqUsd = p.getFloat("minLiq", cfg.minLiqUsd);
  cfg.minMcapUsd = p.getFloat("minMcap", cfg.minMcapUsd);
  cfg.sort = p.getInt("sort", cfg.sort);
  cfg.armed = p.getBool("armed", false);
  cfg.brightness = p.getInt("bright", cfg.brightness);
  cfg.idleMin = p.getInt("idle", cfg.idleMin);
  cfg.minerBuySol = p.getFloat("mBuy", cfg.minerBuySol);
  cfg.minerHoldX = p.getInt("mX", cfg.minerHoldX);
  cfg.minerSlPct = p.getInt("mSl", cfg.minerSlPct);
  cfg.minerTimeoutMin = p.getInt("mTo", cfg.minerTimeoutMin);
  cfg.minerIntervalSec = p.getInt("mInt", cfg.minerIntervalSec);
  p.end();
}

void configSave() {
  Preferences p;
  p.begin("pump", false);
  for (int i = 0; i < 3; i++) {
    p.putString(("ssid" + String(i)).c_str(), cfg.ssid[i]);
    p.putString(("pass" + String(i)).c_str(), cfg.pass[i]);
  }
  p.putString("rpc", cfg.rpc);
  p.putString("birdeye", cfg.birdeye);
  p.putString("peep", cfg.peepKey);
  p.putFloat("buySol", cfg.buySol);
  p.putFloat("maxSol", cfg.maxSol);
  p.putInt("slipBps", cfg.slipBps);
  p.putUInt("prio", cfg.prioMicro);
  p.putUInt("cuLimit", cfg.cuLimit);
  p.putInt("tp", cfg.tpPct);
  p.putInt("sl", cfg.slPct);
  p.putFloat("minLiq", cfg.minLiqUsd);
  p.putFloat("minMcap", cfg.minMcapUsd);
  p.putInt("sort", cfg.sort);
  p.putBool("armed", cfg.armed);
  p.putInt("bright", cfg.brightness);
  p.putInt("idle", cfg.idleMin);
  p.putFloat("mBuy", cfg.minerBuySol);
  p.putInt("mX", cfg.minerHoldX);
  p.putInt("mSl", cfg.minerSlPct);
  p.putInt("mTo", cfg.minerTimeoutMin);
  p.putInt("mInt", cfg.minerIntervalSec);
  p.end();
}

void configToJson(JsonObject o) {
  JsonArray w = o["wifi"].to<JsonArray>();
  for (int i = 0; i < 3; i++) {
    JsonObject n = w.add<JsonObject>();
    n["ssid"] = cfg.ssid[i];
    n["pass_set"] = cfg.pass[i].length() > 0;  // password never returned
  }
  o["rpc"] = cfg.rpc;
  o["birdeye_set"] = cfg.birdeye.length() > 0;  // key never returned
  o["peep_set"] = cfg.peepKey.length() > 0;
  o["buy_sol"] = cfg.buySol;
  o["max_sol"] = cfg.maxSol;
  o["slippage_bps"] = cfg.slipBps;
  o["prio_microlamports"] = cfg.prioMicro;
  o["cu_limit"] = cfg.cuLimit;
  o["tp_pct"] = cfg.tpPct;
  o["sl_pct"] = cfg.slPct;
  o["min_liq_usd"] = cfg.minLiqUsd;
  o["min_mcap_usd"] = cfg.minMcapUsd;
  o["sort"] = cfg.sort;
  o["armed"] = cfg.armed;
  o["brightness"] = cfg.brightness;
  o["idle_min"] = cfg.idleMin;
  o["miner_buy_sol"] = cfg.minerBuySol;
  o["miner_hold_x"] = cfg.minerHoldX;
  o["miner_sl_pct"] = cfg.minerSlPct;
  o["miner_timeout_min"] = cfg.minerTimeoutMin;
  o["miner_interval_sec"] = cfg.minerIntervalSec;
}

bool configFromJson(JsonObjectConst o) {
  bool ch = false;
  if (o["wifi"].is<JsonArrayConst>()) {
    int i = 0;
    for (JsonObjectConst n : o["wifi"].as<JsonArrayConst>()) {
      if (i >= 3) break;
      if (n["ssid"].is<const char*>()) cfg.ssid[i] = n["ssid"].as<String>();
      // empty/absent password leaves stored one untouched unless ssid changed
      if (n["pass"].is<const char*>()) cfg.pass[i] = n["pass"].as<String>();
      i++;
      ch = true;
    }
  }
#define SET_S(key, field) if (o[key].is<const char*>()) { cfg.field = o[key].as<String>(); ch = true; }
#define SET_F(key, field) if (o[key].is<float>() || o[key].is<int>()) { cfg.field = o[key].as<float>(); ch = true; }
#define SET_I(key, field) if (o[key].is<int>()) { cfg.field = o[key].as<int>(); ch = true; }
#define SET_U(key, field) if (o[key].is<int>()) { cfg.field = o[key].as<uint32_t>(); ch = true; }
#define SET_B(key, field) if (o[key].is<bool>()) { cfg.field = o[key].as<bool>(); ch = true; }
  SET_S("rpc", rpc);
  SET_S("birdeye", birdeye);
  SET_S("peep_key", peepKey);
  SET_F("buy_sol", buySol);
  SET_F("max_sol", maxSol);
  SET_I("slippage_bps", slipBps);
  SET_U("prio_microlamports", prioMicro);
  SET_U("cu_limit", cuLimit);
  SET_I("tp_pct", tpPct);
  SET_I("sl_pct", slPct);
  SET_F("min_liq_usd", minLiqUsd);
  SET_F("min_mcap_usd", minMcapUsd);
  SET_I("sort", sort);
  SET_B("armed", armed);
  SET_I("brightness", brightness);
  SET_I("idle_min", idleMin);
  SET_F("miner_buy_sol", minerBuySol);
  SET_I("miner_hold_x", minerHoldX);
  SET_I("miner_sl_pct", minerSlPct);
  SET_I("miner_timeout_min", minerTimeoutMin);
  SET_I("miner_interval_sec", minerIntervalSec);
  // clamp
  cfg.minerBuySol = constrain(cfg.minerBuySol, 0.0005f, 100.0f);
  cfg.minerHoldX = constrain(cfg.minerHoldX, 2, 4);
  cfg.minerSlPct = constrain(cfg.minerSlPct, 5, 90);
  cfg.minerTimeoutMin = constrain(cfg.minerTimeoutMin, 1, 60);
  cfg.minerIntervalSec = constrain(cfg.minerIntervalSec, 10, 60);
  cfg.slipBps = constrain(cfg.slipBps, 10, 5000);
  cfg.brightness = constrain(cfg.brightness, 5, 100);
  cfg.buySol = constrain(cfg.buySol, 0.0005f, 100.0f);
  if (cfg.buySol > cfg.maxSol) cfg.buySol = cfg.maxSol;
  if (cfg.minerBuySol > cfg.maxSol) cfg.minerBuySol = cfg.maxSol;
  return ch;
}
