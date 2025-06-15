{
  stdenv_32bit,
  pkgsi686Linux,
  steam,
  replaceVars,
  cmake,
  ninja,
  callPackage,
  lib,
}:
let
  loader = callPackage ./loader.nix { };
in
stdenv_32bit.mkDerivation {
  pname = "millennium";
  version = "git";

  src = ../.;
  patches = [
    ./disableCli.patch
    (replaceVars ./startScript.patch {
      inherit steam;
      OUT = null;
    })
  ];

  buildInputs = [
    pkgsi686Linux.python311
    pkgsi686Linux.curl
    pkgsi686Linux.openssl
    cmake
    ninja
    loader
  ];

  ####
  #
  # TODO: remove those substituteInPlace in favour of git patches
  #
  ####
  configurePhase = ''
    cmake -G Ninja
    substituteInPlace scripts/posix/start.sh \
      --replace '@OUT@' "$out"
  '';
  buildPhase = ''
    cmake --build .
  '';
  installPhase = ''
    mkdir -p $out/bin $out/lib/millennium
    cp libmillennium_x86.so $out/lib/millennium
    cp scripts/posix/start.sh $out/bin/millennium
  '';
  NIX_CFLAGS_COMPILE = [
    "-isystem ${pkgsi686Linux.python311Full}/include/${pkgsi686Linux.python311Full.libPrefix}"
  ];
  NIX_LDFLAGS = [ "-l${pkgsi686Linux.python311Full.libPrefix}" ];

  meta = with lib; {
    maintainers = with maintainers; [ Sk7Str1p3 ];
  };
}
