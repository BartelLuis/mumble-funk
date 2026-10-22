"""Staged native radio-noise installation; no system paths, users or services touched."""
from pathlib import Path
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import unittest

BINARY = Path(sys.argv.pop(1)).resolve()
REPO = Path(__file__).resolve().parents[2]


class RadioNoiseInstallation(unittest.TestCase):
    def test_fresh_disabled_manual_wav_load_and_update_preservation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stage = root / 'stage'
            command = ['bash', str(REPO / 'scripts/funk/install.sh'), '--setup',
                       '--experimental-radio-noise', '--stage', str(stage), str(BINARY)]
            subprocess.run(command, check=True, capture_output=True)
            ini = stage / 'etc/mumble-funk/server.ini'
            original = ini.read_text()
            self.assertNotIn('radionoise=true', original)
            self.assertFalse((stage / 'etc/mumble-funk/sounds/radio-noise.wav').exists())
            info = (stage / 'opt/mumble-funk/current/BUILD-INFO').read_text()
            # GitHub Actions checks out PRs detached. Source authorization and
            # radio-noise BUILD-INFO are covered by the orchestration fixture.
            branch = subprocess.check_output(['git', '-C', str(REPO), 'branch', '--show-current'],
                                             text=True).strip()
            self.assertIn(f'branch={branch}\n', info)
            if branch == 'feature/radio-noise':
                self.assertIn('experimental=true\n', info)
            self.assertEqual(ini.stat().st_mode & 0o777, 0o640)
            self.assertEqual((stage / 'etc/mumble-funk/sounds').stat().st_mode & 0o777, 0o750)

            # Use an optional private file outside Git; the public fixture is
            # sufficient when the private recording is unavailable in CI.
            source = Path(os.environ.get('MUMBLE_FUNK_NOISE_WAV',
                                        str(REPO / 'assets/audio/busy-tone.wav')))
            recording = source.read_bytes()
            wav = stage / 'etc/mumble-funk/sounds/radio-noise.wav'

            def startup(extra):
                with socket.socket() as reserve:
                    reserve.bind(('127.0.0.1', 0))
                    port = reserve.getsockname()[1]
                text = original.replace('host=0.0.0.0', 'host=127.0.0.1')
                text = text.replace('port=64739', f'port={port}')
                text = text.replace('/var/lib/mumble-funk', str(stage / 'var/lib/mumble-funk'))
                text = text.replace('/var/log/mumble-funk', str(stage / 'var/log/mumble-funk'))
                text = text.replace('/usr/local/share/mumble-funk/audio', str(stage / 'usr/local/share/mumble-funk/audio'))
                # No secrets are printed; this disposable INI retains the staged password.
                live_ini = root / 'live.ini'
                live_ini.write_text(text + extra)
                log = root / 'startup.log'
                with log.open('wb') as output:
                    server = subprocess.Popen([str(BINARY), '-ini', str(live_ini), '-fg'],
                                              stdout=output, stderr=subprocess.STDOUT)
                    try:
                        deadline = time.monotonic() + 15
                        while time.monotonic() < deadline:
                            current = log.read_text(errors='replace')
                            if 'Server listening on' in current:
                                return current
                            self.assertIsNone(server.poll(), current)
                            time.sleep(.05)
                        self.fail('Disposable server did not start')
                    finally:
                        server.terminate()
                        try:
                            server.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            server.kill()
                            server.wait()

            disabled = startup('')
            self.assertNotIn('Experimental radio noise enabled:', disabled)
            self.assertIn('Experimental audio feedback enabled:', disabled)
            shutil.copyfile(source, wav)
            wav.chmod(0o640)
            self.assertEqual(wav.read_bytes(), recording)
            extra = f'\nradionoise=true\nradionoisefile={wav}\nradionoisevolume=20\n'
            enabled = startup(extra)
            self.assertIn('Experimental radio noise enabled:', enabled)
            self.assertIn('Experimental Squelch', enabled)
            ini.write_text(original + '\nradionoise=true\n'
                           'radionoisefile=/etc/mumble-funk/sounds/radio-noise.wav\nradionoisevolume=20\n')
            state = stage / 'var/lib/mumble-funk/server.sqlite'
            protected = {p: p.read_bytes() for p in (ini, wav, state)}
            subprocess.run(command, check=True, capture_output=True)
            for path, content in protected.items():
                self.assertEqual(path.read_bytes(), content, str(path))
            self.assertEqual(wav.stat().st_mode & 0o777, 0o640)
            self.assertEqual(source.read_bytes(), recording)


if __name__ == '__main__':
    unittest.main(verbosity=2)
