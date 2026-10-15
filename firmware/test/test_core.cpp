// Host test: g++ -std=c++17 -I../src test_core.cpp -o /tmp/test_core && /tmp/test_core
#include <cstdio>
#include "addrs.h"
using namespace core;

static int fails = 0;
#define CHECK_EQ(name, got, want)                                                   \
  do {                                                                              \
    std::string g = (got), w = (want);                                              \
    if (g == w) printf("ok   %s\n", name);                                          \
    else { printf("FAIL %s\n  got  %s\n  want %s\n", name, g.c_str(), w.c_str()); fails++; } \
  } while (0)

int main() {
  // sha256("abc")
  {
    Sha256 h; h.update((const uint8_t*)"abc", 3);
    uint8_t d[32]; h.final(d);
    Bytes b(d, d + 32);
    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", d[i]);
    CHECK_EQ("sha256(abc)", hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  }
  // base58 round trip
  {
    Pk p = Pk::of("6EF8rrecthR5Dkzon8Nwu78hRvfCKubJ14M5uBEwF6P");
    CHECK_EQ("b58 roundtrip", p.str(), "6EF8rrecthR5Dkzon8Nwu78hRvfCKubJ14M5uBEwF6P");
    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", p.b[i]);
    CHECK_EQ("pump program bytes", hex, "0156e0f693665acf44db1568bf175baa5189cb97f5d2ff3b655d2bb6fd6d18b0");
  }
  CHECK_EQ("b64", b64enc((const uint8_t*)"hello!?", 7), "aGVsbG8hPw==");

  // PDA vectors from Orquestra /pda/derive (2026-10-14)
  Pk mint = Pk::of("2qAU9FxwNTiykfxwcURPoevmsMfDTwX3Fr2Kto3qhmLV");
  Pk user = Pk::of("9WzDXwBbmkg8ZTbNMqUxvQRAyrZzDsGYdLVL9zYtAWWM");
  Pk creator = Pk::of("DLQuKV6NUSvxnbwZLuFTCdmSYAg5T9TXcxFE5fxd9asf");
  CHECK_EQ("pump global", addrs::pumpGlobal().str(), "4wTV1YmiEkRvAtNtsSGPtUrqRYQMe5SKy2uB4Jjaxnjf");
  CHECK_EQ("pump event_authority", addrs::pumpEventAuthority().str(), "Ce6TQqeHC9p8KetsN6JsjHK7UTZk7nasjjnr7XxXp9F1");
  CHECK_EQ("pump global_vol_acc", addrs::pumpGlobalVolAcc().str(), "Hq2wp8uJ9jCPsYgNHex8RtqdvMPfVGoYwjvF1ATiwn2Y");
  CHECK_EQ("bonding_curve", addrs::pumpBondingCurve(mint).str(), "9Bo8borJpbQoAQxEnEBhooGxmTJSRgcZXLGpvj9mWw4n");
  CHECK_EQ("user_vol_acc", addrs::pumpUserVolAcc(user).str(), "ASZB5x4iWFaadZo6DxwyhNVdAH4NfZryPhVeS3Ca9RE6");
  CHECK_EQ("creator_vault", addrs::pumpCreatorVault(creator).str(), "FPbZq4Tg6acQt8FzpMhZJiXduPBhcjfxMz3dYMEmHqXn");
  CHECK_EQ("user ata (tokenkeg)", addrs::ata(user, addrs::tokenProgram(), mint).str(), "Edh7GLT6rx9tD2BmvmjN1fMheXDFKypwdJ1kWisTeyxY");
  CHECK_EQ("amm event_authority", addrs::ammEventAuthority().str(), "GS4CU59F31iL7aR2Q8zVS8DRrcRnXX1yjQ66TqNVQnaR");
  CHECK_EQ("amm global_vol_acc", addrs::ammGlobalVolAcc().str(), "C2aFPdENg4A2HQsmrd5rTw5TaYBX5Ku887cWjbFKtZpw");

  // vectors from a real mainnet buy_exact_quote_in_v2 tx on mint 1YRchB3U...pump (bonding curve BTWFF1dz...)
  Pk mint2 = Pk::of("1YRchB3U7Dyo1XqZJLgpLpSREN8H2U9v5cw4ZZkpump");
  CHECK_EQ("bonding_curve (real tx)", addrs::pumpBondingCurve(mint2).str(), "BTWFF1dzCXD7uYkY29r9F1CvQtDBAY4JfhoDMtGRZfun");
  CHECK_EQ("sharing_config (real tx)", addrs::pumpSharingConfig(mint2).str(), "2mjy3kGz7hzrmnHQo6Yd3mRZuEW2MqBf5fWJsk9RRr83");
  CHECK_EQ("assoc base bc, token2022 (real tx)",
           addrs::ata(addrs::pumpBondingCurve(mint2), addrs::token2022Program(), mint2).str(),
           "8vAw1W2RSopVtez1X1c2zSdJ4azBdP8Fpxuyr9VKzqrT");
  CHECK_EQ("assoc quote bc wsol (real tx)",
           addrs::ata(addrs::pumpBondingCurve(mint2), addrs::tokenProgram(), addrs::wsolMint()).str(),
           "DTyzZMRWHUDEfrQGXX1vt4ciUjCA1miUau7R1ab8zhDm");
  CHECK_EQ("assoc quote fee recipient (real tx)",
           addrs::ata(Pk::of("62qc2CNXwrYqQScmEdiZFFAnJR262PxWEuNQtxfafNgV"), addrs::tokenProgram(), addrs::wsolMint()).str(),
           "94qWNrtmfn42h3ZjUZwWvK1MEo9uVmmrBPd2hpNjYDjb");
  CHECK_EQ("assoc quote buyback recipient (real tx)",
           addrs::ata(Pk::of("9M4giFFMxmFGXtc3feFzRai56WbBqehoSeRE5GK7gf7"), addrs::tokenProgram(), addrs::wsolMint()).str(),
           "GAFuhgcd328SkkBYHpfadzmef9hTGAFRCi9QoCnsZQug");

  CHECK_EQ("amm pool_v2 (real tx)", addrs::ammPoolV2(Pk::of("7fumw7GYTHZJJxkKJ8sSh1CqTdHmB234vX3xsKjepump")).str(),
           "CRQ5wPM5efu1eLghdx4zhBbde2g6q53Lhr7zgjWGbZSf");
  CHECK_EQ("amm buyback ata (real tx)",
           addrs::ata(Pk::of("EHAAiTxcdDwQ3U4bU6YcMsQGaekdzLS3B5SmYo46kJtL"), addrs::tokenProgram(), addrs::wsolMint()).str(),
           "CA7v8gHfbquYXyDnDx6QxWW8hmL1H7X6Y2RYDrGLnuck");

  CHECK_EQ("amm canonical pool (real)", addrs::ammPool(Pk::of("7fumw7GYTHZJJxkKJ8sSh1CqTdHmB234vX3xsKjepump")).str(),
           "8iSy6UUmgDqXsVAKL8tJyRUXSTLdMwPuC6QXW5j3fhjA");

  // fee_config: Orquestra derives under the wrong program; real value is under the fee program.
  printf("info pump fee_config (fee program)  = %s\n", addrs::pumpFeeConfig().str().c_str());
  printf("info amm  fee_config (fee program)  = %s\n", addrs::ammFeeConfig().str().c_str());
  printf("info amm  global_config             = %s\n", addrs::ammGlobalConfig().str().c_str());
  CHECK_EQ("pump fee_config (known on-chain)", addrs::pumpFeeConfig().str(), "8Wf5TiAheLUqBrKXeYg2JtAFFMWtKdG2BSFgqUcPVwTt");
  CHECK_EQ("amm fee_config (known on-chain)", addrs::ammFeeConfig().str(), "5PHirr8joyTMp9JMm6nW7hNDVyEYdkzDqazxPD7RaTjx");
  CHECK_EQ("amm global_config (known on-chain)", addrs::ammGlobalConfig().str(), "ADyA8hdefvWN2dbGGWFotbzWxrAvLW83WG6QCVXvJKqw");

  printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
  return fails ? 1 : 0;
}
