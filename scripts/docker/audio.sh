#!/usr/bin/env bash
# Uses only the existing Compose service and volumes. No .env or port migration.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo"
command=${1:-status}
case "$command" in
    status)
        [[ $# -le 1 ]] || { echo 'Aufruf: audio.sh status' >&2; exit 1; }
        docker compose run --rm --no-deps -T mumble-funk audio status
        exit 0 ;;
    enable|disable|reset-busy-tone|enable-squelch|disable-squelch|enable-open|disable-open|enable-close|disable-close|enable-timeout-close|disable-timeout-close|reset-open|reset-close)
        [[ $# == 1 ]] || { echo 'Dieser Befehl benötigt keinen Wert.' >&2; exit 1; } ;;
    volume|open-volume|close-volume)
        if [[ $# != 2 ]] || [[ ! $2 =~ ^[0-9]{1,3}$ ]] || ((10#$2 > 100)); then
            echo 'Lautstärke: 0 bis 100.' >&2; exit 1
        fi ;;
    set-busy-tone|set-open|set-close)
        [[ $# == 2 ]] || { echo 'WAV-Datei fehlt.' >&2; exit 1; }
        [[ -f $2 && ! -L $2 ]] || { echo 'Normale WAV-Datei ohne symbolischen Link benötigt.' >&2; exit 1; } ;;
    *) echo 'Unbekannter Audio-Befehl; siehe docs/funk/squelch-preview.md.' >&2; exit 1 ;;
esac
was_running=$(docker compose ps --status running --quiet mumble-funk)
resume() { if [[ -n $was_running ]]; then docker compose start mumble-funk >&2; fi; }
trap resume EXIT
docker compose stop --timeout 30 mumble-funk >&2
if [[ $command == set-* ]]; then
    # Snapshot a bounded local file. Only this temporary copy is mounted read-only.
    # The manager validates it with the actual server parser before publishing.
    temporary=$(mktemp -d)
    cleanup() { rm -rf -- "$temporary"; resume; }
    trap cleanup EXIT
    python3 - "$2" "$temporary/asset.wav" <<'PY'
import os, stat, sys
fd = os.open(sys.argv[1], os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
try:
    info = os.fstat(fd)
    if not stat.S_ISREG(info.st_mode) or info.st_size > 131072:
        raise SystemExit('Nur normale WAV-Dateien bis 128 KiB erlaubt.')
    data = os.read(fd, 131073)
    if len(data) > 131072:
        raise SystemExit('WAV-Datei zu groß.')
finally:
    os.close(fd)
with open(sys.argv[2], 'xb') as target:
    target.write(data)
os.chmod(sys.argv[2], 0o444)
PY
    chmod 0755 "$temporary"
    docker compose run --rm --no-deps -T -v "$temporary/asset.wav:/tmp/funk-import.wav:ro" mumble-funk audio "$command" /tmp/funk-import.wav
else
    docker compose run --rm --no-deps -T mumble-funk audio "$@"
fi
