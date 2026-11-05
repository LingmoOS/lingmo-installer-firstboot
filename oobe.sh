#!/bin/bash
# Lingmo OS OOBE launcher
#   oobe.sh setup    - pre-DM: write the first-boot autologin configuration
#   oobe.sh          - session: launch lingmo-oobe
#
# The installer now creates the user account via Calamares users module and
# writes /etc/lingmo-dm.conf with Session=lingmo-oobe. This script only
# unlocks shadow aging and re-confirms the autologin config so the first
# boot reaches the OOBE session.

case "${1:-run}" in
    setup)
        # The account the installer created: the first regular account.
        original_user="$(awk -F: '$3 >= 1000 && $3 < 60000 && $7 !~ /(nologin|false)$/ { print $1; exit }' /etc/passwd)"
        if [ -z "${original_user}" ]; then
            # Safety fallback: no user found (e.g. live ISO or install without
            # users page). Create a temporary lingmo account so the first boot
            # has a login to enter the OOBE.
            original_user="lingmo"
            useradd -m -G sudo,audio,cdrom,dip,floppy,video,plugdev,netdev,bluetooth \
                -s /bin/bash "${original_user}" 2>/dev/null || true
        fi

        # Unlock shadow aging so pam_acct_mgmt does not return
        # PAM_NEW_AUTHTOK_REQD or PAM_ACCT_EXPIRED and refuse login.
        passwd -u "${original_user}" 2>/dev/null || true
        chage -d "$(date +%Y-%m-%d)" -E -1 "${original_user}" 2>/dev/null || true

        # Write or overwrite /etc/lingmo-dm.conf to ensure the OOBE session
        # is used for first boot. The installer's displaymanager module already
        # wrote this file; we overwrite to handle the fallback path above and
        # to guarantee the correct session name.
        cat > /etc/lingmo-dm.conf << EOF
[Autologin]
User=${original_user}
Session=lingmo-oobe
EOF
        ;;
    run|*)
        exec /usr/bin/lingmo-oobe
        ;;
esac
