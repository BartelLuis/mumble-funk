#!/usr/bin/env bash
# Integration harness, never run on a host or user's VM. /input is a read-only repo mount.
set -euo pipefail
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
mkdir -p /work
mkdir -p /work/mumble-funk
cp -a /input/. /work/mumble-funk/
if ! id setup-tester >/dev/null 2>&1; then
    useradd --create-home --shell /bin/bash setup-tester
fi
chown -R setup-tester:setup-tester /work/mumble-funk
printf 'setup-tester ALL=(ALL) NOPASSWD: ALL\n' > /etc/sudoers.d/funk-setup-test
printf 'Defaults env_keep += "http_proxy https_proxy HTTP_PROXY HTTPS_PROXY SSL_CERT_FILE GIT_SSL_CAINFO"\n' >> /etc/sudoers.d/funk-setup-test
chmod 0440 /etc/sudoers.d/funk-setup-test
install -m 0755 /work/mumble-funk/tests/funk-install/mock_systemctl.py /usr/local/bin/systemctl
cd /work/mumble-funk
shellcheck scripts/funk/*.sh tests/funk-install/ubuntu_container_check.sh
# The wrapper simulates service calls, while creating real files and running the real binary.
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
# Static checks and all setup safety tests, as a non-root user.
runuser -u setup-tester -- python3 tests/funk-install/test_setup.py
runuser -u setup-tester -- python3 tests/funk-install/test_install.py build/funk/mumble-server
# Use a custom database name to check backup and preservation, including the stored certificate.
systemctl stop mumble-funk.service
mv /var/lib/mumble-funk/server.sqlite /var/lib/mumble-funk/custom.sqlite
sed -i 's@database=/var/lib/mumble-funk/server.sqlite@database=/var/lib/mumble-funk/custom.sqlite@; s/^users=20$/users=7/' /etc/mumble-funk/server.ini
sha256sum /etc/mumble-funk/server.ini > /tmp/funk-config.sha
python3 - <<'PY'
import sqlite3
from pathlib import Path
with sqlite3.connect('/var/lib/mumble-funk/custom.sqlite') as db:
    data=list(db.execute('SELECT server_id,key,value FROM config ORDER BY server_id,key'))
    Path('/tmp/funk-database-content.txt').write_text(repr(data))
PY
runuser -u setup-tester -- env FUNK_BUILD_JOBS=1 bash scripts/funk/setup.sh > /tmp/funk-repeat-setup.log 2>&1 || {
    tail -n 100 /tmp/funk-repeat-setup.log; exit 1;
}
sha256sum --check /tmp/funk-config.sha
python3 - <<'PY'
import sqlite3
from pathlib import Path
with sqlite3.connect('/var/lib/mumble-funk/custom.sqlite') as db:
    assert repr(list(db.execute('SELECT server_id,key,value FROM config ORDER BY server_id,key'))) == Path('/tmp/funk-database-content.txt').read_text()
assert list(Path('/var/backups/mumble-funk').glob('*/state/custom.sqlite'))
PY
# A separate real standard server and occupied-port checks are covered in test_install.py.
if grep -q 'mumble-server.service' /tmp/funk-systemctl-calls; then exit 1; fi
printf 'UBUNTU_SETUP_CHECK_OK: %s (real build/start; systemctl simulated; no boot/supervision test)\n' "$VERSION_ID"
