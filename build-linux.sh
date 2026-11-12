#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$root"
build="${TINY_PTZ_BUILD_DIR:-$root/build/linux}"
cmake -S "$root" -B "$build" -G Ninja -DBUILD_QT_DESKTOP=OFF -DBUILD_OBS_PLUGIN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "$build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
QT_QPA_PLATFORM=offscreen ctest --test-dir "$build" --output-on-failure
if [[ "${1:-}" == "--package" ]]; then
    arch="$(uname -m)"
    parent="$root/dist/TinyPTZ-OBS-v0.6.2-Linux-$arch"
    stage="$parent/tiny-ptz-obs"
    cmake --install "$build" --prefix "$stage"
    strip --strip-unneeded "$stage/bin/64bit/tiny-ptz-obs.so"
    archive="$root/dist/TinyPTZ-OBS-v0.6.2-Linux-$arch-Release.tar.gz"
    tar -czf "$archive" -C "$parent" tiny-ptz-obs
    (cd "$parent" && cmake -E tar cf "$root/dist/TinyPTZ-OBS-v0.6.2-Linux-$arch-Release.zip" --format=zip tiny-ptz-obs)
    printf '\nLinux plugin: %s\n' "$archive"
fi
