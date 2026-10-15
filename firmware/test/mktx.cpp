// Host tool: build unsigned buy tx (cu limit + cu price + create ATA + Orquestra buy ix) for RPC simulation.
// usage: mktx <user> <mint> <blockhash> <ixDataHex> <pubkey:signer:writable;...> <baseTokenProgram> <programId> <wrapWsol 0|1>
// -> base64 wire (zero sig)
#include <cstdio>
#include <sstream>
#include "txbuild.h"
using namespace core;
int main(int argc, char** argv) {
  if (argc < 6) return 2;
  Pk user = Pk::of(argv[1]), mint = Pk::of(argv[2]), bh = Pk::of(argv[3]);
  sol::Ix buy;
  buy.program = argc > 7 ? Pk::of(argv[7]) : addrs::pumpProgram();
  hexdec(argv[4], buy.data);
  std::stringstream ss(argv[5]);
  std::string item;
  while (std::getline(ss, item, ';')) {
    // pubkey:signer:writable
    size_t a = item.find(':'), b = item.find(':', a + 1);
    buy.accts.push_back({Pk::of(item.substr(0, a).c_str()), item.substr(a + 1, b - a - 1) == "1", item.substr(b + 1) == "1"});
  }
  Pk tp = argc > 6 ? Pk::of(argv[6]) : addrs::tokenProgram();
  std::vector<sol::Ix> ixs = {sol::cuLimit(250000), sol::cuPrice(100000),
                              sol::createAtaIdempotent(user, addrs::ata(user, tp, mint), user, mint, tp)};
  if (argc > 8 && std::string(argv[8]) == "1")
    ixs.push_back(sol::createAtaIdempotent(user, addrs::ata(user, addrs::tokenProgram(), addrs::wsolMint()), user,
                                           addrs::wsolMint(), addrs::tokenProgram()));
  ixs.push_back(buy);
  Bytes msg, wire;
  if (!sol::compile(user, ixs, bh, msg)) return 3;
  wire.push_back(1);
  wire.insert(wire.end(), 64, 0);
  wire.insert(wire.end(), msg.begin(), msg.end());
  printf("%s", b64enc(wire.data(), wire.size()).c_str());
  return 0;
}
