// Host test: compiled legacy message must be byte-identical to Orquestra's own serializedTransaction.
// g++ -std=c++17 -I../src test_tx.cpp -o /tmp/test_tx && /tmp/test_tx
#include <cstdio>
#include "txbuild.h"
using namespace core;

static const char* SERIALIZED = "3d7jKWNUVa5bkt1gz7iWxCnfJYnqfYTRsvvcmjDyczvGgwuWzuwwA74T5VDrbH7EJs2dewgbhPys2E2W1EeTyMvJCBSGhkEEddW9A6v9rrd92XtccGiPQtrHgifPcHh9G5Pyt1LawmaGE6BwbWVU1Yej6wvTdvRW5sK8Cw4wRGnMW3q5bMoDtrb3Z3J2kF5bAQoofrkR74PH3f6kJBSMamX2JuucdJ9GBZSW8pk5ubLuWECFSmmjFTLLNxLycPBhJWg46UTKPbzEsoHXPrM5E4K6aSxaP6riRVLuxoJRaamdNvEcjLibiBn21ALQngu5tS4Hi9q9f6nNEd9oT5p6ATcUbHrzBBGpGaNZaPGs2bbGSbKYhkRdut8wVVAzuLnKQWDamNS1JmqdrKKkB3AT7iJgYAfu3soobEhRWGZCnXodtgf5uC8t3QnBUauXBoVfRJRJGoFA3urVZvWnSQaDDrP82HfPETzLU1MtYW1gVS13un7xaJM7YrsQSCpUtbz1JDDWpAu92EjMeAQ7bJ7gXhqKPwkXXh1dkQEYP33BofVP5xHA8sLbHdaH8pJqNU7Uuiopa4YGfViywWypQ6UZKYGBC2SqcEr4WxsJMMDaXEfVmQSCmyhKXhRrqARxtoPdGPDHkghxj24ZptGrqN7EwcNTvGYbc5Yh74dzFd2c4yzi1xTkZo78spjhMi83QndBWnwj43RjmGvveZS7s3De2CkF5NYHwv2edaCxAZHm2puLPjT2PpqtcGCp7ecVkDUSMRKkhgyfnfQKUoQUj3qtEcFKBobyRo47NvBJzz1SykX3JR9bw9J5qEeN4ycjbdLi53aPN3AqB2sxMyaMo7cBpNZhQBM5ZjcWX37M6z6u1JD21s8rdT9jjNqdwsNgPsRWcX";
static const char* BLOCKHASH = "7feDDmuMo3v7yrRG7e5aCHtRQxLj3zz7iMvmTUPt8x5U";
static const char* DATA = "38fc74089edfcd5f8096980000000000010000000000000000";

int main() {
  int fails = 0;
  Pk user = Pk::of("9WzDXwBbmkg8ZTbNMqUxvQRAyrZzDsGYdLVL9zYtAWWM");
  Pk mint = Pk::of("2qAU9FxwNTiykfxwcURPoevmsMfDTwX3Fr2Kto3qhmLV");
  Pk bc = addrs::pumpBondingCurve(mint);
  Pk tp = addrs::tokenProgram();

  // extra PDA vector: associated_bonding_curve
  std::string abc = addrs::ata(bc, tp, mint).str();
  bool abcOk = abc == "6pP3uPoXb3uYnw3qbiwJNKDAYVqH8rRz8zY4K1iSneGv";
  printf("%s associated_bonding_curve %s\n", abcOk ? "ok  " : "FAIL", abc.c_str());
  fails += !abcOk;

  sol::Ix ix;
  ix.program = addrs::pumpProgram();
  hexdec(DATA, ix.data);
  auto m = [&](const Pk& k, bool s, bool w) { ix.accts.push_back({k, s, w}); };
  m(addrs::pumpGlobal(), false, false);
  m(Pk::of("62qc2CNXwrYqQScmEdiZFFAnJR262PxWEuNQtxfafNgV"), false, true);
  m(mint, false, false);
  m(bc, false, true);
  m(Pk::of(abc.c_str()), false, true);
  m(addrs::ata(user, tp, mint), false, true);
  m(user, true, true);
  m(addrs::systemProgram(), false, false);
  m(tp, false, false);
  m(addrs::pumpCreatorVault(Pk::of("DLQuKV6NUSvxnbwZLuFTCdmSYAg5T9TXcxFE5fxd9asf")), false, true);
  m(addrs::pumpEventAuthority(), false, false);
  m(addrs::pumpProgram(), false, false);
  m(addrs::pumpGlobalVolAcc(), false, false);
  m(addrs::pumpUserVolAcc(user), false, true);
  m(addrs::pumpFeeConfig(), false, false);
  m(addrs::feeProgram(), false, false);

  Bytes msg;
  bool ok = sol::compile(user, {ix}, Pk::of(BLOCKHASH), msg);
  Bytes want;
  b58dec(SERIALIZED, want);
  // wire = shortvec(1 sig)=1 byte + 64 byte sig + message
  Bytes wantMsg(want.begin() + 65, want.end());
  bool same = ok && msg == wantMsg;
  printf("%s message matches Orquestra (%zu vs %zu bytes)\n", same ? "ok  " : "FAIL", msg.size(), wantMsg.size());
  if (!same) {
    size_t n = std::min(msg.size(), wantMsg.size());
    for (size_t i = 0; i < n; i++) if (msg[i] != wantMsg[i]) { printf("  first diff at byte %zu: got %02x want %02x\n", i, msg[i], wantMsg[i]); break; }
    fails++;
  }
  // compute-budget ix encodings
  auto lim = sol::cuLimit(250000);
  auto prc = sol::cuPrice(100000);
  char h[64];
  std::string hl, hp;
  for (auto b : lim.data) { snprintf(h, sizeof h, "%02x", b); hl += h; }
  for (auto b : prc.data) { snprintf(h, sizeof h, "%02x", b); hp += h; }
  bool cb = hl == "0290d00300" && hp == "03a086010000000000";
  printf("%s compute budget encodings %s %s\n", cb ? "ok  " : "FAIL", hl.c_str(), hp.c_str());
  fails += !cb;

  // full tx with priority + ata create must compile with a single signer
  std::vector<sol::Ix> all = {sol::cuLimit(250000), sol::cuPrice(100000),
                              sol::createAtaIdempotent(user, addrs::ata(user, tp, mint), user, mint, tp), ix};
  Bytes full;
  bool fok = sol::compile(user, all, Pk::of(BLOCKHASH), full);
  printf("%s full composed tx compiles (%zu bytes msg)\n", fok ? "ok  " : "FAIL", full.size());
  fails += !fok;
  printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
  return fails ? 1 : 0;
}
