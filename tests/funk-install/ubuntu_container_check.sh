#!/usr/bin/env bash
# Disposable Ubuntu only. Needs SYS_PTRACE in its private PID namespace for ss process attribution.
set -euo pipefail
umask 077
[[ $EUID == 0 && -f /.dockerenv && -d /input/.git ]] || {
    echo 'Nur in einem frischen Ubuntu-Docker-Testcontainer mit /input verwenden.' >&2; exit 1;
}
# shellcheck source=/dev/null
. /etc/os-release
export http_proxy=${HTTP_PROXY:-}
export https_proxy=${HTTPS_PROXY:-}
export GIT_SSL_CAINFO=/run/proxy-ca.pem
export SSL_CERT_FILE=/run/proxy-ca.pem
apt-get update
apt-get install -y --no-install-recommends git sudo ca-certificates python3 shellcheck
mkdir -p /work/mumble-funk
cp -a /input/. /work/mumble-funk/
if ! id setup-tester >/dev/null 2>&1; then
    useradd --create-home --shell /bin/bash setup-tester
fi
chown -R setup-tester:setup-tester /work/mumble-funk
printf 'setup-tester ALL=(ALL) NOPASSWD: ALL\n' > /etc/sudoers.d/funk-setup-test
printf 'Defaults env_keep += "http_proxy https_proxy HTTP_PROXY HTTPS_PROXY SSL_CERT_FILE GIT_SSL_CAINFO"\n' >> /etc/sudoers.d/funk-setup-test
chmod 0440 /etc/sudoers.d/funk-setup-test
if [[ ${1:-} == --built ]]; then
    # Continue after a completed build in an isolated snapshot; preserve prior generated data.
    [[ -f /tmp/funk-first-setup.log && -x /work/mumble-funk/build/funk/mumble-server ]]
    cp /tmp/funk-first-setup.log /tmp/funk-original-build.log
    prior=$(mktemp -d /tmp/funk-prior.XXXXXX)
    for entry in /etc/mumble-funk /var/lib/mumble-funk /var/log/mumble-funk /opt/mumble-funk \
        /etc/systemd/system/mumble-funk.service /etc/logrotate.d/mumble-funk; do
        if [[ -e $entry ]]; then mv -- "$entry" "$prior/$(basename "${entry%/*}")-$(basename "$entry")"; fi
    done
    printf '{"pid":0,"enabled":false}\n' > /tmp/funk-service-state.json
    chmod 0644 /tmp/funk-service-state.json
fi
install -m 0755 /work/mumble-funk/tests/funk-install/mock_systemctl.py /usr/local/bin/systemctl
cd /work/mumble-funk
shellcheck scripts/funk/*.sh tests/funk-install/ubuntu_container_check.sh
# A real foreign TCP+UDP listener must survive a rejected fresh setup.
rm -f /tmp/funk-port-holder-ready
python3 - <<'PY'
import subprocess
from pathlib import Path
program = '''import socket,time
from pathlib import Path
a=socket.socket(); a.bind(('0.0.0.0',64739)); a.listen()
b=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); b.bind(('0.0.0.0',64739))
Path('/tmp/funk-port-holder-ready').touch()
time.sleep(300)
'''
process=subprocess.Popen(['python3','-c',program], start_new_session=True)
Path('/tmp/funk-port-holder-pid').write_text(str(process.pid))
PY
for _attempt in {1..100}; do
    [[ -f /tmp/funk-port-holder-ready ]] && break
    sleep .05
done
if runuser -u setup-tester -- bash scripts/funk/setup.sh > /tmp/funk-occupied-setup.log 2>&1; then
    echo 'FEHLER: belegter Port wurde nicht abgewiesen'; exit 1
fi
grep -q 'Port 64739 ist bereits' /tmp/funk-occupied-setup.log
[[ ! -e /etc/mumble-funk/server.ini ]]
holder_pid=$(cat /tmp/funk-port-holder-pid)
kill -0 "$holder_pid"
kill "$holder_pid"
for _attempt in {1..100}; do
    [[ ! -e /proc/$holder_pid ]] && break
    sleep .05
done
# Real packages, build, all tests and installation. systemctl is explicitly simulated.
runuser -u setup-tester -- env GIT_SSL_CAINFO=/run/proxy-ca.pem SSL_CERT_FILE=/run/proxy-ca.pem \
    FUNK_BUILD_JOBS=1 bash scripts/funk/setup.sh > /tmp/funk-first-setup.log 2>&1 || {
    tail -n 100 /tmp/funk-first-setup.log; exit 1;
}
grep -q 'INSTALLATION ERFOLGREICH' /tmp/funk-first-setup.log
grep -q '^simplex=true$' /etc/mumble-funk/server.ini
grep -q '^simplexretrytimeout=500$' /etc/mumble-funk/server.ini
/usr/local/sbin/mumble-funk-info > /tmp/funk-info.log
systemd-analyze verify /etc/systemd/system/mumble-funk.service
logrotate --debug /etc/logrotate.d/mumble-funk
runuser -u setup-tester -- python3 tests/funk-install/test_setup.py
# Free only our known test instance for the existing concurrent-server tests (no skips).
systemctl stop mumble-funk.service
runuser -u setup-tester -- python3 tests/funk-install/test_install.py build/funk/mumble-server
# Keep another real server, in standard mode, running throughout the Funk update.
python3 tests/funk-install/standard_mode_server.py start /work/mumble-funk/build/funk/mumble-server
# Use a custom database name to check backups, password and stored certificate preservation.
mv /var/lib/mumble-funk/server.sqlite /var/lib/mumble-funk/custom.sqlite
sed -i 's@database=/var/lib/mumble-funk/server.sqlite@database=/var/lib/mumble-funk/custom.sqlite@; s/^users=20$/users=7/' /etc/mumble-funk/server.ini
sha256sum /etc/mumble-funk/server.ini > /tmp/funk-config.sha
python3 - <<'PY'
from contextlib import closing
import sqlite3
from pathlib import Path
with closing(sqlite3.connect('/var/lib/mumble-funk/custom.sqlite')) as db:
    data=list(db.execute('SELECT server_id,key,value FROM config ORDER BY server_id,key'))
    Path('/tmp/funk-database-content.txt').write_text(repr(data))
PY
runuser -u setup-tester -- env FUNK_BUILD_JOBS=1 bash scripts/funk/setup.sh > /tmp/funk-repeat-setup.log 2>&1 || {
    tail -n 100 /tmp/funk-repeat-setup.log; exit 1;
}
sha256sum --check /tmp/funk-config.sha
python3 - <<'PY'
from contextlib import closing
import sqlite3
from pathlib import Path
with closing(sqlite3.connect('/var/lib/mumble-funk/custom.sqlite')) as db:
    assert repr(list(db.execute('SELECT server_id,key,value FROM config ORDER BY server_id,key'))) == Path('/tmp/funk-database-content.txt').read_text()
assert list(Path('/var/backups/mumble-funk').glob('*/state/custom.sqlite'))
PY
python3 tests/funk-install/standard_mode_server.py check
python3 tests/funk-install/standard_mode_server.py stop
if grep -q 'mumble-server.service' /tmp/funk-systemctl-calls; then exit 1; fi
printf 'UBUNTU_SETUP_CHECK_OK: %s (real build/start; systemctl simulated; no boot/supervision test)\n' "$VERSION_ID"
