#!/usr/bin/env bash
# Host tests: core/PDA/tx compile vs Orquestra, Ed25519 vector. No device needed.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT/firmware"
g++ -std=c++17 -O1 -Isrc test/test_core.cpp -o /tmp/ps_test_core && /tmp/ps_test_core
g++ -std=c++17 -O1 -Isrc test/test_tx.cpp -o /tmp/ps_test_tx && /tmp/ps_test_tx
L=.pio/libdeps/m5stick-c/Crypto
if [ -d "$L" ]; then
  g++ -std=c++11 -O1 -I$L test/test_ed25519.cpp $L/Ed25519.cpp $L/Curve25519.cpp $L/BigNumberUtil.cpp $L/SHA512.cpp $L/Hash.cpp $L/Crypto.cpp -o /tmp/ps_test_ed && /tmp/ps_test_ed
fi
