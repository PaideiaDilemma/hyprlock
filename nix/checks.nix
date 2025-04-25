inputs: pkgs: let
  flake = inputs.self.packages.${pkgs.stdenv.hostPlatform.system};
  hyprland = inputs.hyprland.packages.${pkgs.stdenv.hostPlatform.system}.hyprland;
  env = {
        "AQ_TRACE" = "1";
        "HYPRLAND_TRACE" = "1";
        "XDG_RUNTIME_DIR" = "/tmp";
        "XDG_CACHE_HOME" = "/tmp";
      #"MESA_EXTENSION_OVERRIDE" = "-EGL_EXT_platform_device -GL_EXT_platform_device -EXT_platform_device"; # Causes hyprland to use GBM backend - nevermind doesn't work
      };
in {
  tests = pkgs.testers.runNixOSTest {
    name = "hyprlock-tests";

    nodes.machine = {pkgs, ...}: {
      environment.systemPackages = with pkgs; [
        flake.hyprlock_tester

        # Programs needed for tests
        pkgs.egl-wayland
      ];

      # Enabled by default for some reason
      services.speechd.enable = false;

      environment.variables = env;

      programs.hyprland = {
        enable = true;
        package = hyprland;
        # We don't need portals in this test, so we don't set portalPackage
      };

      # Hyprland config (runs the tester with exec-once)
      environment.etc."hyprland.conf".source = "${flake.hyprlock_tester}/share/hypr/headless_hyprland.conf";

      # Disable portals
      xdg.portal.enable = pkgs.lib.mkForce false;

      # Autologin root into tty
      services.getty.autologinUser = "alice";

      system.stateVersion = "24.11";

      users.users.alice = {
        isNormalUser = true;
      };

      # Might crash with less
      virtualisation.memorySize = 8192;

      virtualisation.qemu = {
        package = pkgs.lib.mkForce pkgs.qemu_full;
        options = ["-cpu host -vga none -display none -device virtio-gpu"];
      };
    };

    testScript = ''
      # Wait for tty to be up
      machine.wait_for_unit("multi-user.target")

      # Hyprland systeminfo
      _, out = machine.execute("Hyprland --version")
      print(out)

      _, __ = machine.execute("touch /tmp/hyprlock_tester_log")

      # Start Hyprland
      print("Starting Hyprland")
      shell_cmd = "${hyprland}/bin/Hyprland -c /etc/hyprland.conf"
      env_addition = " ".join((${pkgs.lib.concatStringsSep "," (pkgs.lib.mapAttrsToList (k: v: "'--setenv=${k}=\"${v}\"'") env)}))
      systemd_run_cmd = (
        "systemd-run --wait --uid $(id -u alice) --unit=hyprland " +
        env_addition + " --setenv=PATH=$PATH " +
        shell_cmd
      )
      run_cmd = f"su - alice -c '{shell_cmd}'"
      # TODO: is this exit code correct?
      exit_status, _out = machine.execute(run_cmd, timeout=30)
      print(f"Hyprtester exited with {exit_status}")
      _, __ = machine.execute("systemctl kill hyprland")

      # Copy logs to host
      machine.execute('cp "$(find /tmp/hypr -name *.log | head -1)" /tmp/hyprlog')
      machine.copy_from_vm("/tmp/hyprlog")

      machine.copy_from_vm("/tmp/hyprlock_tester_log")

      # Print logs for visibility in CI
      _, out = machine.execute("cat /tmp/hyprlog")
      print(f"Hyprland log:\n{out}")
      _, out = machine.execute("cat /tmp/hyprlock_tester_log")
      print(f"Hyprtester log:\n{out}")

      # TODO: Do we need a clean shutdown? It's a bit slow...
      #machine.shutdown()

      if exit_status != 0:
        raise Exception(f"Exit code non-zero: {exit_status}")
    '';
  };
}
