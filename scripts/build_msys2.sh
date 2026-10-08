#!/usr/bin/env bash
set -euo pipefail
if [[ "${MSYSTEM:-}" != UCRT64 ]]; then
  echo 'Run this script in the MSYS2 UCRT64 terminal.' >&2
  exit 1
fi
cd "$(dirname "$0")/.."
cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build-ucrt64 --parallel
ctest --test-dir build-ucrt64 --output-on-failure
