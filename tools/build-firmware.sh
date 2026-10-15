#!/usr/bin/env bash
# Build firmware and produce web/firmware/peep-meme-miner-merged.bin for the browser installer.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PIO="${PIO:-$ROOT/tools/.venv/bin/pio}"
[ -x "$PIO" ] || PIO="$(command -v pio || true)"
[ -n "$PIO" ] || { echo "PlatformIO not found. Run: python3 -m venv tools/.venv && tools/.venv/bin/pip install platformio"; exit 1; }

python3 "$ROOT/tools/gen-logo.py"
( cd "$ROOT/firmware" && "$PIO" run )

B="$ROOT/firmware/.pio/build/m5stick-c"
PKG="$HOME/.platformio/packages"
ESPTOOL="$PKG/tool-esptoolpy/esptool.py"
BOOT_APP0="$(ls "$PKG"/framework-arduinoespressif32*/tools/partitions/boot_app0.bin | head -1)"
PY="${PYTHON:-$ROOT/tools/.venv/bin/python3}"; [ -x "$PY" ] || PY=python3

mkdir -p "$ROOT/web/firmware"
"$PY" "$ESPTOOL" --chip esp32 merge_bin \
  -o "$ROOT/web/firmware/peep-meme-miner-merged.bin" \
  --flash_mode dio --flash_freq 40m --flash_size 8MB \
  0x1000 "$B/bootloader.bin" \
  0x8000 "$B/partitions.bin" \
  0xe000 "$BOOT_APP0" \
  0x10000 "$B/firmware.bin"

VER="$(grep -o 'FW_VERSION "[^"]*"' "$ROOT/firmware/src/proto.cpp" | cut -d'"' -f2)"
cat > "$ROOT/web/manifest.json" <<JSON
{
  "name": "Peep meme miner",
  "version": "${VER:-1.0.0}",
  "new_install_prompt_erase": true,
  "builds": [
    { "chipFamily": "ESP32", "parts": [ { "path": "firmware/peep-meme-miner-merged.bin", "offset": 0 } ] }
  ]
}
JSON
ls -l "$ROOT/web/firmware/peep-meme-miner-merged.bin"
