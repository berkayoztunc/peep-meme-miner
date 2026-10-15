#include "orquestra.h"
#include "net.h"

namespace orq {
bool build(const char* project, const char* ixName, JsonDocument& req,
           sol::Ix& out, core::Pk& blockhash, String& err) {
  JsonDocument f;
  f["instruction"]["programId"] = true;
  f["instruction"]["data"] = true;
  f["instruction"]["accounts"][0]["pubkey"] = true;
  f["instruction"]["accounts"][0]["isSigner"] = true;
  f["instruction"]["accounts"][0]["isWritable"] = true;
  f["recentBlockhash"] = true;
  f["error"] = true;
  f["details"] = true;
  JsonDocument r;
  String body;
  serializeJson(req, body);
  String url = String("https://api.orquestra.dev/api/") + project + "/instructions/" + ixName + "/build";
  int code = net::json(url, r, &f, "POST", &body);
  if (code != 200) {
    err = "orquestra " + String(code);
    if (!r["error"].isNull()) {
      err += ": " + r["error"].as<String>();
      if (!r["details"].isNull()) { String d; serializeJson(r["details"], d); err += " " + d; }
    } else if (code < 0) err += " " + net::lastError;
    return false;
  }
  JsonObjectConst ix = r["instruction"];
  if (ix.isNull()) { err = "orquestra: no instruction"; return false; }
  if (!core::Pk::parse(ix["programId"] | "", out.program)) { err = "bad programId"; return false; }
  core::Bytes d;
  if (!core::hexdec(ix["data"] | "", d)) { err = "bad data"; return false; }
  out.data = d;
  out.accts.clear();
  for (JsonObjectConst a : ix["accounts"].as<JsonArrayConst>()) {
    sol::Meta m;
    if (!core::Pk::parse(a["pubkey"] | "", m.key)) { err = "bad account"; return false; }
    m.signer = a["isSigner"] | false;
    m.writable = a["isWritable"] | false;
    out.accts.push_back(m);
  }
  if (!core::Pk::parse(r["recentBlockhash"] | "", blockhash)) { err = "no blockhash"; return false; }
  return true;
}
}  // namespace orq
