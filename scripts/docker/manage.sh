#!/usr/bin/env bash
# Run on the Docker host, from any directory; affects only this Compose project.
set -euo pipefail
umask 077
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo"
command=${1:-}
[[ $# == 2 && ( $command == backup || $command == restore ) ]] || {
    echo 'Aufruf: bash scripts/docker/manage.sh backup DATEI.tar.gz | restore DATEI.tar.gz' >&2; exit 1;
}
archive=$2
was_running=$(docker compose ps --status running --quiet mumble-funk)
resume() { if [[ -n $was_running ]]; then docker compose start mumble-funk >&2; fi; }
trap resume EXIT
docker compose stop --timeout 30 mumble-funk >&2
if [[ $command == backup ]]; then
    [[ ! -e $archive ]] || { echo 'Zieldatei existiert bereits; keine Sicherung überschrieben.' >&2; exit 1; }
    parent=$(dirname -- "$archive")
    temporary=$(mktemp "$parent/.funk-backup.XXXXXX")
    if docker compose run --rm --no-deps -T mumble-funk backup > "$temporary"; then
        # Hard-link publication refuses a concurrently created destination too.
        ln -- "$temporary" "$archive"
        rm -- "$temporary"
        echo "Konsistente Sicherung erstellt: $archive" >&2
    else
        rm -f -- "$temporary"
        echo 'Sicherung fehlgeschlagen; keine fertige Archivdatei veröffentlicht.' >&2
        exit 1
    fi
else
    [[ -f $archive ]] || { echo 'Sicherungsdatei fehlt.' >&2; exit 1; }
    docker compose run --rm --no-deps -T mumble-funk restore < "$archive"
fi
