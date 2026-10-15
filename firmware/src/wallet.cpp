#include "wallet.h"
#include <Preferences.h>
#include <Ed25519.h>
#include <esp_system.h>
#include <bootloader_random.h>

namespace wallet {
static uint8_t seed[32];
static core::Pk pk;
static bool loaded = false;

static void derive() { Ed25519::derivePublicKey(pk.b, seed); }

static void persist() {
  Preferences p;
  p.begin("wallet", false);
  p.putBytes("seed", seed, 32);
  p.end();
}

bool load() {
  Preferences p;
  p.begin("wallet", true);
  size_t n = p.getBytes("seed", seed, 32);
  p.end();
  loaded = (n == 32);
  if (loaded) derive();
  return loaded;
}

bool has() { return loaded; }
const core::Pk& pub() { return pk; }
String pubStr() { return loaded ? String(pk.str().c_str()) : String(""); }

bool generate() {
  bootloader_random_enable();
  esp_fill_random(seed, 32);
  bootloader_random_disable();
  derive();
  persist();
  loaded = true;
  return true;
}

bool importB58(const String& secret) {
  core::Bytes b;
  if (!core::b58dec(std::string(secret.c_str()), b)) return false;
  if (b.size() != 32 && b.size() != 64) return false;
  memcpy(seed, b.data(), 32);
  derive();
  if (b.size() == 64 && memcmp(pk.b, b.data() + 32, 32) != 0) {  // keypair halves disagree
    memset(seed, 0, 32);
    loaded = false;
    return false;
  }
  persist();
  loaded = true;
  return true;
}

bool sign(const uint8_t* msg, size_t n, uint8_t sig[64]) {
  if (!loaded) return false;
  Ed25519::sign(sig, seed, pk.b, msg, n);
  return true;
}

void wipe() {
  Preferences p;
  p.begin("wallet", false);
  p.clear();
  p.end();
  memset(seed, 0, 32);
  loaded = false;
}
}  // namespace wallet
