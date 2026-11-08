#!/bin/bash
# Use the installed account; never create or unlock a fallback user.
set -euo pipefail
case "${1:-run}" in
    setup)
        state=/etc/lingmo-oobe-account.conf
        if [ ! -r "$state" ]; then
            echo "OOBE: installed account record is missing" >&2
            exit 1
        fi
        original_user="$(sed -n 's/^[[:space:]]*User[[:space:]]*=[[:space:]]*//p' "$state")"
        if [ -z "$original_user" ] || ! getent passwd "$original_user" >/dev/null; then
            echo "OOBE: installed account is missing" >&2
            exit 1
        fi
        python3 - "$original_user" <<'PY'
import configparser
import sys
path = "/etc/lingmo-dm.conf"
config = configparser.ConfigParser()
config.optionxform = str
config.read(path)
if not config.has_section("Autologin"):
    config.add_section("Autologin")
config.set("Autologin", "User", sys.argv[1])
config.set("Autologin", "Session", "lingmo-oobe")
with open(path, "w") as stream:
    config.write(stream)
PY
        ;;
    run) exec /usr/bin/lingmo-oobe ;;
    *) echo "usage: $0 [setup|run]" >&2; exit 2 ;;
esac
