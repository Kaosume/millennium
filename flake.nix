{
  description = ''
    Millennium - an open-source low-code modding framework to create, 
    manage and use themes/plugins for the desktop Steam Client
  '';

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    self.submodules = true; # Requires *Nix* >= 2.27
  };

  outputs =
    { nixpkgs, ... }:
    let
      pkgs = import nixpkgs {
        system = "x86_64-linux";
        config.allowUnfree = true;
      };
    in
    {
      # TODO: move shell to separate file to
      #       let users without flakes use it too
      devShells."x86_64-linux".default = pkgs.mkShellNoCC {
        stdenv = pkgs.stdenv_32bit;
        name = "Millennium";
        packages = with pkgs; [
          nixd
          nixfmt-rfc-style
        ];
        buildInputs =
          with pkgs;
          (
            [
              cli11
            ]
            ++ (with pkgsi686Linux; [
              python311
              curl
            ])
          );
        nativeBuildInputs = with pkgs; [
          cmake
          ninja
          pnpm
        ];
        NIX_CFLAGS_COMPILE = [
          "-isystem ${pkgs.pkgsi686Linux.python311}/include/${pkgs.pkgsi686Linux.python311.libPrefix}"
        ];
        NIX_LDFLAGS = [ "-l${pkgs.pkgsi686Linux.python311.libPrefix}" ];
      };

      packages."x86_64-linux" = {
        millennium = pkgs.callPackage ./nix/millennium.nix { };
        loader = pkgs.callPackage ./nix/loader.nix { };
        assets = pkgs.callPackage ./nix/assets.nix { };
      };
    };
}
