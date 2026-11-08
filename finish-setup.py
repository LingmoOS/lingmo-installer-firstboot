#!/usr/bin/python3
"""Complete OOBE using the account and login preference recorded at installation."""
import configparser
import os
from pathlib import Path
import pwd
import re
import subprocess
import sys


def finish(hostname="", root=Path("/"), run=subprocess.run, lookup=pwd.getpwnam):
    state = configparser.ConfigParser()
    if not state.read(root / "etc/lingmo-oobe-account.conf"):
        raise RuntimeError("Installed account record is missing")
    user = state.get("Account", "User")
    if lookup(user).pw_uid < 1000:
        raise RuntimeError("OOBE requires the installed regular account")
    autologin = state.getboolean("Account", "Autologin")
    if hostname:
        if not re.fullmatch(r"[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?", hostname):
            raise ValueError("Invalid hostname")
        run(["hostnamectl", "set-hostname", hostname], check=True)
    path = root / "etc/lingmo-dm.conf"
    config = configparser.ConfigParser()
    config.optionxform = str
    config.read(path)
    if not config.has_section("Autologin"):
        config.add_section("Autologin")
    config.set("Autologin", "User", user if autologin else "")
    config.set("Autologin", "Session", "lingmo-wayland")
    temporary = path.with_suffix(".conf.tmp")
    with open(temporary, "w") as stream:
        config.write(stream)
    os.replace(temporary, path)
    (root / "etc/.firstboot-complete").touch()


if __name__ == "__main__":
    try:
        finish(sys.argv[1] if len(sys.argv) > 1 else "")
    except Exception as error:
        print("OOBE setup failed: {}".format(error), file=sys.stderr)
        sys.exit(1)
