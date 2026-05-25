{
  description = "ndsort — generic C++20 non-dominated sorting library";

  inputs = {
    flake-parts.url = "github:hercules-ci/flake-parts";
    nixpkgs.url = "github:nixos/nixpkgs/master";
    foolnotion.url = "github:foolnotion/nur-pkg";
    foolnotion.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs =
    inputs@{ self, flake-parts, nixpkgs, foolnotion }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [
        "x86_64-linux"
        "x86_64-darwin"
        "aarch64-linux"
        "aarch64-darwin"
      ];

      flake.overlays.default = final: prev: {
        ndsort = self.packages.${prev.stdenv.hostPlatform.system}.default;
      };

      perSystem = { system, ... }:
        let
          pkgs = import inputs.nixpkgs {
            inherit system;
            overlays = [ foolnotion.overlay ];
          };
          inherit (pkgs.llvmPackages_21) stdenv;

          isX86 = pkgs.stdenv.hostPlatform.isx86_64;

          buildInputs = with pkgs; [ eve ];
          testInputs  = with pkgs; [ catch2_3 nanobench ];

          ndsort = stdenv.mkDerivation {
            name = "ndsort";
            src = ./.;
            cmakeFlags = [
              "-DBUILD_SHARED_LIBS=YES"
              "-DCMAKE_BUILD_TYPE=Release"
            ] ++ pkgs.lib.optionals isX86 [
              "-DCMAKE_CXX_FLAGS=-march=x86-64-v3"
            ];
            nativeBuildInputs = with pkgs; [ cmake ];
            inherit buildInputs;
          };
        in
        {
          packages.default = ndsort;

          devShells.default = stdenv.mkDerivation {
            name = "ndsort-dev";
            nativeBuildInputs = with pkgs; [
              cmake
              clang-tools
              cppcheck
              ninja
            ];
            buildInputs = buildInputs ++ testInputs;
          };
        };
    };
}
