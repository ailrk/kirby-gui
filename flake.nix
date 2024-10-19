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
      gtk4
      glib
    ];
  in rec {
    defaultPackage = packages.hmgui;
    defaultApp     = apps.hmgui;
    packages = {
      hmgui = pkgs.stdenv.mkDerivation {
        version           = "0.0.0";
        name              = "hmgui";
        src               = ./.;
        buildInputs       = libraries;
        nativeBuildInputs = with pkgs; [ pkg-config ];
        buildPhase = ''
          NIX_CFLAGS_COMPILE="$(pkg-config --cflags --libs gtk4 glib-2.0) $NIX_CFLAGS_COMPILE"
          make
        '';
      };
    };

    apps.hmgui = {
      type = "app";
      program = "${defaultPackage}/bin/hmgui";
    };

    devShells.default = packages.hmgui.overrideAttrs (prev: {
      buildInputs = with pkgs; prev.buildInputs ++ [
        clang-tools
        gcc
        gnumake
        # for lsp to work
        (runCommand "cDependencies" {} ''
          mkdir -p $out/include
          cp -r ${gtk4.dev}/include/gtk-4.0/* $out/include
          cp -r ${glib.dev}/include/glib-2.0/* $out/include
        '')
      ];

      shellHook = ''
          NIX_CFLAGS_COMPILE="$(pkg-config --cflags --libs gtk4 glib-2.0) $NIX_CFLAGS_COMPILE"
      '';
    });
  });
}
