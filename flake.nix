{
  # Keep this flake at the repository root: Nix and the NixOS CI job resolve
  # packages through the project's ".#packages.<system>.default" reference.
  description = "GenyCaster native Modern C++/Qt package for NixOS";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAll = nixpkgs.lib.genAttrs systems;
      packageFor = system:
        let
          pkgs = import nixpkgs { inherit system; };
          qt = pkgs.qt6Packages;
        in
        pkgs.gcc16Stdenv.mkDerivation {
          pname = "genycaster";
          version = "1.0.0";
          src = self;
          nativeBuildInputs = [
            pkgs.cmake pkgs.ninja pkgs.pkg-config pkgs.python3
            qt.wrapQtAppsHook qt.qttools
          ];
          buildInputs = [
            qt.qtbase qt.qtdeclarative qt.qtshadertools
            qt.qtmultimedia qt.qtsvg qt.qtlottie qt.qtkeychain
            pkgs.boost pkgs.sqlite pkgs.openssl pkgs.curl pkgs.libsecret
            pkgs.libGL pkgs.libxkbcommon pkgs.fontconfig pkgs.freetype
          ];
          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DFARCASTER_BUILD_TESTS=OFF"
            "-DFARCASTER_BUILD_UI=ON"
            "-DFARCASTER_REQUIRE_SECURE_SIGNER_VAULT=ON"
            "-DGENYCASTER_FETCH_QTKEYCHAIN=OFF"
          ];
          enableParallelBuilding = true;
          installPhase = ''
            runHook preInstall
            install -Dm755 bin/Genycaster "$out/bin/Genycaster"
            runHook postInstall
          '';
          meta = with pkgs.lib; {
            description = "Native local-first decentralized social client";
            platforms = platforms.linux;
            mainProgram = "Genycaster";
          };
        };
    in {
      packages = forAll (system: {
        default = packageFor system;
        genycaster = packageFor system;
      });
    };
}
