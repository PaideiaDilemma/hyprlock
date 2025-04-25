{
  cmake,
  egl-wayland,
  hyprland-protocols,
  hyprlock,
  hyprwayland-scanner,
  lib,
  pkg-config,
  stdenv,
  stdenvAdapters,
  wayland-scanner,
  version ? "git",
}: let
  inherit (lib.lists) flatten foldl';
  inherit (lib.sources) cleanSourceWith cleanSource;
  inherit (lib.strings) hasSuffix;

  adapters = flatten [
    stdenvAdapters.useMoldLinker
    stdenvAdapters.keepDebugInfo
];

  customStdenv = foldl' (acc: adapter: adapter acc) stdenv adapters;
in
  customStdenv.mkDerivation (finalAttrs: {
    pname = "hyprlock_tester";
    inherit version;

    #src = ../tests;
    src  = cleanSourceWith {
      filter = name: _type: let
        baseName = baseNameOf (toString name);
      in
        ! (hasSuffix ".nix" baseName);
      src = cleanSource ../tests;
    };

    nativeBuildInputs = [
      cmake
      hyprland-protocols
      hyprwayland-scanner
      pkg-config
      wayland-scanner
      egl-wayland
    ];

    buildInputs = hyprlock.buildInputs;

    cmakeBuildType = "Debug";

    meta = {
      homepage = "https://github.com/hyprwm/hyprlock";
      description = "Hyprlock testing utility";
      license = lib.licenses.bsd3;
      platforms = hyprlock.meta.platforms;
      mainProgram = "hyprlock_tester";
    };
  })
