#include "pump.h"
#include "addrs.h"
#include "config.h"
#include "net.h"
#include "orquestra.h"
#include "solana.h"
#include "wallet.h"
#include <Preferences.h>

using core::Bytes;
using core::Pk;
using sol::Ix;

Position pos;
ProgressFn onProgress = nullptr;

static void prog(const char* m) { if (onProgress) onProgress(m); }

// A failed buy/sell is retried this many times (after the first attempt), each with a fresh
// build + blockhash. Before every retry the token balance is re-read: if the earlier send
// actually landed, we continue as success instead of trading twice.
static const int TX_RETRIES = 3;
static void progRetry(const char* what, int n) {
  char b[24];
  snprintf(b, sizeof b, "%s retry %d/%d", what, n, TX_RETRIES);
  prog(b);
}

// ---------------- position persistence ----------------
void positionLoad() {
  Preferences p;
  p.begin("pos", true);
  pos.open = p.getBool("open", false);
  pos.mint = p.getString("mint", "");
  pos.sym = p.getString("sym", "");
  pos.pair = p.getString("pair", "");
  pos.dex = p.getString("dex", "");
  pos.spentLamports = p.getULong64("spent", 0);
  pos.tokens = p.getULong64("tokens", 0);
  pos.decimals = p.getInt("dec", 6);
  pos.openedAt = p.getUInt("at", 0);
  pos.peakPrice = p.getDouble("peak", 0);
  p.end();
  if (pos.mint.length() < 32) pos.open = false;
}

void positionSave() {
  Preferences p;
  p.begin("pos", false);
  p.putBool("open", pos.open);
  p.putString("mint", pos.mint);
  p.putString("sym", pos.sym);
  p.putString("pair", pos.pair);
  p.putString("dex", pos.dex);
  p.putULong64("spent", pos.spentLamports);
  p.putULong64("tokens", pos.tokens);
  p.putInt("dec", pos.decimals);
  p.putUInt("at", pos.openedAt);
  p.putDouble("peak", pos.peakPrice);
  p.end();
}

void positionClear() {
  pos = Position();
  positionSave();
}

// Value you would actually receive on sell: spot value minus the ~1.25% trading fee.
double positionValueSol(double priceNative) {
  double ui = (double)pos.tokens / pow(10.0, pos.decimals);
  return ui * priceNative * (1.0 - 0.0125);
}
double positionPnlPct(double priceNative) {
  if (pos.spentLamports == 0) return 0;
  double spent = (double)pos.spentLamports / 1e9;
  return (positionValueSol(priceNative) / spent - 1.0) * 100.0;
}

// ---------------- helpers ----------------
static uint64_t rd64(const Bytes& b, size_t off) {
  uint64_t v = 0;
  for (int i = 7; i >= 0; i--) v = (v << 8) | b[off + i];
  return v;
}
static void wr64(Bytes& b, uint64_t v) { for (int i = 0; i < 8; i++) b.push_back((v >> (8 * i)) & 0xFF); }
static uint64_t ceilDiv(uint64_t a, uint64_t b) { return (a + b - 1) / b; }

struct Acc {  // account map for an Orquestra build request + expected set for validation
  JsonDocument* req;
  std::vector<Pk> expect;
  void add(const char* name, const Pk& k) {
    (*req)["accounts"][name] = String(k.str().c_str());
    expect.push_back(k);
  }
};

// Reject anything that is not exactly what we asked for. Orquestra (or a MITM) can't
// swap accounts, add signers, or alter amounts without being caught here.
static bool validateIx(const Ix& ix, const Pk& program, const Acc& a, const Pk& user,
                       const Bytes& expectArgs, String& err) {
  if (!(ix.program == program)) { err = "ix: wrong program"; return false; }
  if (ix.accts.size() != a.expect.size()) { err = "ix: account count"; return false; }
  for (auto& m : ix.accts) {
    bool ok = false;
    for (auto& e : a.expect) if (e == m.key) { ok = true; break; }
    if (!ok) { err = "ix: unexpected account"; return false; }
    if (m.signer && !(m.key == user)) { err = "ix: extra signer"; return false; }
  }
  if (ix.data.size() < 8 + expectArgs.size()) { err = "ix: data size"; return false; }
  if (memcmp(ix.data.data() + 8, expectArgs.data(), expectArgs.size()) != 0) { err = "ix: args mismatch"; return false; }
  return true;
}

