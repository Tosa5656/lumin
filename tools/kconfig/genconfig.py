#!/usr/bin/env python3
"""
Wrapper for Kconfiglib operations:
  genconfig.py menuconfig   - run curses menuconfig
  genconfig.py config       - generate .config from default values
  genconfig.py autoconf     - generate kernel/include/generated/autoconf.h
  genconfig.py defconfig    - save minimal defconfig
"""
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))

def main():
    if len(sys.argv) < 2:
        print("Usage: genconfig.py <command>", file=sys.stderr)
        sys.exit(1)

    cmd = sys.argv[1]
    kconfig_path = os.path.join(os.getcwd(), "Kconfig")

    if cmd == "menuconfig":
        import menuconfig
        from kconfiglib import Kconfig
        kconf = Kconfig(kconfig_path)
        kconf.load_config()
        menuconfig.menuconfig(kconf)

    elif cmd == "config":
        from kconfiglib import Kconfig
        kconf = Kconfig(kconfig_path)
        kconf.load_config()
        kconf.write_config(".config")
        print("Configuration written to .config")

    elif cmd == "autoconf":
        from kconfiglib import Kconfig
        kconf = Kconfig(kconfig_path)
        kconf.load_config()
        os.makedirs("kernel/include/generated", exist_ok=True)
        kconf.write_autoconf("kernel/include/generated/autoconf.h")
        print("Generated kernel/include/generated/autoconf.h")

    elif cmd == "defconfig":
        from kconfiglib import Kconfig
        kconf = Kconfig(kconfig_path)
        kconf.load_config()
        kconf.write_min_config("defconfig")
        print("Minimal configuration saved to defconfig")

    else:
        print(f"Unknown command: {cmd}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
