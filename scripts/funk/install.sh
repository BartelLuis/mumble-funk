#!/usr/bin/env bash
# Only deploys our instance. Never runs apt or cmake --install.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
templates="$repo/scripts/funk/ubuntu"
# shellcheck source=scripts/funk/platform.sh
source "$repo/scripts/funk/platform.sh"
root=
if [[ ${1:-} == --stage && $# == 3 ]]; then
    # Test-only: generate a directory tree without users, services or host writes.
    root=$(realpath -m -- "$2")
    [[ $root != / ]] || { echo 'Stage darf nicht / sein.' >&2; exit 1; }
    binary=$(realpath -- "$3")
elif [[ $# == 0 ]]; then
    [[ $EUID == 0 ]] || { echo 'Bitte mit sudo bash scripts/funk/install.sh starten.' >&2; exit 1; }
    # OS-provided file, not part of the repository.
    # shellcheck source=/dev/null
    . /etc/os-release
    funk_require_supported_os "${ID:-}" "${VERSION_ID:-}" || exit 1
    exec 9>/run/lock/mumble-funk-install.lock
    flock -n 9 || { echo 'Eine andere Funkserver-Installation läuft bereits.' >&2; exit 1; }
    binary="$repo/build/funk/mumble-server"
else
    echo 'Aufruf: install.sh [--stage TESTVERZEICHNIS SERVERBINARY]' >&2; exit 1
fi
[[ -x $binary ]] || { echo "Serverbinary fehlt: $binary" >&2; exit 1; }
version=$("$binary" --version 2>&1)
[[ $version == *1.5.939* ]] || { echo 'Erwartet wird Mumble 1.5.939.' >&2; exit 1; }
release=$(sha256sum -- "$binary" | cut -d ' ' -f 1)
base="$root/opt/mumble-funk"
if [[ -z $root ]]; then
    if ! id mumble-funk >/dev/null 2>&1; then
        useradd --system --user-group --home-dir /var/lib/mumble-funk --shell /usr/sbin/nologin mumble-funk
    fi
fi
install -d -m 0755 "$base/releases/$release" "$root/etc/systemd/system" "$root/etc/logrotate.d"
install -d -m 0750 "$root/etc/mumble-funk" "$root/var/lib/mumble-funk" "$root/var/log/mumble-funk"
if [[ ! -e $base/releases/$release/mumble-server ]]; then
    install -m 0755 "$binary" "$base/releases/$release/mumble-server"
else
    [[ $(sha256sum "$base/releases/$release/mumble-server" | cut -d ' ' -f 1) == "$release" ]] || {
        echo 'Vorhandene Release-Datei stimmt nicht mit ihrer Prüfsumme überein.' >&2; exit 1;
    }
fi
if [[ ! -e $root/etc/mumble-funk/server.ini ]]; then
    password=$(openssl rand -hex 16)
    sed "s/@PASSWORD@/$password/" "$templates/server.ini" > "$root/etc/mumble-funk/server.ini"
    chmod 0640 "$root/etc/mumble-funk/server.ini"
fi
install -m 0644 "$templates/mumble-funk.service" "$root/etc/systemd/system/mumble-funk.service"
install -m 0644 "$templates/logrotate" "$root/etc/logrotate.d/mumble-funk"
if [[ -z $root ]]; then
    chown root:mumble-funk /etc/mumble-funk /etc/mumble-funk/server.ini
    chown mumble-funk:mumble-funk /var/lib/mumble-funk /var/log/mumble-funk
    touch /var/log/mumble-funk/server.log
    chown mumble-funk:mumble-funk /var/log/mumble-funk/server.log
    chmod 0640 /var/log/mumble-funk/server.log
    # Stop only our instance; copy the entire SQLite state while it is stopped.
    systemctl daemon-reload
    systemctl stop mumble-funk.service
    if [[ -f /var/lib/mumble-funk/server.sqlite ]]; then
        backup="/var/backups/mumble-funk/$(date -u +%Y%m%dT%H%M%S)-$$"
        install -d -m 0700 "$backup"
        cp -a /var/lib/mumble-funk "$backup/state"
        cp -a /etc/mumble-funk "$backup/config"
        echo "Sicherung: $backup"
    fi
fi
if [[ -L $base/current ]]; then
    previous=$(readlink -- "$base/current")
    if [[ $previous != "releases/$release" ]]; then
        ln -sfn -- "$previous" "$base/previous"
    fi
fi
ln -sfn -- "releases/$release" "$base/current.new"
mv -Tf -- "$base/current.new" "$base/current"
if [[ -z $root ]]; then
    systemctl daemon-reload
    systemctl enable mumble-funk.service
    systemctl start mumble-funk.service
    systemctl --no-pager --full status mumble-funk.service
    echo 'Installation abgeschlossen. Zugangspasswort: sudo cat /etc/mumble-funk/server.ini'
else
    echo "Testinstallation erstellt: $root (kein Dienst gestartet)"
fi