// compile + sign + simulate/send/confirm
static bool land(const Pk& user, std::vector<Ix>& ixs, const Pk& blockhash, bool armed, String& sig, String& err) {
  Bytes msg, wire;
  if (!sol::compile(user, ixs, blockhash, msg)) { err = "compile"; return false; }
  if (!sol::signWire(msg, wire)) { err = "sign"; return false; }
  if (wire.size() > 1232) { err = "tx too large"; return false; }
  if (!armed) {
    prog("simulating");
    String logs;
    if (!sol::simulateTx(wire, err, logs)) return false;
    sig = "SIMULATED";
    return true;
  }
  prog("sending");
  if (!sol::sendTx(wire, sig, err)) return false;
  // No RPC confirm wait: buy/sell verify by polling the token balance instead (faster, same safety).
  return true;
}

static void addPriority(std::vector<Ix>& ixs) {
  ixs.push_back(sol::cuLimit(cfg.cuLimit));
  ixs.push_back(sol::cuPrice(cfg.prioMicro));
}

// ---------------- common state read ----------------
struct Curve {
  uint64_t vTok, vSol;
  bool complete;
  Pk creator;
};
struct PumpGlobal {
  Pk feeRecipient, buybackRecipient;
  uint64_t feeBps, creatorFeeBps;
};

static bool readCurve(const Pk& bc, Curve& c, String& err) {
  Bytes d; bool found;
  if (!sol::getAccount(bc, 0, 81, d, found)) { err = net::lastError; return false; }
  if (!found || d.size() < 81) { err = "no bonding curve"; return false; }
  c.vTok = rd64(d, 8);
  c.vSol = rd64(d, 16);
  c.complete = d[48] != 0;
  memcpy(c.creator.b, d.data() + 49, 32);
  return true;
}

// Global layout (1088 bytes): fee_recipient @41, fee_bps @105, creator_fee_bps @154,
// buyback_fee_recipient @773 (verified against real mainnet txs).
static bool readGlobal(PumpGlobal& g, String& err) {
  Bytes d; bool found;
  if (!sol::getAccount(addrs::pumpGlobal(), 0, 810, d, found) || !found || d.size() < 805) {
    err = "pump global: " + net::lastError;
    return false;
  }
  memcpy(g.feeRecipient.b, d.data() + 41, 32);
  memcpy(g.buybackRecipient.b, d.data() + 773, 32);
  g.feeBps = rd64(d, 105);
  g.creatorFeeBps = rd64(d, 154);
  if (g.feeBps > 500) g.feeBps = 95;                // sanity: fall back to known defaults,
  if (g.creatorFeeBps > 500) g.creatorFeeBps = 5;   // slippage margin covers the difference
  return true;
}

static bool tokenProgramOf(const Pk& mint, Pk& tp, String& err) {
  Bytes d; bool found;
  if (!sol::getAccount(mint, 0, 0, d, found, &tp) || !found) { err = "mint not found"; return false; }
  return true;
}

static uint64_t applySlip(uint64_t v) { return v * (10000 - cfg.slipBps) / 10000; }

// ---------------- bonding curve (V2 instructions, SOL quote = WSOL) ----------------
// Accounts shared by buy_exact_quote_in_v2 and sell_v2.
static void curveAccounts(Acc& a, const Pk& mint, const Pk& baseTp, const Pk& bc, const Curve& c,
                          const PumpGlobal& g, const Pk& user) {
  Pk qtp = addrs::tokenProgram(), wsol = addrs::wsolMint();
  Pk cv = addrs::pumpCreatorVault(c.creator);
  Pk uva = addrs::pumpUserVolAcc(user);
  a.add("global", addrs::pumpGlobal());
  a.add("base_mint", mint);
  a.add("quote_mint", wsol);
  a.add("base_token_program", baseTp);
  a.add("quote_token_program", qtp);
  a.add("associated_token_program", addrs::ataProgram());
  a.add("fee_recipient", g.feeRecipient);
  a.add("associated_quote_fee_recipient", addrs::ata(g.feeRecipient, qtp, wsol));
  a.add("buyback_fee_recipient", g.buybackRecipient);
  a.add("associated_quote_buyback_fee_recipient", addrs::ata(g.buybackRecipient, qtp, wsol));
  a.add("bonding_curve", bc);
  a.add("associated_base_bonding_curve", addrs::ata(bc, baseTp, mint));
  a.add("associated_quote_bonding_curve", addrs::ata(bc, qtp, wsol));
  a.add("user", user);
  a.add("associated_base_user", addrs::ata(user, baseTp, mint));
  a.add("associated_quote_user", addrs::ata(user, qtp, wsol));
  a.add("creator_vault", cv);
  a.add("associated_creator_vault", addrs::ata(cv, qtp, wsol));
  a.add("sharing_config", addrs::pumpSharingConfig(mint));
  a.add("user_volume_accumulator", uva);
  a.add("associated_user_volume_accumulator", addrs::ata(uva, qtp, wsol));
  a.add("fee_config", addrs::pumpFeeConfig());
  a.add("fee_program", addrs::feeProgram());
  a.add("system_program", addrs::systemProgram());
  a.add("event_authority", addrs::pumpEventAuthority());
  a.add("program", addrs::pumpProgram());
}

