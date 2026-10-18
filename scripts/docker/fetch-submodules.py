#!/usr/bin/env python3
"""Fetch only the existing upstream submodules at the v0.2 gitlink commits."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
for item in json.loads((root / 'scripts/docker/submodules.lock.json').read_text()):
    path = root / item['path']
    path.mkdir(parents=True, exist_ok=True)
    subprocess.run(['git', 'init', '--quiet', str(path)], check=True)
    subprocess.run(['git', '-C', str(path), 'fetch', '--quiet', '--depth=1', item['url'], item['commit']], check=True)
    subprocess.run(['git', '-C', str(path), 'checkout', '--quiet', '--detach', 'FETCH_HEAD'], check=True)
    actual = subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
    if actual != item['commit']:
        raise RuntimeError('Submodule commit mismatch: '+item['path'])
