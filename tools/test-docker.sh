#!/bin/sh
# Builds and runs the core tests in Docker (no local compiler needed). Run from the repo root:
#   sh tools/test-docker.sh
set -e
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W 2>/dev/null || pwd):/src" -w /src gcc:14 sh -c '
  g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc -DPEAK_TEST_DATA=\"/src/docs/dumps\" \
      src/core/*.cpp tests/CoreTests.cpp -o /tmp/peak_tests && /tmp/peak_tests'
