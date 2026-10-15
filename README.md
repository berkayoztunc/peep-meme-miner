# Peep meme miner

[![CI](https://github.com/berkayoztunc/peep-meme-miner/actions/workflows/ci.yml/badge.svg)](https://github.com/berkayoztunc/peep-meme-miner/actions/workflows/ci.yml) ![License: MIT](https://img.shields.io/badge/license-MIT-blue)

> Not financial advice. Open-source hobby software, provided as is.

Token list: Peep partner signals API only (pump.fun mints), newest token (createdAtMs) first, no market-cap filter. Signal type selectable: thesis (default), radar, call, any. Position price is read on-chain (DexScreener fallback). No Birdeye.

Pump.fun trader for the **M5StickC Plus2** (ESP32-PICO-V3-02, 8 MB flash; not the older Plus with the AXP192). Browse trending pump.fun / PumpSwap tokens on the stick,
buy with one button, watch live PnL, sell with one button. Trades are built with **Orquestra**,
token signals come from the **Peep partner API**, prices are read on-chain (DexScreener fallback), and transactions are signed **on the device** with a burner key.

> Experimental software. Memecoins can go to zero or be unsellable. Use a burner wallet and tiny amounts.
> Starts **disarmed** (simulate only). No warranty.

## Quick start (installer + settings page)

```bash
tools/serve.sh            # serves web/ on http://localhost:8080
```

1. Open http://localhost:8080 in **Chrome or Edge** (Web Serial required).
2. **Install** tab: plug in the stick, click *Install Peep meme miner*, tick *Erase device*.
3. **Settings** tab: *Connect stick*, enter Wi-Fi, Peep API key, trade size, then *Save*.
4. **Wallet** tab: *Generate new key on stick* (recommended) or import one. Fund the shown address.
5. Leave **Armed** off and press A on a token: it simulates the tx. Arm only when that works.

Rebuild the firmware yourself: `tools/build-firmware.sh` (creates `tools/.venv` PlatformIO first time:
`python3 -m venv tools/.venv && tools/.venv/bin/pip install platformio`).

## Modes (menu on boot)

| Menu item | What it does |
|---|---|
| Manual | Token list, buy/sell with buttons (as below). Hold A in the list: back to menu. |
| Miner | Every N sec (10/30/60) fetch newest **radar** signal. New mint: beep, auto-buy (real SOL, ignores Armed), open PnL screen. Sells at stop loss (-20/-30/-50%), hold target (2x/3x/4x) or timeout (default 10 min), first wins. Same mint is never bought twice (remembered across reboots). First run only records the current newest as baseline. Hold Power: back to menu. |
| Settings | On-device miner settings: A = value up, Power = value down, B = next row, then Save or Exit. Same fields on the web Trading tab. |

## Buttons

| Screen | A (front) | B (side) | Power |
|---|---|---|---|
| Token card | buy | next token | short: previous token, hold B: refresh, hold Power: off |
| Buy confirm | confirm | cancel | |
| Position | sell (asks again) | locked: stays until sold | |
| Result | continue | continue | |

`c` = bonding curve, `a` = PumpSwap AMM in the list. Idle in list = powers off; press the power button to start again.

## How it works

```
M5StickC Plus2 --HTTPS--> Peep API     (signals / token list)
               --HTTPS--> DexScreener  (fallback price)
               --HTTPS--> Orquestra    (builds the Pump / PumpSwap instruction)
               --HTTPS--> Solana RPC   (accounts, send, confirm)
  PDAs derived locally  ->  instruction from Orquestra checked against what we asked for
  -> compose tx (compute budget, ATA create, wrap/unwrap) -> ed25519 sign on device -> send
```

- Bonding curve: `buy_exact_quote_in_v2` / `sell_v2` (WSOL quote, buyback fee recipient from Global).
- PumpSwap: `buy_exact_quote_in` / `sell` with the 3 extra accounts current mainnet requires.
- Token-2022 tokens supported (token program detected from the mint).

## Security model

- Key: 32-byte seed in the stick's flash (NVS), **unencrypted**. Anyone with the stick can extract it. Burner only.
- The key is never printed or returned over serial; `gen_wallet` creates it with the ESP32 hardware RNG.
- TLS certificate validation is **off** (no CA bundle in flash). Mitigation: before signing, the firmware
  checks the Orquestra instruction matches exactly what it requested (program, account set, amounts, only the
  wallet signs). A MITM can read public data and make requests fail, not redirect funds.
- Wi-Fi password and API keys are write-only in the Settings page.
- Public RPC works but is rate limited; set a Helius/QuickNode URL in Settings for reliability.

## Repo layout

```
firmware/src   core.h (base58/sha256/PDA), addrs.h, txbuild.h (portable, host-tested)
               pump.cpp (trade flows), market.cpp, orquestra.cpp, solana.cpp, ui.cpp, proto.cpp, main.cpp
firmware/test  host tests (PDAs vs mainnet, tx bytes vs Orquestra, Ed25519 RFC vector)
web/           installer + settings page (ESP Web Tools vendored), firmware/*.bin, manifest.json
tools/         build-firmware.sh, serve.sh, test.sh
PLAN_V2.md     design notes and findings
```

Run host tests: `tools/test.sh`.

## Verified vs not verified

Verified (see `firmware/test`, and mainnet `simulateTransaction` with a funded dummy payer):
PDA derivation vs on-chain values, tx message bytes vs Orquestra, Ed25519 vs RFC 8032, v2 curve buy and
PumpSwap buy simulate with no error on current mainnet tokens, Token-2022 included, firmware compiles for
M5StickC Plus2 and runs on a real Plus2 (serial protocol answers hello/get_config/status), web page serves and the merged image has valid bootloader/partition/app headers.

**Not verified:** on-device screens and buttons as seen by a human, a real landed transaction, curve `sell_v2` and
AMM `sell` simulation (needs a token holder), TP/SL timing, battery life. Test with the stick disarmed first,
then a tiny armed trade.

## Flash from the command line

```bash
tools/build-firmware.sh
tools/.venv/bin/python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 \
  --port /dev/cu.usbserial-XXXX --baud 460800 erase_flash
tools/.venv/bin/python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 \
  --port /dev/cu.usbserial-XXXX --baud 460800 write_flash -z --flash_mode dio --flash_freq 40m \
  --flash_size 8MB 0x0 web/firmware/peep-meme-miner-merged.bin
```

## Logo

Put your logo at `firmware/assets/logo.png` (square PNG, ideally 86x86, under 20 KB) and run
`tools/build-firmware.sh`. It is shown on the right of every token card and in the web page header.
A default eye mark is generated if the file is missing.

## License / security

MIT, see [LICENSE](LICENSE). Security model and reporting: [SECURITY.md](SECURITY.md). Contributing: [CONTRIBUTING.md](CONTRIBUTING.md).
