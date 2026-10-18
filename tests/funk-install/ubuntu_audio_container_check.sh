#!/usr/bin/env bash
# Run only in disposable Ubuntu images containing the earlier stable setup test.
set -euo pipefail
umask 077
[[ $EUID == 0 && -f /.dockerenv && -d /input/.git && -x /opt/mumble-funk/current/mumble-server ]]
export http_proxy=${HTTP_PROXY:-}
export https_proxy=${HTTPS_PROXY:-}
export GIT_SSL_CAINFO=/run/proxy-ca.pem
export SSL_CERT_FILE=/run/proxy-ca.pem
mkdir -p /work/audio-preview
tar -C /input --exclude=./build --exclude=./.cache -cf - . | tar -C /work/audio-preview -xf -
chown -R setup-tester:setup-tester /work/audio-preview
# The cloud's shared disk is below the conservative 8 GiB installation gate.
# Only that resource probe is simulated; apt, compiler, CTest and server are real.
# Production setup.sh and its resource checks are not modified.
mkdir -p /tmp/audio-resource-bin
cat > /tmp/audio-resource-bin/df <<'EOF'
#!/bin/sh
/usr/bin/df "$@" | awk 'NR==2 {$4=16777216} {print}'
EOF
chmod 0755 /tmp/audio-resource-bin/df
chmod 0755 /tmp/audio-resource-bin
cd /work/audio-preview
install -m 0755 tests/funk-install/mock_audio_systemctl.py /usr/local/bin/systemctl
shellcheck scripts/funk/platform.sh scripts/funk/audio-preview/*.sh scripts/funk/setup-audio-preview.sh tests/funk-install/ubuntu_audio_container_check.sh
# Record all stable instance files; preview setup/updates/management/removal must preserve them.
find /etc/mumble-funk /var/lib/mumble-funk /var/log/mumble-funk /opt/mumble-funk \
    /etc/systemd/system/mumble-funk.service /usr/local/sbin/mumble-funk-info -type f -exec sha256sum {} + | sort > /tmp/audio-protected-before
# A completed test intentionally retains preview data. Re-running tests an upgrade.
if [[ ! -f /etc/mumble-funk-audio/server.ini ]]; then
# Occupied preview port: refuse without stopping the foreign listener or creating configuration.
python3 - <<'PY'
import subprocess
from pathlib import Path
process=subprocess.Popen(['python3','-c',"import socket,time; a=socket.socket(); a.bind(('0.0.0.0',64740)); a.listen(); b=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); b.bind(('0.0.0.0',64740)); time.sleep(300)"],start_new_session=True)
Path('/tmp/audio-holder-pid').write_text(str(process.pid))
PY
sleep .2
if runuser -u setup-tester -- env PATH="/tmp/audio-resource-bin:$PATH" bash scripts/funk/setup-audio-preview.sh > /tmp/audio-occupied.log 2>&1; then
    echo 'Occupied port was not refused'; exit 1
fi
grep -q 'Port 64740 ist bereits' /tmp/audio-occupied.log
[[ ! -e /etc/mumble-funk-audio/server.ini ]]
kill "$(cat /tmp/audio-holder-pid)"
fi
runuser -u setup-tester -- env PATH="/tmp/audio-resource-bin:$PATH" FUNK_BUILD_JOBS=2 bash scripts/funk/setup-audio-preview.sh > /tmp/audio-first-setup.log 2>&1 || {
    tail -n 90 /tmp/audio-first-setup.log; exit 1;
}
systemd-analyze verify /etc/systemd/system/mumble-funk-audio.service
/usr/local/sbin/mumble-funk-audio-info > /tmp/audio-info.log
# Create a rights-clear synthetic fixture for the management test.
python3 - <<'PY'
import math,struct,wave
with wave.open('/tmp/custom-busy.wav','wb') as file:
    file.setparams((1,2,16000,0,'NONE','not compressed'))
    file.writeframes(b''.join(struct.pack('<h',int(16000*math.sin(2*math.pi*800*i/16000))) for i in range(1600)))
PY
audio_offset=$(stat -c %s /var/log/mumble-funk-audio/server.log)
/usr/local/sbin/mumble-funk-audio set-busy-tone /tmp/custom-busy.wav
# The actual preview server must start with the custom asset, not merely accept the command.
for _attempt in {1..50}; do
    if tail -c "+$((audio_offset + 1))" /var/log/mumble-funk-audio/server.log | grep -q 'Busy tone source: custom PCM WAV'; then
        break
    fi
    sleep .1
done
tail -c "+$((audio_offset + 1))" /var/log/mumble-funk-audio/server.log | grep -q 'Busy tone source: custom PCM WAV'
/usr/local/sbin/mumble-funk-audio reset-busy-tone
/usr/local/sbin/mumble-funk-audio volume 35
/usr/local/sbin/mumble-funk-audio disable
/usr/local/sbin/mumble-funk-audio enable
sha256sum /etc/mumble-funk-audio/server.ini > /tmp/audio-config-before
# Repeat setup: reuse compiled artifacts, run tests, retain password/config/database/certificate.
runuser -u setup-tester -- env PATH="/tmp/audio-resource-bin:$PATH" FUNK_BUILD_JOBS=2 bash scripts/funk/setup-audio-preview.sh > /tmp/audio-repeat-setup.log 2>&1 || {
    tail -n 90 /tmp/audio-repeat-setup.log; exit 1;
}
sha256sum -c /tmp/audio-config-before
bash scripts/funk/audio-preview/uninstall.sh
[[ ! -e /etc/systemd/system/mumble-funk-audio.service && -f /etc/mumble-funk-audio/server.ini && -f /var/lib/mumble-funk-audio/server.sqlite ]]
find /etc/mumble-funk /var/lib/mumble-funk /var/log/mumble-funk /opt/mumble-funk \
    /etc/systemd/system/mumble-funk.service /usr/local/sbin/mumble-funk-info -type f -exec sha256sum {} + | sort > /tmp/audio-protected-after
cmp /tmp/audio-protected-before /tmp/audio-protected-after
echo 'AUDIO PREVIEW UBUNTU CONTAINER CHECK PASSED (systemctl simulated; server real)'
