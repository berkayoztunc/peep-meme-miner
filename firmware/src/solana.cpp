#include "solana.h"
#include "addrs.h"
#include "config.h"
#include "net.h"
#include "wallet.h"

namespace sol {

// ---------------- RPC ----------------
static bool rpc(const String& method, const String& params, JsonDocument& out, const JsonDocument* filter, String& err) {
  String body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"" + method + "\",\"params\":" + params + "}";
  int code = net::json(cfg.rpc, out, filter, "POST", &body);
  if (code != 200) { err = code < 0 ? net::lastError : "rpc http " + String(code); return false; }
  if (!out["error"].isNull()) {
    err = out["error"]["message"] | "rpc error";
    return false;
  }
  return true;
}

bool getAccount(const Pk& a, size_t off, size_t len, Bytes& data, bool& found, Pk* owner) {
  JsonDocument f;
  f["result"]["value"]["data"] = true;
  f["result"]["value"]["owner"] = true;
  f["error"]["message"] = true;
  JsonDocument r;
  String err;
  String p = "[\"" + String(a.str().c_str()) + "\",{\"encoding\":\"base64\",\"commitment\":\"confirmed\",\"dataSlice\":{\"offset\":" +
             String((unsigned)off) + ",\"length\":" + String((unsigned)len) + "}}]";
  if (!rpc("getAccountInfo", p, r, &f, err)) { net::lastError = err; return false; }
  JsonVariantConst v = r["result"]["value"];
  if (v.isNull()) { found = false; data.clear(); return true; }
  found = true;
  const char* b64 = v["data"][0] | "";
  core::b64dec(b64, data);
  if (owner) core::Pk::parse(v["owner"] | "", *owner);
  return true;
}

bool getBalance(const Pk& a, uint64_t& lamports) {
  JsonDocument f;
  f["result"]["value"] = true;
  f["error"]["message"] = true;
  JsonDocument r;
  String err;
  if (!rpc("getBalance", "[\"" + String(a.str().c_str()) + "\",{\"commitment\":\"confirmed\"}]", r, &f, err)) {
    net::lastError = err;
    return false;
  }
  lamports = r["result"]["value"].as<uint64_t>();
  return true;
}

bool tokenBalance(const Pk& ata, uint64_t& amount, int* decimals) {
  JsonDocument f;
  f["result"]["value"]["amount"] = true;
  f["result"]["value"]["decimals"] = true;
  f["error"]["message"] = true;
  JsonDocument r;
  String err;
  amount = 0;
  if (!rpc("getTokenAccountBalance", "[\"" + String(ata.str().c_str()) + "\",{\"commitment\":\"confirmed\"}]", r, &f, err)) {
    // "could not find account" => zero balance, not a failure
    if (err.indexOf("could not find") >= 0 || err.indexOf("Invalid param") >= 0) return true;
    net::lastError = err;
    return false;
  }
  amount = strtoull(r["result"]["value"]["amount"] | "0", nullptr, 10);
  if (decimals) *decimals = r["result"]["value"]["decimals"] | 6;
  return true;
}

bool sendTx(const Bytes& wire, String& sig, String& err) {
  JsonDocument f;
  f["result"] = true;
  f["error"]["message"] = true;
  f["error"]["data"]["logs"] = true;
  JsonDocument r;
  String p = "[\"" + String(core::b64enc(wire.data(), wire.size()).c_str()) +
             "\",{\"encoding\":\"base64\",\"skipPreflight\":false,\"preflightCommitment\":\"processed\",\"maxRetries\":3}]";
  if (!rpc("sendTransaction", p, r, &f, err)) return false;
  sig = r["result"] | "";
  return sig.length() > 0;
}

bool simulateTx(const Bytes& wire, String& err, String& logs) {
  JsonDocument f;
  f["result"]["value"]["err"] = true;
  f["result"]["value"]["logs"] = true;
  f["error"]["message"] = true;
  JsonDocument r;
  String p = "[\"" + String(core::b64enc(wire.data(), wire.size()).c_str()) +
             "\",{\"encoding\":\"base64\",\"sigVerify\":false,\"replaceRecentBlockhash\":true,\"commitment\":\"processed\"}]";
  if (!rpc("simulateTransaction", p, r, &f, err)) return false;
  JsonVariantConst v = r["result"]["value"];
  JsonArrayConst l = v["logs"].as<JsonArrayConst>();
  if (!l.isNull() && l.size() > 0) logs = l[l.size() - 1].as<String>();
  if (!v["err"].isNull()) {
    String s;
    serializeJson(v["err"], s);
    err = "sim: " + s;
    return false;
  }
  return true;
}

bool confirm(const String& sig, uint32_t timeoutMs, String& err) {
  uint32_t t0 = millis();
  JsonDocument f;
  f["result"]["value"][0]["confirmationStatus"] = true;
  f["result"]["value"][0]["err"] = true;
  f["error"]["message"] = true;
  while (millis() - t0 < timeoutMs) {
    delay(1500);
    JsonDocument r;
    String e;
    if (!rpc("getSignatureStatuses", "[[\"" + sig + "\"],{\"searchTransactionHistory\":false}]", r, &f, e)) continue;
    JsonVariantConst v = r["result"]["value"][0];
    if (v.isNull()) continue;
    if (!v["err"].isNull()) {
      String s;
      serializeJson(v["err"], s);
      err = "tx failed " + s;
      return false;
    }
    const char* st = v["confirmationStatus"] | "";
    if (!strcmp(st, "confirmed") || !strcmp(st, "finalized")) return true;
  }
  err = "confirm timeout";
  return false;
}

// ---------------- sign ----------------
bool signWire(const Bytes& message, Bytes& wire) {
  uint8_t sig[64];
  if (!wallet::sign(message.data(), message.size(), sig)) return false;
  wire.clear();
  wire.push_back(1);
  wire.insert(wire.end(), sig, sig + 64);
  wire.insert(wire.end(), message.begin(), message.end());
  return true;
}
}  // namespace sol
