{
  description = "Kirby gui";

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
      pcre2
    ];
  in rec {
    defaultPackage = packages.kbgui;
    defaultApp     = apps.kbgui;
    packages = {
      kbgui = pkgs.stdenv.mkDerivation {
        version           = "0.0.0";
        name              = "kbgui";
        src               = ./.;
        buildInputs       = libraries;
        nativeBuildInputs = with pkgs; [ pkg-config ];
        buildPhase = ''
          NIX_CFLAGS_COMPILE="$(pkg-config --cflags --libs gtk4 glib-2.0 libpcre2-8) $NIX_CFLAGS_COMPILE"
          make
        '';
        # hardeningDisable = [ "all" ];
      };
    };

    apps.kbgui = {
      type = "app";
      program = "${defaultPackage}/bin/kbgui";
    };

    devShells.default = packages.kbgui.overrideAttrs (prev: {
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
          NIX_CFLAGS_COMPILE="$(pkg-config --cflags --libs gtk4 glib-2.0 libpcre2-8) $NIX_CFLAGS_COMPILE"
      '';
    });
  });
}
