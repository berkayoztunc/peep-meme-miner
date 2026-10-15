// Program ids and PDA derivation for Pump.fun bonding curve + PumpSwap AMM. Portable (host-testable).
#pragma once
#include "core.h"

namespace addrs {
using core::Pk;
using core::Seeds;

inline Pk pumpProgram() { return Pk::of("6EF8rrecthR5Dkzon8Nwu78hRvfCKubJ14M5uBEwF6P"); }
inline Pk ammProgram() { return Pk::of("pAMMBay6oceH9fJKBRHGP5D4bD4sWpmSwMn52FMfXEA"); }
inline Pk feeProgram() { return Pk::of("pfeeUxB6jkeY1Hxd7CsFCAjcbHA9rWtchMGdZ6VojVZ"); }
inline Pk systemProgram() { return Pk::of("11111111111111111111111111111111"); }
inline Pk tokenProgram() { return Pk::of("TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA"); }
inline Pk token2022Program() { return Pk::of("TokenzQdBNbLqP5VEhdkAS6EPFLC1PHnBqCXEpPxuEb"); }
inline Pk ataProgram() { return Pk::of("ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL"); }
inline Pk computeBudget() { return Pk::of("ComputeBudget111111111111111111111111111111"); }
inline Pk wsolMint() { return Pk::of("So11111111111111111111111111111111111111112"); }

inline Pk pda(const Seeds& s, const Pk& prog) {
  Pk o;
  core::findPda(s, prog, o);
  return o;
}

inline Pk ata(const Pk& owner, const Pk& tokenProg, const Pk& mint) {
  return pda({core::seedPk(owner), core::seedPk(tokenProg), core::seedPk(mint)}, ataProgram());
}

// ---- pump bonding curve ----
inline Pk pumpGlobal() { return pda({core::seedStr("global")}, pumpProgram()); }
inline Pk pumpEventAuthority() { return pda({core::seedStr("__event_authority")}, pumpProgram()); }
inline Pk pumpBondingCurve(const Pk& mint) { return pda({core::seedStr("bonding-curve"), core::seedPk(mint)}, pumpProgram()); }
inline Pk pumpCreatorVault(const Pk& creator) { return pda({core::seedStr("creator-vault"), core::seedPk(creator)}, pumpProgram()); }
inline Pk pumpGlobalVolAcc() { return pda({core::seedStr("global_volume_accumulator")}, pumpProgram()); }
inline Pk pumpUserVolAcc(const Pk& user) { return pda({core::seedStr("user_volume_accumulator"), core::seedPk(user)}, pumpProgram()); }
// fee_config lives under the fee program; second seed is the pump program id bytes
inline Pk pumpFeeConfig() { return pda({core::seedStr("fee_config"), core::seedPk(pumpProgram())}, feeProgram()); }

// sharing_config lives under the fee program
inline Pk pumpSharingConfig(const Pk& mint) { return pda({core::seedStr("sharing-config"), core::seedPk(mint)}, feeProgram()); }

// ---- pump amm ----
inline Pk ammGlobalConfig() { return pda({core::seedStr("global_config")}, ammProgram()); }
inline Pk ammEventAuthority() { return pda({core::seedStr("__event_authority")}, ammProgram()); }
inline Pk ammGlobalVolAcc() { return pda({core::seedStr("global_volume_accumulator")}, ammProgram()); }
inline Pk ammUserVolAcc(const Pk& user) { return pda({core::seedStr("user_volume_accumulator"), core::seedPk(user)}, ammProgram()); }
inline Pk ammFeeConfig() { return pda({core::seedStr("fee_config"), core::seedPk(ammProgram())}, feeProgram()); }
// Canonical pool for a migrated pump token: index 0, creator = pump "pool-authority" PDA, quote = WSOL.
inline Pk ammPool(const Pk& mint) {
  Pk auth = pda({core::seedStr("pool-authority"), core::seedPk(mint)}, pumpProgram());
  core::Bytes idx = {0, 0};
  return pda({core::seedStr("pool"), idx, core::seedPk(auth), core::seedPk(mint), core::seedPk(wsolMint())}, ammProgram());
}
// uninitialised PDA that current AMM builds pass as a read-only account
inline Pk ammPoolV2(const Pk& baseMint) { return pda({core::seedStr("pool-v2"), core::seedPk(baseMint)}, ammProgram()); }
inline Pk ammCreatorVaultAuthority(const Pk& coinCreator) {
  return pda({core::seedStr("creator_vault"), core::seedPk(coinCreator)}, ammProgram());
}
}  // namespace addrs
