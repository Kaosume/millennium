{
  stdenv_32bit,
  pkgsi686Linux,
  cli11,
  steam,
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
  #patches = [
  #  ./disableCli.patch
  #];

  buildInputs = [
    pkgsi686Linux.python311
    pkgsi686Linux.curl
    cmake
    ninja
    cli11
    loader
  ];

  ####
  #
  # TODO: remove those substituteInPlace in favour of git patches
  #
  ####
  configurePhase = ''
    cmake -G Ninja 
  '';
  buildPhase = ''
    cmake --build . --config Release
  '';
  installPhase = ''
    mkdir -p $out/bin $out/lib/millennium
    cp cli/millennium $out/bin/millennium-cli
    cp libmillennium_x86.so $out/lib/millennium
    cp $src/scripts/posix/start.sh $out/bin/millennium
  '';
  postFixup = ''
    substituteInPlace $out/bin/millennium \
      --replace '/usr/lib/millennium' "$out/lib" \
      --replace '/usr/lib/steam/bin_steam.sh' '${steam}/bin/steam' \
      --replace '/home/shadow/dev/Millennium/build' "$out/lib/millennium"
  '';
  NIX_CFLAGS_COMPILE = [
    "-isystem ${pkgsi686Linux.python311Full}/include/${pkgsi686Linux.python311Full.libPrefix}"
  ];
  NIX_LDFLAGS = [ "-l${pkgsi686Linux.python311Full.libPrefix}" ];

  meta = with lib; {
    maintainers = with maintainers; [ Sk7Str1p3 ];
  };
}
