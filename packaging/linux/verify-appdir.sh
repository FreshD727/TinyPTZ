#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
stage="${1:-$root/build/linux-desktop/AppDir}"
for script in build-linux-desktop.sh packaging/linux/AppRun packaging/linux/verify-appdir.sh; do
    bash -n "$root/$script"
done
export XDG_RUNTIME_DIR="$root/build/linux-desktop/xdg-runtime"
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
QT_QPA_PLATFORM=offscreen "$stage/AppRun" --check-runtime --data-dir "$root/build/linux-desktop/portable-qa-data"
LD_LIBRARY_PATH="$stage/usr/lib" GST_PLUGIN_SYSTEM_PATH_1_0="$stage/usr/lib/gstreamer-1.0" \
    GST_PLUGIN_SCANNER_1_0="$stage/usr/libexec/gst-plugin-scanner" \
    GST_REGISTRY_1_0="$root/build/linux-desktop/gstreamer-validation.bin" \
    python3 "$root/packaging/linux/check-video-runtime.py" "$stage"