static bool buyCurve(const Token& t, bool armed, uint64_t spend, String& err, String& sig, uint64_t& tokensOut) {
  Pk user = wallet::pub(), mint;
  if (!Pk::parse(t.mint.c_str(), mint)) { err = "bad mint"; return false; }
  prog("reading curve");
  Pk tp;
  if (!tokenProgramOf(mint, tp, err)) return false;
  Pk bc = addrs::pumpBondingCurve(mint);
  Curve c;
  if (!readCurve(bc, c, err)) return false;
  if (c.complete) { err = "migrated, refresh list"; return false; }
  PumpGlobal g;
  if (!readGlobal(g, err)) return false;

  // quote per Pump docs (buy_exact_sol_in formulas also apply to the v2 quote-in variant)
  uint64_t totalBps = g.feeBps + g.creatorFeeBps;
  uint64_t net = spend * 10000 / (10000 + totalBps);
  uint64_t fees = ceilDiv(net * g.feeBps, 10000) + ceilDiv(net * g.creatorFeeBps, 10000);
  if (net + fees > spend) net -= (net + fees - spend);
  uint64_t out = (net - 1) * c.vTok / (c.vSol + net - 1);
  uint64_t minOut = applySlip(out);
  if (minOut == 0) minOut = 1;

  JsonDocument req;
  Acc a{&req, {}};
  curveAccounts(a, mint, tp, bc, c, g, user);
  a.add("global_volume_accumulator", addrs::pumpGlobalVolAcc());
  req["args"]["spendable_quote_in"] = String((unsigned long long)spend);
  req["args"]["min_tokens_out"] = String((unsigned long long)minOut);
  req["feePayer"] = String(user.str().c_str());

  prog("orquestra build");
  Ix ix; Pk bh;
  if (!orq::build(orq::PUMP, "buy_exact_quote_in_v2", req, ix, bh, err)) return false;
  Bytes args; wr64(args, spend); wr64(args, minOut);
  if (!validateIx(ix, addrs::pumpProgram(), a, user, args, err)) return false;

  std::vector<Ix> ixs;
  addPriority(ixs);
  ixs.push_back(sol::createAtaIdempotent(user, addrs::ata(user, tp, mint), user, mint, tp));
  ixs.push_back(ix);
  if (!land(user, ixs, bh, armed, sig, err)) return false;
  tokensOut = out;
  return true;
}

static bool sellCurve(const Pk& mint, uint64_t amount, bool armed, String& err, String& sig) {
  Pk user = wallet::pub(), tp;
  prog("reading curve");
  if (!tokenProgramOf(mint, tp, err)) return false;
  Pk bc = addrs::pumpBondingCurve(mint);
  Curve c;
  if (!readCurve(bc, c, err)) return false;
  if (c.complete) { err = "migrated, refresh list"; return false; }
  PumpGlobal g;
  if (!readGlobal(g, err)) return false;
  uint64_t gross = amount * c.vSol / (c.vTok + amount);
  uint64_t fees = ceilDiv(gross * g.feeBps, 10000) + ceilDiv(gross * g.creatorFeeBps, 10000);
  uint64_t out = gross > fees ? gross - fees : 0;
  uint64_t minOut = applySlip(out);

  JsonDocument req;
  Acc a{&req, {}};
  curveAccounts(a, mint, tp, bc, c, g, user);
  req["args"]["amount"] = String((unsigned long long)amount);
  req["args"]["min_sol_output"] = String((unsigned long long)minOut);
  req["feePayer"] = String(user.str().c_str());

  prog("orquestra build");
  Ix ix; Pk bh;
  if (!orq::build(orq::PUMP, "sell_v2", req, ix, bh, err)) return false;
  Bytes args; wr64(args, amount); wr64(args, minOut);
  if (!validateIx(ix, addrs::pumpProgram(), a, user, args, err)) return false;
  std::vector<Ix> ixs;
  addPriority(ixs);
  ixs.push_back(ix);
  return land(user, ixs, bh, armed, sig, err);
}

