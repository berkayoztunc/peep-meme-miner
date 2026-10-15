#!/usr/bin/env bash
# Rebuild web/vendor/tailwind.css from web/index.html (needs node + network once for npx).
set -euo pipefail
cd "$(dirname "$0")/../web/tailwind"
npx --yes tailwindcss@3.4.17 -c tailwind.config.js -i input.css -o ../vendor/tailwind.css --minify
