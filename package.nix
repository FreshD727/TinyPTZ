{
  stdenv,
  lib,
  fetchFromGitHub,
  cmake,
  pkg-config,
  wrapQtAppsHook,
  qtbase,
  qtmultimedia,
  obs-studio,

  buildDesktop ? true,
  buildObsPlugin ? false,
}:
stdenv.mkDerivation rec {
  pname = "tinyptz";
  version = "1.0.1";

  src = ./.;

  cmakeFlags = [
    (lib.cmakeBool "BUILD_OBS_PLUGIN" buildObsPlugin)
    (lib.cmakeBool "BUILD_QT_DESKTOP" buildDesktop)
  ];

  nativeBuildInputs = [
    cmake
    pkg-config
    wrapQtAppsHook
  ];
  buildInputs = [
    qtbase
    qtmultimedia
  ]
  ++ lib.optional buildObsPlugin obs-studio;

  # install paths are hardcoded
  postInstall =
    lib.optional buildObsPlugin ''
      readlink -f .
      ls -alR
      mkdir -p $out/lib/obs-plugins
      mkdir -p $out/share/obs/obs-plugins
      mv $out/bin/64bit/tiny-ptz-obs.so $out/lib/obs-plugins/obs-tinyptz.so
      mv $out/data $out/share/obs/obs-plugins/tinyptz
      rmdir $out/bin/64bit $out/bin
      rm $out/README.md $out/LICENSE.txt
    ''
    ++ lib.optional buildDesktop ''
      substituteInPlace $out/share/applications/tinyptz.desktop \
        --replace-fail "Exec=TinyPTZ" "Exec=$out/bin/TinyPTZ"
    '';

  meta = {
    description = "OBSBOT PTZ Camera control with OBS Plugin Integration";
    homepage = "https://github.com/FreshD727/TinyPTZ";
    license = lib.licenses.gpl2Plus;
    platforms = lib.platforms.linux;
  }
  // (if buildDesktop then { mainProgram = "TinyPTZ"; } else { });
}