// ---------------- PumpSwap AMM ----------------
struct Pool {
  Pk baseMint, quoteMint, baseVault, quoteVault, coinCreator;
  Pk feeRecipient, buybackRecipient;
  uint64_t totalFeeBps;
  uint64_t baseRes, quoteRes;
};

static bool readPool(const Pk& pool, const Pk& mint, Pool& p, String& err) {
  Bytes d; bool found;
  uint64_t virtualQuote = 0;
  if (!sol::getAccount(pool, 0, 301, d, found) || !found || d.size() < 253) { err = "pool not found"; return false; }
  memcpy(p.baseMint.b, d.data() + 43, 32);
  memcpy(p.quoteMint.b, d.data() + 75, 32);
  memcpy(p.baseVault.b, d.data() + 139, 32);
  memcpy(p.quoteVault.b, d.data() + 171, 32);
  memcpy(p.coinCreator.b, d.data() + 211, 32);
  if (!(p.baseMint == mint) || !(p.quoteMint == addrs::wsolMint())) { err = "pool not SOL-quoted"; return false; }
  Bytes g;
  if (!sol::getAccount(addrs::ammGlobalConfig(), 0, 840, g, found) || !found || g.size() < 835) {
    err = "amm config"; return false;
  }
  uint64_t lp = rd64(g, 40), pf = rd64(g, 48), cf = rd64(g, 313);
  memcpy(p.buybackRecipient.b, g.data() + 803, 32);  // entry of the buyback recipient list
  if (lp > 500) lp = 20;
  if (pf > 500) pf = 5;
  if (cf > 500) cf = 5;
  p.totalFeeBps = lp + pf + cf;
  if (p.totalFeeBps < 125) p.totalFeeBps = 125;   // measured: real PumpSwap trades cost ~1.25% (dynamic fee tiers)
  virtualQuote = rd64(d, 245);                      // pool virtual quote reserve (lamports), see chainPrice()
  bool any = false;
  for (int i = 0; i < 8; i++) {
    const uint8_t* r = g.data() + 57 + 32 * i;
    bool nz = false;
    for (int k = 0; k < 32; k++) if (r[k]) { nz = true; break; }
    if (nz) { memcpy(p.feeRecipient.b, r, 32); any = true; break; }
  }
  if (!any) { err = "no fee recipient"; return false; }
  if (!sol::tokenBalance(p.baseVault, p.baseRes) || !sol::tokenBalance(p.quoteVault, p.quoteRes)) {
    err = "pool reserves";
    return false;
  }
  if (p.baseRes == 0 || p.quoteRes == 0) { err = "empty pool"; return false; }
  // Swaps price against vault + virtual quote reserve; the vault alone understates price by 30-45%.
  if (virtualQuote < 200ULL * 1000000000ULL) p.quoteRes += virtualQuote;
  return true;
}

static void ammAccounts(Acc& a, const Pk& pool, const Pool& p, const Pk& user, const Pk& mint,
                        const Pk& baseTp, const Pk& userBase, const Pk& userQuote) {
  Pk quoteTp = addrs::tokenProgram();
  Pk cvAuth = addrs::ammCreatorVaultAuthority(p.coinCreator);
  a.add("pool", pool);
  a.add("user", user);
  a.add("global_config", addrs::ammGlobalConfig());
  a.add("base_mint", mint);
  a.add("quote_mint", addrs::wsolMint());
  a.add("user_base_token_account", userBase);
  a.add("user_quote_token_account", userQuote);
  a.add("pool_base_token_account", p.baseVault);
  a.add("pool_quote_token_account", p.quoteVault);
  a.add("protocol_fee_recipient", p.feeRecipient);
  a.add("protocol_fee_recipient_token_account", addrs::ata(p.feeRecipient, quoteTp, addrs::wsolMint()));
  a.add("base_token_program", baseTp);
  a.add("quote_token_program", quoteTp);
  a.add("system_program", addrs::systemProgram());
  a.add("associated_token_program", addrs::ataProgram());
  a.add("event_authority", addrs::ammEventAuthority());
  a.add("program", addrs::ammProgram());
  a.add("coin_creator_vault_ata", addrs::ata(cvAuth, quoteTp, addrs::wsolMint()));
  a.add("coin_creator_vault_authority", cvAuth);
}

