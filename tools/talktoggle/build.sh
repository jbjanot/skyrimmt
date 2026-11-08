#!/bin/bash
# Build talktoggle.exe (x86_64 mingw: brew install mingw-w64) → build/talktoggle/talktoggle.exe
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; OUT="$ROOT/build/talktoggle"; mkdir -p "$OUT"
x86_64-w64-mingw32-gcc -O2 -s -Wall -o "$OUT/talktoggle.exe" "$ROOT/tools/talktoggle/talktoggle.c"
echo "$OUT/talktoggle.exe"
