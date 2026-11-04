#!/bin/bash
# Lingmo OS OOBE launcher
#   oobe.sh setup    - pre-DM: write the first-boot autologin configuration
#   oobe.sh          - session: launch lingmo-oobe
#
# The first boot logs in automatically as the account the installation
# created, and runs the OOBE session. The account is kept after the wizard: it
# is what re-enters the OOBE if the first boot is interrupted, and the wizard
# only adds the account the owner will use.

case "${1:-run}" in
    setup)
        # The account the installation created: the first regular account.
        original_user="$(awk -F: '$3 >= 1000 && $3 < 60000 && $7 !~ /(nologin|false)$/ { print $1; exit }' /etc/passwd)"
        if [ -z "${original_user}" ]; then
            # No account at all (installation without a users page): create the
            # account the wizard is entered with, so the first boot has a login.
            original_user="lingmo"
            useradd -m -G sudo,audio,cdrom,dip,floppy,video,plugdev,netdev,bluetooth \
                -s /bin/bash "${original_user}" 2>/dev/null || true
        fi

        # The lingmo account copied from the live squashfs may have a locked
        # password or stale shadow aging fields (live-config sets these at
        # boot via chpasswd, but on the installed system that hook never runs).
        # passwd -u removes the '!' prefix; chage resets the last-change date
        # to today and clears the account-expiry date, so pam_acct_mgmt does
        # not return PAM_NEW_AUTHTOK_REQD or PAM_ACCT_EXPIRED and refuse login.
        passwd -u "${original_user}" 2>/dev/null || true
        chage -d "$(date +%Y-%m-%d)" -E -1 "${original_user}" 2>/dev/null || true

        # lingmo-dm names a session by its desktop file name without suffix.
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
