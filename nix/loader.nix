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
    #TODO: automatic hash update
    hash = "sha256-LofHepVz6CjbAXkUwwNFVzlbmPq+g/gJvkBka9I/gHo=";
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
