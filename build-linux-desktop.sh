#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$root"
build="$root/build/linux-desktop"
cmake -S "$root" -B "$build" -G Ninja -DBUILD_QT_DESKTOP=ON -DBUILD_OBS_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build "$build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
ctest --test-dir "$build" --output-on-failure
if [[ "${1:-}" != "--appimage" ]]; then exit 0; fi
[[ "$(uname -m)" == "x86_64" ]] || { echo 'The pinned packaging tools target x86_64.' >&2; exit 1; }
tools="$root/build/appimage-tools"
mkdir -p "$tools"
for name in linuxdeploy linuxdeploy-plugin-qt; do
    file="$name-x86_64.AppImage"
    if [[ ! -f "$tools/$file" ]]; then
        curl -fL --retry 2 -o "$tools/$file" "https://github.com/linuxdeploy/$name/releases/download/continuous/$file"
    fi
    chmod +x "$tools/$file"
done
(cd "$tools" && sha256sum -c "$root/packaging/linux/appimage-tools.sha256")
appdir="$build/AppDir"
# This stage contains generated files only; the source and user data are elsewhere.
[[ "$(realpath -m "$appdir")" == "$root/build/linux-desktop/AppDir" ]] || exit 1
rm -rf -- "$appdir"
cmake --install "$build" --prefix "$appdir/usr" --component desktop
triplet="$(gcc -dumpmachine)"
gst="/usr/lib/$triplet/gstreamer-1.0"
mkdir -p "$appdir/usr/lib/gstreamer-1.0" "$appdir/usr/libexec"
for plugin in app coreelements typefindfunctions video4linux2 videotestsrc videoconvertscale videorate jpeg playback camerabin encoding autodetect audioconvert audioresample volume pulseaudio; do
    cp "$gst/libgst$plugin.so" "$appdir/usr/lib/gstreamer-1.0/"
done
cp "/usr/lib/$triplet/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner" "$appdir/usr/libexec/"
export APPIMAGE_EXTRACT_AND_RUN=1 QMAKE="${QMAKE:-/usr/bin/qmake6}"
export EXTRA_PLATFORM_PLUGINS='libqwayland-egl.so;libqwayland-generic.so;libqoffscreen.so;libqminimal.so'
export EXTRA_QT_MODULES='waylandclient'
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --executable "$build/TinyPTZ" --desktop-file "$root/packaging/linux/tinyptz.desktop" \
    --icon-file "$root/assets/tinyptz.png" --plugin qt \
    --deploy-deps-only "$appdir/usr/lib/gstreamer-1.0" \
    --deploy-deps-only "$appdir/usr/libexec/gst-plugin-scanner" \
    --custom-apprun "$root/packaging/linux/AppRun"
python3 "$root/packaging/linux/collect-notices.py" "$appdir"
bash "$root/packaging/linux/verify-appdir.sh" "$appdir"
mkdir -p "$root/dist"
export OUTPUT="$root/dist/TinyPTZ-v0.7.5-Linux-x86_64.AppImage" VERSION=0.7.5 ARCH=x86_64
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --custom-apprun "$root/packaging/linux/AppRun" --output appimage
chmod +x "$OUTPUT"
QT_QPA_PLATFORM=offscreen "$OUTPUT" --appimage-extract-and-run --check-runtime --data-dir "$build/runtime-check-data"
QT_QPA_PLATFORM=offscreen "$OUTPUT" --appimage-extract-and-run --check-preview-runtime --data-dir "$build/runtime-check-data"
cp "$root/docs/DESKTOP-LINUX.md" "$root/dist/TinyPTZ-v0.7.5-Linux-README.md"
sha256sum "$OUTPUT"
