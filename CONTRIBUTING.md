# Contributing

## Setup
```bash
python3 -m venv tools/.venv && tools/.venv/bin/pip install platformio
tools/test.sh            # host tests, no device needed
tools/build-firmware.sh  # build + merge web/firmware/peep-meme-miner-merged.bin
tools/serve.sh           # installer page on http://localhost:8080
tools/check-web.sh       # static checks for web/
```

## Pull requests
- Keep changes focused; one concern per PR.
- Run `tools/test.sh` and `tools/check-web.sh`.
- If you change firmware, rebuild the merged bin and bump `FW_VERSION` (`firmware/src/proto.cpp`); `manifest.json` is generated.
- If you change `web/index.html` classes, run `tools/build-css.sh`.
- Never commit keys, seeds, Wi-Fi credentials or API tokens.
- Anything touching transaction building must keep the instruction-match check intact and add/extend a host test.
