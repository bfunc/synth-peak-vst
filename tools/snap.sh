#!/bin/sh
# Draw every editor page to PNG (build-snap/) with the test dump loaded. Run after tools/build-linux.sh.
set -e
mkdir -p build-snap
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W 2>/dev/null || pwd):/src" -v peak-build:/build -w /src peak-juce-build \
  sh -c "/build/peak_snapshot_artefacts/Release/peak_snapshot /src/build-snap"
