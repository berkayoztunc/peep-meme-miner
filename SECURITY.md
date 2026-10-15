# Security

## Threat model
- The wallet key is a 32-byte seed in the stick's flash (NVS), **unencrypted**. Anyone with physical access can extract it. Use a burner wallet with a small balance only.
- The key is never printed or returned over serial. `gen_wallet` uses the ESP32 hardware RNG.
- TLS certificate validation is **off** on the device (no CA bundle in flash). Before signing, firmware verifies the Orquestra instruction matches what it requested (program, accounts, amounts, only the wallet signs). A MITM can read public data or make requests fail, not redirect funds.
- Wi-Fi password and API keys are write-only in the settings page.
- Miner mode buys with real SOL regardless of the Armed flag.

## Reporting a vulnerability
Use GitHub's private vulnerability reporting (Security tab > Report a vulnerability). Do not open a public issue for anything that could lead to loss of funds. No bounty program; credit given on request.

## Out of scope
Loss from memecoin price moves, rug pulls, unsellable tokens, or RPC/API outages.
