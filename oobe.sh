#!/bin/bash
# Lingmo OS OOBE launcher
#   oobe.sh setup    - pre-DM: write SDDM first-boot autologin config
#   oobe.sh          - session: launch lingmo-oobe
#
# The OOBE runs as the system's original user: the account the installation
# left in place. That account is kept after the wizard (it is what re-enters
# the OOBE if the first boot is interrupted and the account the wizard creates
# is only added next to it).

case "${1:-run}" in
    setup)
        # The first regular account, in case several exist.
        original_user="$(awk -F: '$3 >= 1000 && $3 < 60000 && $7 !~ /(nologin|false)$/ { print $1; exit }' /etc/passwd)"
        if [ -z "${original_user}" ]; then
            # No account at all (installation without a user page): create the
            # account the wizard is entered with, so the first boot has a login.
            original_user="lingmo"
            useradd -m -G sudo,audio,cdrom,dip,floppy,video,plugdev,netdev,bluetooth \
                -s /bin/bash "${original_user}" 2>/dev/null || true
        fi

        mkdir -p /etc/sddm.conf.d
        cat > /etc/sddm.conf.d/lingmo-firstboot.conf << EOF
[Autologin]
User=${original_user}
Session=lingmo-oobe.desktop
EOF
        ;;
    run|*)
        exec /usr/bin/lingmo-oobe
        ;;
esac
