#!/usr/bin/env python3
"""Container test double: launches a real server, but does NOT emulate systemd supervision/boot."""
import json
import os
from pathlib import Path
import pwd
import signal
import subprocess
import sys
import time

if not Path('/.dockerenv').exists():
    sys.exit('Nur in einem wegwerfbaren Docker-Testcontainer verwenden.')
args = sys.argv[1:]
statefile = Path('/tmp/funk-service-state.json')
state = json.loads(statefile.read_text()) if statefile.exists() else {'pid': 0, 'enabled': False}
command = next((arg for arg in args if not arg.startswith('-')), '')
if any('mumble-server' in arg for arg in args):
    sys.exit('Testfehler: fremder Dienst angesprochen')
with Path('/tmp/funk-systemctl-calls').open('a') as log:
    log.write(' '.join(args) + '\n')
if command == 'show':
    prop = next((arg.split('=', 1)[1] for arg in args if arg.startswith('--property=')), '')
    if prop == 'Version':
        print('container-test-double')
    elif prop == 'MainPID':
        print(state['pid'])
    elif prop == 'FragmentPath':
        print('/etc/systemd/system/mumble-funk.service' if Path('/etc/systemd/system/mumble-funk.service').exists() else '')
    elif prop == 'ExecStart':
        print('/opt/mumble-funk/current/mumble-server -ini /etc/mumble-funk/server.ini -fg')
elif command == 'stop':
    if state['pid']:
        try:
            os.kill(state['pid'], signal.SIGTERM)
        except ProcessLookupError:
            pass
        for _ in range(100):
            if not Path(f"/proc/{state['pid']}").exists():
                break
            time.sleep(.05)
        else:
            sys.exit('Testserver wurde nicht sauber beendet')
    state['pid'] = 0
elif command == 'start':
    user = pwd.getpwnam('mumble-funk')
    with Path('/var/log/mumble-funk/server.log').open('ab') as output:
        process = subprocess.Popen(['/opt/mumble-funk/current/mumble-server', '-ini', '/etc/mumble-funk/server.ini', '-fg'],
                                   user=user.pw_uid, group=user.pw_gid, extra_groups=[],
                                   cwd='/var/lib/mumble-funk', stdin=subprocess.DEVNULL,
                                   stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
    state['pid'] = process.pid
elif command == 'enable':
    state['enabled'] = True
elif command == 'is-enabled':
    print('enabled' if state['enabled'] else 'disabled')
    sys.exit(0 if state['enabled'] else 1)
elif command in ('is-active', 'status'):
    try:
        if not state['pid']:
            raise ProcessLookupError
        os.kill(state['pid'], 0)
    except ProcessLookupError:
        print('inactive')
        sys.exit(3)
    print('active')
elif command != 'daemon-reload':
    sys.exit('Nicht unterstützter Testaufruf: ' + command)
if os.geteuid() == 0:
    statefile.write_text(json.dumps(state))
