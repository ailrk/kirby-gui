{
  description = "Home manager ui";

  inputs = {
      nixpkgs.url     = "github:NixOS/nixpkgs/nixos-24.05";
      flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:  flake-utils.lib.eachDefaultSystem (system:
  let
    pkgs      = import nixpkgs { inherit system; };
    libraries = with pkgs; [
      gtk4.dev
    ] ;
  in rec {
    packages = rec {
        home-manager-ui = pkgs.stdenv.mkDerivation rec {
            version     = "0.0.0";
            name        = "home-manager-ui";
            src         = ./.;
            buildInputs = libraries;
            nativeBuildInputs = with pkgs; [ pkg-config ];
        };

        default = home-manager-ui;
    };

    devShells.default = packages.home-manager-ui.overrideAttrs (prev: {
      buildInputs = with pkgs; prev.buildInputs ++ [
        clang-tools
        gcc
        gnumake
      ];

      LD_LIBRARY_PATH="${pkgs.lib.makeLibraryPath libraries}";
    });
  });
}