// Current PumpSwap builds take 3 accounts beyond Orquestra's IDL: pool_v2 (ro), buyback
// recipient (ro) and its WSOL ATA (w). Verified against real mainnet buy/sell txs.
static void ammExtras(Ix& ix, const Pk& mint, const Pool& p) {
  ix.accts.push_back({addrs::ammPoolV2(mint), false, false});
  ix.accts.push_back({p.buybackRecipient, false, false});
  ix.accts.push_back({addrs::ata(p.buybackRecipient, addrs::tokenProgram(), addrs::wsolMint()), false, true});
}

static bool buyAmm(const Token& t, bool armed, uint64_t spend, String& err, String& sig, uint64_t& tokensOut) {
  Pk user = wallet::pub(), mint, pool, baseTp;
  if (!Pk::parse(t.mint.c_str(), mint) || !Pk::parse(t.pair.c_str(), pool)) { err = "bad ids"; return false; }
  prog("reading pool");
  if (!tokenProgramOf(mint, baseTp, err)) return false;
  Pool p;
  if (!readPool(pool, mint, p, err)) return false;
  uint64_t net = spend * 10000 / (10000 + p.totalFeeBps);
  uint64_t out = p.baseRes * net / (p.quoteRes + net);
  uint64_t minOut = applySlip(out);
  if (minOut == 0) minOut = 1;

  Pk wsolAta = addrs::ata(user, addrs::tokenProgram(), addrs::wsolMint());
  Pk userBase = addrs::ata(user, baseTp, mint);
  JsonDocument req;
  Acc a{&req, {}};
  ammAccounts(a, pool, p, user, mint, baseTp, userBase, wsolAta);
  a.add("global_volume_accumulator", addrs::ammGlobalVolAcc());
  a.add("user_volume_accumulator", addrs::ammUserVolAcc(user));
  a.add("fee_config", addrs::ammFeeConfig());
  a.add("fee_program", addrs::feeProgram());
  req["args"]["spendable_quote_in"] = String((unsigned long long)spend);
  req["args"]["min_base_amount_out"] = String((unsigned long long)minOut);
  req["args"]["track_volume"]["field_0"] = false;
  req["feePayer"] = String(user.str().c_str());

  prog("orquestra build");
  Ix ix; Pk bh;
  if (!orq::build(orq::AMM, "buy_exact_quote_in", req, ix, bh, err)) return false;
  Bytes args; wr64(args, spend); wr64(args, minOut);
  if (!validateIx(ix, addrs::ammProgram(), a, user, args, err)) return false;
  ammExtras(ix, mint, p);
  if (ix.data.size() == 25) ix.data.pop_back();  // on-chain clients omit track_volume

  std::vector<Ix> ixs;
  addPriority(ixs);
  ixs.push_back(sol::createAtaIdempotent(user, wsolAta, user, addrs::wsolMint(), addrs::tokenProgram()));
  ixs.push_back(sol::createAtaIdempotent(user, userBase, user, mint, baseTp));
  ixs.push_back(sol::transferSol(user, wsolAta, spend));
  ixs.push_back(sol::syncNative(wsolAta));
  ixs.push_back(ix);
  ixs.push_back(sol::closeAccount(wsolAta, user, user));  // unwrap leftover
  if (!land(user, ixs, bh, armed, sig, err)) return false;
  tokensOut = out;
  return true;
}

