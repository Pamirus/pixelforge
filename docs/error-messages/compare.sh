#!/bin/sh
# usage: docs/error-messages/compare.sh CXX
# Compiles bad_chain.cpp for every case and both versions with the compiler
# CXX and prints what it says. Run it from the repository root.
CXX=${1:-c++}
"$CXX" --version | head -1
for case in 1 2 3 4; do
  for version in concept enable_if; do
    flags=""
    [ "$version" = enable_if ] && flags="-DVERSION_ENABLE_IF"
    echo
    echo "=== case $case, $version ==="
    "$CXX" -std=c++20 -Iinclude -fsyntax-only -DCASE=$case $flags \
      docs/error-messages/bad_chain.cpp 2>&1
  done
done
