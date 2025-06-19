{
  stdenv_32bit,
  pkgsi686Linux,
  steam,
  replaceVars,
  cmake,
  ninja,
  nodejs,
  pnpm,
  lib,

  #brotli,
  # krb5,
  # libidn2,
  # libnghttp2,
  # libpsl,
  # libssh2,
  # zlib,
  # zstd,

  # keepBuildTree
}:
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

  nativeBuildInputs = [
    cmake
    ninja
    pnpm
    nodejs
    # keepBuildTree
  ];

  buildInputs = [
    # brotli
    # krb5
    # libidn2
    # libnghttp2
    # libpsl
    # libssh2
    # zlib
    # zstd

    pkgsi686Linux.curl
    pkgsi686Linux.python311
    pkgsi686Linux.openssl
  ];

  ####
  #
  # TODO: remove those substituteInPlace in favour of git patches
  #
  ####
  configurePhase = ''
    cmake -G "Ninja" -B build -DCMAKE_BUILD_TYPE=Release
    substituteInPlace scripts/posix/start.sh \
      --replace '@OUT@' "$out"
  '';
  buildPhase = ''
    # cd ./assets
    # npm run build
    cmake --build build --config Release
  '';
  installPhase = ''
    mkdir -p $out/bin $out/lib/millennium $out/share
    # cp -r ./assets $out/share/millennium
    cp build/libmillennium_x86.so $out/lib/millennium
    cp scripts/posix/start.sh $out/bin/steam-millennium
  '';
  NIX_CFLAGS_COMPILE = [
    "-isystem ${pkgsi686Linux.python311Full}/include/${pkgsi686Linux.python311Full.libPrefix}"
  ];
  NIX_LDFLAGS = [ "-l${pkgsi686Linux.python311Full.libPrefix}" ];

  meta = with lib; {
    maintainers = with maintainers; [ Sk7Str1p3 ];
  };
}