static bool sellAmm(const Pk& mint, const Pk& pool, uint64_t amount, bool armed, String& err, String& sig) {
  Pk user = wallet::pub(), baseTp;
  prog("reading pool");
  if (!tokenProgramOf(mint, baseTp, err)) return false;
  Pool p;
  if (!readPool(pool, mint, p, err)) return false;
  uint64_t gross = p.quoteRes * amount / (p.baseRes + amount);
  uint64_t out = gross - gross * p.totalFeeBps / 10000;
  uint64_t minOut = applySlip(out);

  Pk wsolAta = addrs::ata(user, addrs::tokenProgram(), addrs::wsolMint());
  Pk userBase = addrs::ata(user, baseTp, mint);
  JsonDocument req;
  Acc a{&req, {}};
  ammAccounts(a, pool, p, user, mint, baseTp, userBase, wsolAta);
  a.add("fee_config", addrs::ammFeeConfig());
  a.add("fee_program", addrs::feeProgram());
  req["args"]["base_amount_in"] = String((unsigned long long)amount);
  req["args"]["min_quote_amount_out"] = String((unsigned long long)minOut);
  req["feePayer"] = String(user.str().c_str());

  prog("orquestra build");
  Ix ix; Pk bh;
  if (!orq::build(orq::AMM, "sell", req, ix, bh, err)) return false;
  Bytes args; wr64(args, amount); wr64(args, minOut);
  if (!validateIx(ix, addrs::ammProgram(), a, user, args, err)) return false;
  ammExtras(ix, mint, p);

  std::vector<Ix> ixs;
  addPriority(ixs);
  ixs.push_back(sol::createAtaIdempotent(user, wsolAta, user, addrs::wsolMint(), addrs::tokenProgram()));
  ixs.push_back(ix);
  ixs.push_back(sol::closeAccount(wsolAta, user, user));
  return land(user, ixs, bh, armed, sig, err);
}

// ---------------- public flows ----------------
bool buy(const Token& t, bool armed, String& err, String& sig, float solOverride) {
  if (!wallet::has()) { err = "no wallet"; return false; }
  if (pos.open) { err = "position open"; return false; }
  float sol = min(solOverride > 0 ? solOverride : cfg.buySol, cfg.maxSol);
  uint64_t spend = (uint64_t)(sol * 1e9f);
  Pk user = wallet::pub();
  uint64_t bal = 0;
  if (!sol::getBalance(user, bal)) { err = "balance: " + net::lastError; return false; }
  if (bal < spend + 5000000ULL) { err = "low balance"; return false; }

  // Venue is decided on-chain: live bonding curve -> curve, completed -> canonical PumpSwap pool.
  Token tt = t;
  {
    Pk m;
    if (!Pk::parse(t.mint.c_str(), m)) { err = "bad mint"; return false; }
    Curve c;
    String e;
    bool haveCurve = readCurve(addrs::pumpBondingCurve(m), c, e);
    if (haveCurve && !c.complete) {
      tt.dex = "pumpfun";
      tt.pair = String(addrs::pumpBondingCurve(m).str().c_str());
    } else if (haveCurve || e == "no bonding curve") {   // completed, or curve closed after migration
      tt.dex = "pumpswap";
      tt.pair = String(addrs::ammPool(m).str().c_str());
    } else { err = e; return false; }
  }
  Pk mint, tp;
  Pk::parse(t.mint.c_str(), mint);
  String e2;
  tokenProgramOf(mint, tp, e2);
  Pk ata = addrs::ata(user, tp, mint);
  uint64_t baseTok = 0;
  sol::tokenBalance(ata, baseTok);   // 0 when the ATA does not exist yet

  uint64_t out = 0;
  bool ok = false, landed = false;
  for (int attempt = 0; attempt <= TX_RETRIES; attempt++) {
    if (attempt > 0) {
      progRetry("buy", attempt);
      delay(1500);
      uint64_t now = 0;
      if (armed && sol::tokenBalance(ata, now) && now > baseTok) { landed = true; break; }  // earlier send landed
    }
    ok = (tt.dex == "pumpswap") ? buyAmm(tt, armed, spend, err, sig, out)
                                : buyCurve(tt, armed, spend, err, sig, out);
    if (ok) break;
  }
  if (!ok && !landed) return false;   // err holds the last attempt's error
  if (!armed) return ok;

  // real token amount from chain
  uint64_t have = 0;
  int dec = 6;
  for (int i = 0; i < 5 && have == 0; i++) {
    if (i) delay(1200);
    sol::tokenBalance(ata, have, &dec);
  }
  if (have == 0) have = out;  // fall back to quote
  pos.open = true;
  pos.mint = tt.mint;
  pos.sym = tt.sym;
  pos.pair = tt.pair;
  pos.dex = tt.dex;
  pos.spentLamports = spend;
  pos.tokens = have;
  pos.decimals = dec;
  time_t now = time(nullptr);
  pos.openedAt = now > 1700000000 ? (uint32_t)now : 0;
  positionSave();
  return true;
}

