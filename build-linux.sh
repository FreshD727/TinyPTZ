#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$root"
build="${TINY_PTZ_BUILD_DIR:-$root/build/linux}"
cmake -S "$root" -B "$build" -G Ninja -DBUILD_QT_DESKTOP=OFF -DBUILD_OBS_PLUGIN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "$build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build" --output-on-failure
if [[ "${1:-}" == "--package" ]]; then
    python3 scripts/package_release.py obs-linux --binary "$build/tiny-ptz-obs.so"
    python3 scripts/package_release.py source
fi
