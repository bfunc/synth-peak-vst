#!/bin/sh
# Compile check + page pictures in Linux Docker (Windows release builds come from CI). Run from the repo root:
#   sh tools/build-linux.sh [target]      default target: peak_snapshot (builds the plugin code too)
# The build tree lives in the Docker volume "peak-build" so JUCE is fetched once.
set -e
TARGET=${1:-peak_snapshot}
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W 2>/dev/null || pwd):/src" -v peak-build:/build -w /build peak-juce-build sh -c "
  cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPEAK_SNAPSHOT=ON >/dev/null &&
  cmake --build /build --target $TARGET -- -k 0 2>&1 | grep -E 'error|warning: unused|FAILED|Linking' | head -150"
