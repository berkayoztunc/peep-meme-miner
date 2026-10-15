#!/usr/bin/env bash
# Serve the installer + settings page on http://localhost:8080 (localhost counts as a secure context for Web Serial).
cd "$(dirname "$0")/../web" && echo "Open http://localhost:8080 in Chrome or Edge" && exec python3 -m http.server 8080 --bind 127.0.0.1
