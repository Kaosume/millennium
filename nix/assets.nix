{stdenv,}:
stdenv.mkDerivation {
  pname = "millennium-assets";
  version = "git";

  src = ../assets;

  installPhase = ''
    mkdir $out
    cp -r . $out
  '';
}