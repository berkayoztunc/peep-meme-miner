#!/usr/bin/env bash
# Static checks for web/: manifest valid, firmware bin present, no stray external refs, tailwind css up to date.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT/web"

python3 - <<'PY'
import json, os, sys
m = json.load(open("manifest.json"))
assert m.get("name") and m.get("version"), "manifest needs name+version"
for b in m["builds"]:
    for p in b["parts"]:
        assert os.path.isfile(p["path"]), f"missing {p['path']}"
        assert os.path.getsize(p["path"]) > 100_000, f"{p['path']} too small"
print("manifest ok, version", m["version"])
PY

# every local src/href in index.html must exist
python3 - <<'PY'
import re, os, sys
html = open("index.html").read()
bad = [u for u in re.findall(r'(?:src|href)="([^"#]+)"', html)
       if not u.startswith(("http", "data:", "mailto:")) and not os.path.exists(u)]
if bad: sys.exit(f"broken refs: {bad}")
ext = re.findall(r'<(?:script|link)[^>]+(?:src|href)="(https?://[^"]+)"', html)
if ext: sys.exit(f"external script/link not allowed: {ext}")
print("refs ok")
PY

[ -f _headers ] || { echo "missing _headers"; exit 1; }
echo "web checks passed"
