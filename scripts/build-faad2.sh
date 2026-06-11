#!/usr/bin/env bash
# Cross-compila faad2 (decodificador AAC) para 3DS y deja lib/libfaad.a + include/neaacdec.h
# Requiere: devkitPro (3ds-dev), cmake, git.
set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

: "${DEVKITPRO:?Define DEVKITPRO (source /etc/profile.d/devkit-env.sh)}"

TMP="$(mktemp -d)"
echo ">> Clonando faad2 en $TMP"
git clone --depth 1 https://github.com/knik0/faad2 "$TMP/faad2"

cmake -S "$TMP/faad2" -B "$TMP/faad2/build" \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" \
  -DBUILD_SHARED_LIBS=OFF -DFAAD_BUILD_CLI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build "$TMP/faad2/build" -j"$(nproc)"

mkdir -p "$ROOT/lib" "$ROOT/include"
cp "$TMP/faad2/build/libfaad.a" "$ROOT/lib/libfaad.a"
cp "$TMP/faad2/include/neaacdec.h" "$ROOT/include/neaacdec.h"
echo ">> Listo: lib/libfaad.a + include/neaacdec.h"
rm -rf "$TMP"
