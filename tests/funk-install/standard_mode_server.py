#!/usr/bin/env python3
"""Disposable-container probe using our real binary with simplex=false (not Ubuntu's Mumble package)."""
from contextlib import closing
import json
import os
from pathlib import Path
import signal
import socket
import sqlite3
import subprocess
import sys
import time

if os.geteuid() != 0 or not Path('/.dockerenv').exists():
    sys.exit('Nur im wegwerfbaren Root-Testcontainer verwenden')
root = Path('/tmp/funk-standard-mode')
root.mkdir(exist_ok=True)
config = root / 'server.ini'
database = root / 'server.sqlite'
record = root / 'probe.json'
action = sys.argv[1]
if action == 'start':
    config.write_text(f'host=127.0.0.1\nport=64738\ndatabase={database}\nlogfile=\nice=\nbonjour=false\nsimplex=false\n')
    with (root / 'server.log').open('ab') as log:
        process = subprocess.Popen([sys.argv[2], '-ini', str(config), '-fg'], stdout=log, stderr=subprocess.STDOUT,
                                   stdin=subprocess.DEVNULL, start_new_session=True)
    for _ in range(150):
        try:
            with socket.create_connection(('127.0.0.1', 64738), timeout=.1):
                break
        except OSError:
            time.sleep(.05)
    else:
        sys.exit('Standardmodus-Testserver startet nicht')
    for _ in range(150):
        with closing(sqlite3.connect(database)) as db:
            rows = list(db.execute('SELECT server_id,key,value FROM config ORDER BY server_id,key'))
        if {'certificate', 'key'}.issubset({row[1] for row in rows}):
            break
        time.sleep(.05)
    else:
        sys.exit('Standardmodus-Testserver hat noch kein Zertifikat gespeichert')
    record.write_text(json.dumps({'pid': process.pid, 'config': config.read_text(), 'rows': rows}))
elif action == 'check':
    saved = json.loads(record.read_text())
    os.kill(saved['pid'], 0)
    assert config.read_text() == saved['config']
    with closing(sqlite3.connect(database)) as db:
        rows = list(db.execute('SELECT server_id,key,value FROM config ORDER BY server_id,key'))
    assert json.loads(json.dumps(rows)) == saved['rows']
    with socket.create_connection(('127.0.0.1', 64738), timeout=1):
        pass
    print('STANDARD_MODE_SERVER_PRESERVED: PID, configuration, database settings/certificate, TCP 64738')
elif action == 'stop':
    saved = json.loads(record.read_text())
    os.kill(saved['pid'], signal.SIGTERM)
    for _ in range(100):
        if not Path(f"/proc/{saved['pid']}").exists():
            break
        time.sleep(.05)
else:
    sys.exit('Unbekannte Testaktion')
