{
  stdenv,
  pkgs,
  nodejs,
  pnpm,
}:
stdenv.mkDerivation rec {
  pname = "millennium-loader";
  version = "git";

  src = ../sdk;
  pnpmDeps = pkgs.pnpm.fetchDeps {
    inherit src version pname;
    hash = "sha256-sPZsLlaeCqLZNm4i8yxeIxgPdQKmseKeJiErzngkZoU=";
  };

  nativeBuildInputs = [
    pnpm.configHook
    nodejs
  ];

  buildPhase = ''
    runHook preBuild
    pnpm -C typescript-packages/loader run build
  '';

  installPhase = ''
    mkdir $out
    cp -r typescript-packages/loader/build/* $out
  '';
}
