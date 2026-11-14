{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { nixpkgs, ... }:
    {
      packages =
        (builtins.foldl' (
          acc: system:
          let
            pkgs = import nixpkgs { inherit system; };
          in
          (
            {
              ${system} = rec {
                default = tinyptz;
                tinyptz = pkgs.qt6Packages.callPackage ./package.nix { };
                obs-tinyptz = pkgs.qt6Packages.callPackage ./package.nix {
                  buildDesktop = false;
                  buildObsPlugin = true;
                };
              };
            }
            // acc
          )
        ))
          { }
          nixpkgs.lib.platforms.linux;
    };
}
