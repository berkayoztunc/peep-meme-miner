#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct Config {
  String ssid[3], pass[3];
  String rpc = "https://api.mainnet-beta.solana.com";
  String birdeye;
  String peepKey;           // Peep signals API key (Bearer)
  float buySol = 0.01f;     // SOL per buy
  float maxSol = 0.05f;     // hard cap per trade
  int slipBps = 1500;       // slippage 15%
  uint32_t prioMicro = 100000;   // compute unit price, micro-lamports
  uint32_t cuLimit = 250000;     // compute unit limit
  int tpPct = 0;            // take profit %, 0 = off
  int slPct = 0;            // stop loss %, 0 = off
  float minLiqUsd = 2000;
  float minMcapUsd = 0;
  int sort = 1;             // signal type: 0 radar, 1 thesis, 2 call, 3 any
  bool armed = false;       // false = simulate only
  int brightness = 60;      // 0-100
  int idleMin = 10;         // deep sleep when idle in list
  // miner mode
  float minerBuySol = 0.05f;  // SOL per auto buy
  int minerHoldX = 2;         // sell at Nx entry (2,3,4)
  int minerSlPct = 50;        // stop loss % (20,30,50)
  int minerTrailPct = 20;     // ATH down sell: sell when price falls this % from the peak since buy (0 = off, 10..50)
  int minerTimeoutMin = 10;   // sell after N minutes
  int minerIntervalSec = 30;  // signal check interval (10,30,60)
};

extern Config cfg;

void configLoad();
void configSave();
void configToJson(JsonObject o);          // never includes secrets other than api keys masked
bool configFromJson(JsonObjectConst o);   // returns true if anything changed