bool sell(bool armed, String& err, String& sig, int64_t& delta) {
  delta = 0;
  if (!pos.open) { err = "no position"; return false; }
  Pk user = wallet::pub(), mint, tp, pool;
  Pk::parse(pos.mint.c_str(), mint);
  if (!tokenProgramOf(mint, tp, err)) return false;
  Pk ata = addrs::ata(user, tp, mint);
  uint64_t have = 0;
  if (!sol::tokenBalance(ata, have) ) { err = "balance: " + net::lastError; return false; }
  if (have == 0) {  // sold elsewhere
    positionClear();
    err = "no tokens left";
    return false;
  }
  uint64_t before = 0;
  sol::getBalance(user, before);

  // venue may have changed since buy (curve -> AMM migration): re-resolve
  String dex = pos.dex, pair = pos.pair;
  {
    Curve c;
    String e;
    if (readCurve(addrs::pumpBondingCurve(mint), c, e)) {
      dex = c.complete ? "pumpswap" : "pumpfun";
    } else if (e == "no bonding curve") {
      dex = "pumpswap";
    }
    if (dex == "pumpswap") pair = String(addrs::ammPool(mint).str().c_str());
  }
  uint64_t left = have;
  bool ok = false;
  for (int attempt = 0; attempt <= TX_RETRIES; attempt++) {
    if (attempt > 0) {
      progRetry("sell", attempt);
      delay(1500);
      if (!armed) break;
      uint64_t now = 0;
      if (sol::tokenBalance(ata, now)) {
        if (now == 0) { left = 0; ok = true; break; }   // earlier send landed
        have = now;                                     // sell whatever is left
      }
    }
    if (dex == "pumpswap") {
      if (!Pk::parse(pair.c_str(), pool)) { err = "bad pair"; return false; }
      ok = sellAmm(mint, pool, have, armed, err, sig);
    } else {
      ok = sellCurve(mint, have, armed, err, sig);
    }
    if (!armed) return ok;
    if (!ok) continue;
    // sent without confirm: wait for the tokens to leave the wallet before trusting the sale
    left = have;
    for (int i = 0; i < 10; i++) {
      delay(1000);
      if (sol::tokenBalance(ata, left) && left == 0) break;
    }
    if (left == 0) break;
    err = "sell not landed yet, retry";
    ok = false;
  }
  if (!ok || left > 0) return false;   // position kept
  uint64_t after = before;
  for (int i = 0; i < 4; i++) {
    if (sol::getBalance(user, after) && after != before) break;
    delay(1200);
  }
  delta = (int64_t)after - (int64_t)before;
  return true;
}

bool chainPrice(const String& mint, double& priceNative) {
  Pk m;
  if (!Pk::parse(mint.c_str(), m)) return false;
  double scale = pow(10.0, pos.open ? pos.decimals : 6);
  Curve c;
  String e;
  if (readCurve(addrs::pumpBondingCurve(m), c, e) && !c.complete) {
    if (c.vTok == 0) return false;
    priceNative = ((double)c.vSol / 1e9) / ((double)c.vTok / scale);
    return priceNative > 0;
  }
  // migrated: constant product over vault base and (vault quote + virtual quote)
  Pk pool = addrs::ammPool(m);
  Bytes d; bool found;
  if (!sol::getAccount(pool, 0, 301, d, found) || !found || d.size() < 253) return false;
  Pk baseVault, quoteVault;
  memcpy(baseVault.b, d.data() + 139, 32);
  memcpy(quoteVault.b, d.data() + 171, 32);
  uint64_t virt = rd64(d, 245);
  uint64_t base = 0, quote = 0;
  if (!sol::tokenBalance(baseVault, base) || !sol::tokenBalance(quoteVault, quote) || base == 0) return false;
  if (virt < 200ULL * 1000000000ULL) quote += virt;
  priceNative = ((double)quote / 1e9) / ((double)base / scale);
  return priceNative > 0;
}
