// Host tool: AMM buy tx as firmware composes it, for RPC simulation.
// usage: mkamm <user> <mint> <blockhash> <dataHex> <pubkey:s:w;...> <baseTp> <buybackRecipient> <spendLamports> [sell]
#include <cstdio>
#include <sstream>
#include "txbuild.h"
using namespace core;
int main(int argc, char** argv) {
  if (argc < 9) return 2;
  Pk user = Pk::of(argv[1]), mint = Pk::of(argv[2]), bh = Pk::of(argv[3]), baseTp = Pk::of(argv[6]), buyback = Pk::of(argv[7]);
  uint64_t spend = strtoull(argv[8], nullptr, 10);
  bool sell = argc > 9;
  sol::Ix ix;
  ix.program = addrs::ammProgram();
  hexdec(argv[4], ix.data);
  std::stringstream ss(argv[5]);
  std::string item;
  while (std::getline(ss, item, ';')) {
    size_t a = item.find(':'), b = item.find(':', a + 1);
    ix.accts.push_back({Pk::of(item.substr(0, a).c_str()), item.substr(a + 1, b - a - 1) == "1", item.substr(b + 1) == "1"});
  }
  ix.accts.push_back({addrs::ammPoolV2(mint), false, false});
  ix.accts.push_back({buyback, false, false});
  ix.accts.push_back({addrs::ata(buyback, addrs::tokenProgram(), addrs::wsolMint()), false, true});
  if (!sell && ix.data.size() == 25) ix.data.pop_back();
  Pk wsolAta = addrs::ata(user, addrs::tokenProgram(), addrs::wsolMint());
  Pk userBase = addrs::ata(user, baseTp, mint);
  std::vector<sol::Ix> ixs = {sol::cuLimit(250000), sol::cuPrice(100000),
                              sol::createAtaIdempotent(user, wsolAta, user, addrs::wsolMint(), addrs::tokenProgram())};
  if (!sell) {
    ixs.push_back(sol::createAtaIdempotent(user, userBase, user, mint, baseTp));
    ixs.push_back(sol::transferSol(user, wsolAta, spend));
    ixs.push_back(sol::syncNative(wsolAta));
  }
  ixs.push_back(ix);
  ixs.push_back(sol::closeAccount(wsolAta, user, user));
  Bytes msg, wire;
  if (!sol::compile(user, ixs, bh, msg)) return 3;
  wire.push_back(1);
  wire.insert(wire.end(), 64, 0);
  wire.insert(wire.end(), msg.begin(), msg.end());
  printf("%s", b64enc(wire.data(), wire.size()).c_str());
  return 0;
}
