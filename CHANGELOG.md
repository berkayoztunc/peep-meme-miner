# Changelog

## 1.2.0
- Miner: ATH down sell (off, 10-50%). Tracks the highest price since buy and sells when price falls that much from it, so a run to 3x that turns around still closes in profit. Setting on device and web Trading tab.
- Buy/sell retry up to 3 times, with landed-check before each retry.
- Miner screen shows the last token seen (bought or not).
- Timers and intervals centralized in `timing.h`.

## 1.1.0
- Peep partner signals as the only token source (thesis/radar/call/any).
- Miner mode: auto-buy newest radar signal with stop loss, hold target, timeout.
- Web installer + settings page (Web Serial, ESP Web Tools).
- On-device Ed25519 signing, local PDA derivation, instruction verification.
