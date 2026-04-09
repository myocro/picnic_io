#!/bin/bash

set -euo pipefail

# -B build で、ビルド用ディレクトリを明示的に指定
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# ビルド実行
cmake --build build

# テスト実行
cd build && ctest --output-on-failure --verbose
