// Portable (no Arduino) transaction building: instruction builders + legacy message compiler.
// Host-tested against Orquestra's own serialized tx: firmware/test/test_tx.cpp
#pragma once
#include <vector>
#include "addrs.h"
#include "core.h"

namespace sol {
using core::Pk;
using core::Bytes;

struct Meta { Pk key; bool signer; bool writable; };
struct Ix { Pk program; std::vector<Meta> accts; Bytes data; };

inline void le32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xFF); }
inline void le64(Bytes& b, uint64_t v) { for (int i = 0; i < 8; i++) b.push_back((v >> (8 * i)) & 0xFF); }

inline Ix cuLimit(uint32_t units) {
  Ix x; x.program = addrs::computeBudget();
  x.data.push_back(2); le32(x.data, units);
  return x;
}
inline Ix cuPrice(uint64_t micro) {
  Ix x; x.program = addrs::computeBudget();
  x.data.push_back(3); le64(x.data, micro);
  return x;
}
inline Ix createAtaIdempotent(const Pk& payer, const Pk& ata, const Pk& owner, const Pk& mint, const Pk& tokenProg) {
  Ix x; x.program = addrs::ataProgram();
  x.accts = {{payer, true, true}, {ata, false, true}, {owner, false, false},
             {mint, false, false}, {addrs::systemProgram(), false, false}, {tokenProg, false, false}};
  x.data.push_back(1);
  return x;
}
inline Ix transferSol(const Pk& from, const Pk& to, uint64_t lamports) {
  Ix x; x.program = addrs::systemProgram();
  x.accts = {{from, true, true}, {to, false, true}};
  le32(x.data, 2); le64(x.data, lamports);
  return x;
}
inline Ix syncNative(const Pk& wsolAta) {
  Ix x; x.program = addrs::tokenProgram();
  x.accts = {{wsolAta, false, true}};
  x.data.push_back(17);
  return x;
}
inline Ix closeAccount(const Pk& acct, const Pk& dest, const Pk& owner) {
  Ix x; x.program = addrs::tokenProgram();
  x.accts = {{acct, false, true}, {dest, false, true}, {owner, true, false}};
  x.data.push_back(9);
  return x;
}

namespace detail {
struct K { Pk key; bool s, w; };
inline void addKey(std::vector<K>& ks, const Pk& k, bool s, bool w) {
  for (auto& e : ks) if (e.key == k) { e.s |= s; e.w |= w; return; }
  ks.push_back({k, s, w});
}
inline int idxOf(const std::vector<K>& ks, const Pk& k) {
  for (size_t i = 0; i < ks.size(); i++) if (ks[i].key == k) return (int)i;
  return -1;
}
inline void shortvec(Bytes& b, size_t v) {
  while (true) {
    uint8_t e = v & 0x7F; v >>= 7;
    if (v) b.push_back(e | 0x80); else { b.push_back(e); break; }
  }
}
}  // namespace detail

inline bool compile(const Pk& payer, const std::vector<Ix>& ixs, const Pk& blockhash, Bytes& msg) {
  std::vector<detail::K> ks;
  detail::addKey(ks, payer, true, true);
  for (auto& ix : ixs) {
    for (auto& m : ix.accts) detail::addKey(ks, m.key, m.signer, m.writable);
    detail::addKey(ks, ix.program, false, false);
  }
  // order: payer, signers(w), signers(ro), nonsigners(w), nonsigners(ro)
  std::vector<detail::K> o;
  o.push_back(ks[0]);
  for (size_t i = 1; i < ks.size(); i++) if (ks[i].s && ks[i].w) o.push_back(ks[i]);
  size_t nSignW = o.size();
  for (size_t i = 1; i < ks.size(); i++) if (ks[i].s && !ks[i].w) o.push_back(ks[i]);
  size_t nSign = o.size();
  if (nSign != 1) return false;  // only the wallet may sign
  for (size_t i = 1; i < ks.size(); i++) if (!ks[i].s && ks[i].w) o.push_back(ks[i]);
  for (size_t i = 1; i < ks.size(); i++) if (!ks[i].s && !ks[i].w) o.push_back(ks[i]);
  size_t nRoUnsigned = 0;
  for (size_t i = nSign; i < o.size(); i++) if (!o[i].w) nRoUnsigned++;
  if (o.size() > 64) return false;

  msg.clear();
  msg.push_back((uint8_t)nSign);
  msg.push_back((uint8_t)(nSign - nSignW));
  msg.push_back((uint8_t)nRoUnsigned);
  detail::shortvec(msg, o.size());
  for (auto& k : o) msg.insert(msg.end(), k.key.b, k.key.b + 32);
  msg.insert(msg.end(), blockhash.b, blockhash.b + 32);
  detail::shortvec(msg, ixs.size());
  for (auto& ix : ixs) {
    msg.push_back((uint8_t)detail::idxOf(o, ix.program));
    detail::shortvec(msg, ix.accts.size());
    for (auto& m : ix.accts) msg.push_back((uint8_t)detail::idxOf(o, m.key));
    detail::shortvec(msg, ix.data.size());
    msg.insert(msg.end(), ix.data.begin(), ix.data.end());
  }
  return true;
}
}  // namespace sol
