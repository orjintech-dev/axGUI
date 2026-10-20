#!/bin/sh
# WebAssembly build (Linux / macOS). Requires an active emsdk (source emsdk_env.sh) and CMake.
set -e
cd "$(dirname "$0")/.."
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
echo "Done: build-web/index.html  ->  python3 -m http.server 8080 -d build-web"
