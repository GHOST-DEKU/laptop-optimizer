#!/usr/bin/env bash
# Fails if the SHA-256 shown on index.html differs from the committed LaptopOptimizer.exe.
set -eu
cd "$(dirname "$0")/.."
actual=$(sha256sum LaptopOptimizer.exe | cut -d' ' -f1)
if grep -q "$actual" index.html; then echo "OK: site hash matches LaptopOptimizer.exe ($actual)"
else echo "MISMATCH: index.html does not show $actual. Update the SHA-256 on the page." >&2; exit 1; fi
