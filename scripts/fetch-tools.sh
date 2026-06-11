#!/usr/bin/env bash
# Descarga makerom y bannertool (Linux x86_64) a tools/ para poder hacer 'make cia'.
set -e
cd "$(dirname "$0")/.."
mkdir -p tools && cd tools

echo ">> makerom"
curl -sL -o makerom.zip \
  "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-ubuntu_x86_64.zip"
unzip -o -q makerom.zip && chmod +x makerom && rm -f makerom.zip

echo ">> bannertool"
curl -sL -o bannertool.zip \
  "https://github.com/carstene1ns/3ds-bannertool/releases/download/1.2.2/bannertool-1.2.2-linux.zip"
unzip -o -q bannertool.zip
cp "$(find . -name bannertool -type f | head -1)" ./bannertool
chmod +x bannertool && rm -f bannertool.zip
echo ">> Listo: tools/makerom y tools/bannertool"
