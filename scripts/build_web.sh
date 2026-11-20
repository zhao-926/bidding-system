#!/usr/bin/env bash
# build_web.sh — 用 Emscripten 把 core/ 編成 WebAssembly（Phase 6 完成）
#
# 目前（Phase 1）wasm/bindings.cpp 還不存在，所以只做環境檢查並提示。

cd "$(dirname "$0")/.." || exit 1

if [ ! -f wasm/bindings.cpp ]; then
    echo "build_web: wasm/bindings.cpp 尚未建立（Phase 6 才會實作），略過。"
    exit 0
fi

if ! command -v emcc > /dev/null 2>&1; then
    echo "build_web: 找不到 emcc，請先安裝並啟用 Emscripten（emsdk）。" >&2
    exit 1
fi

emcc wasm/bindings.cpp core/*.cpp -Icore -std=c++17 -O2 --bind \
    -s MODULARIZE=1 -s EXPORT_NAME=createAuction \
    -o web/auction.js
