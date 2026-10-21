"""Native staged installation and shared native/container audio administration."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BINARY = Path(sys.argv.pop(1)).resolve()
REPO = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('native_audio', REPO/'scripts/funk/audio.py')
AUDIO = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIO)


class SquelchAdminTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.install = ['bash', str(REPO/'scripts/funk/install.sh'), '--setup', '--stage', str(self.root), str(BINARY)]
        subprocess.run(self.install, check=True, capture_output=True)
        self.config = self.root/'etc/mumble-funk/server.ini'
        self.control = self.root/'usr/local/sbin/mumble-funk-audio'
        self.sound = self.root/'usr/local/share/mumble-funk/audio'
        self.sounds = self.root/'etc/mumble-funk/sounds'
        self.binary_hash = hashlib.sha256(BINARY.read_bytes()).digest()

    def command(self, *args, ok=True):
        result = subprocess.run([sys.executable, str(self.control), *args, '--root', str(self.root), '--binary', str(BINARY)], capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, ok, result.stdout+result.stderr)
        return result

    def status(self):
        return json.loads(self.command('status').stdout)

    def test_defaults_assets_permissions_and_safe_status(self):
        state = self.status()
        self.assertEqual(state['busytonefile'], AUDIO.BUSY_STANDARD)
        self.assertEqual(state['squelch'], 'false')
        self.assertEqual(state['audiofeedback'], 'false')
        self.assertEqual(state['squelchopen'], 'true')
        self.assertEqual(state['squelchclose'], 'true')
        self.assertEqual(state['squelchclosetimeoutfallback'], 'false')
        for name in ('busy-tone.wav', 'squelch-open.wav', 'squelch-close.wav', 'LICENSE.md'):
            path = self.sound/name
            self.assertEqual(path.read_bytes(), (REPO/'assets/audio'/name).read_bytes())
            self.assertEqual(path.stat().st_mode & 0o777, 0o644)
        self.assertEqual(self.sounds.stat().st_mode & 0o777, 0o750)
        self.assertEqual(self.config.stat().st_mode & 0o777, 0o640)
        secret = [line for line in self.config.read_text().splitlines() if line.startswith('serverpassword=')][0].split('=', 1)[1]
        self.assertNotIn(secret, self.command('status').stdout)
        info = (self.root/'opt/mumble-funk/current/BUILD-INFO').read_text()
        # PR CI stages an isolated install from a detached synthetic merge commit.
        # Staging must report its actual source, not pretend to be the feature head.
        branch = subprocess.check_output(['git', '-C', str(REPO), 'branch', '--show-current'], text=True).strip()
        self.assertIn('branch='+branch+'\n', info)
        self.assertIn('experimental='+str(branch in ('feature/squelch-audio', 'feature/audio-volume', 'feature/busy-tone-asset')).lower()+'\n', info)
        revision = subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', 'HEAD'], text=True).strip()
        self.assertIn('revision='+revision+'\n', info)
        self.assertFalse((self.root/'etc/systemd/system/mumble-funk-audio-engine.service').exists())

    def test_master_and_independent_switches_keep_busy_settings(self):
        self.command('enable-squelch')
        self.assertEqual(self.status()['squelch'], 'true')
        self.assertEqual(self.status()['audiofeedback'], 'true')
        self.command('disable-open'); self.command('enable-close')
        self.assertEqual(self.status()['squelchopen'], 'false')
        self.assertEqual(self.status()['squelchclose'], 'true')
        self.command('disable-close'); self.command('enable-open')
        self.assertEqual(self.status()['squelchclose'], 'false')
        self.assertEqual(self.status()['squelchopen'], 'true')
        self.command('disable-squelch')
        self.assertEqual(self.status()['squelch'], 'false')
        self.assertEqual(self.status()['audiofeedback'], 'true')
        self.assertEqual(self.status()['busytone'], 'true')

    def test_independent_volumes_and_invalid_values(self):
        for cmd, key, value in [('open-volume', 'squelchopenvolume', '17'), ('close-volume', 'squelchclosevolume', '83'), ('volume', 'busytonevolume', '31')]:
            self.command(cmd, value)
            self.assertEqual(self.status()[key], value)
        before = self.config.read_bytes()
        for cmd in ('open-volume', 'close-volume', 'volume'):
            for value in ('101', '-1', '1.5', 'loud', '1000'):
                self.command(cmd, value, ok=False)
                self.assertEqual(self.config.read_bytes(), before)

    def test_boost_all_values_preserves_settings_assets_and_updates(self):
        self.assertEqual(self.status()['feedbackboostdb'], '0')
        # Missing key is the legacy default; status must not migrate the INI.
        self.config.write_text(self.config.read_text().replace('feedbackboostdb=0\n', ''))
        original = self.config.read_bytes()
        self.assertEqual(self.status()['feedbackboostdb'], '0')
        self.assertEqual(self.config.read_bytes(), original)
        before = self.status(); before.pop('feedbackboostdb')
        for value in range(13):
            self.command('boost', str(value))
            state = self.status()
            self.assertEqual(state.pop('feedbackboostdb'), str(value))
            self.assertEqual(state, before)  # Does not enable disabled effects.
        for cmd, name in [('set-open','squelch-open.wav'),('set-close','squelch-close.wav'),('set-busy-tone','squelch-close.wav')]:
            self.command(cmd, str(REPO/'assets/audio'/name))
        self.command('enable-timeout-close')
        original = self.config.read_bytes()
        files = {p: p.read_bytes() for p in self.sounds.glob('*.wav')}
        subprocess.run(self.install, check=True, capture_output=True)
        self.assertEqual(self.config.read_bytes(), original)
        for p, content in files.items(): self.assertEqual(p.read_bytes(), content)
        for value in ('13','100','-1','1.5','NaN','inf','loud','01','+6','6e0'):
            self.command('boost', value, ok=False)
            self.assertEqual(self.config.read_bytes(), original)
        self.command('boost','0')
        self.assertEqual(self.status()['feedbackboostdb'],'0')

    def test_timeout_close_opt_in_preserves_other_settings_and_updates(self):
        before = self.status()
        self.command('enable-timeout-close')
        enabled = self.status()
        self.assertEqual(enabled.pop('squelchclosetimeoutfallback'), 'true')
        before.pop('squelchclosetimeoutfallback')
        self.assertEqual(enabled, before)  # Does not enable Squelch, master feedback or alter Busy.
        original = self.config.read_bytes()
        subprocess.run(self.install, check=True, capture_output=True)
        self.assertEqual(self.config.read_bytes(), original)
        self.command('disable-timeout-close')
        self.assertEqual(self.status()['squelchclosetimeoutfallback'], 'false')

    def test_custom_wavs_and_update_preservation_without_recompilation(self):
        for signal in ('open', 'close'):
            self.command('set-'+signal, str(REPO/'assets/audio'/('squelch-'+signal+'.wav')))
            value = self.status()['squelch'+signal+'file']
            target = self.root/value.lstrip('/')
            self.assertEqual(target.read_bytes(), (REPO/'assets/audio'/('squelch-'+signal+'.wav')).read_bytes())
            self.assertEqual(target.stat().st_mode & 0o777, 0o640)
        self.command('open-volume', '73'); self.command('enable-squelch')
        before = self.config.read_bytes()
        custom = {p: p.read_bytes() for p in self.sounds.glob('*.wav')}
        subprocess.run(self.install, check=True, capture_output=True)
        self.assertEqual(self.config.read_bytes(), before)
        for p, data in custom.items(): self.assertEqual(p.read_bytes(), data)
        self.assertEqual(hashlib.sha256(BINARY.read_bytes()).digest(), self.binary_hash)
        for signal in ('open', 'close'):
            self.command('reset-'+signal)
            self.assertEqual(self.status()['squelch'+signal+'file'], AUDIO.DEFAULTS['squelch'+signal+'file'])
        # Reset does not delete the imported files; a previous configuration can use them again.
        for p, data in custom.items(): self.assertEqual(p.read_bytes(), data)

    def test_busy_parser_and_fallback_unchanged(self):
        before = self.config.read_bytes()
        self.command('set-busy-tone', str(REPO/'assets/audio/squelch-open.wav'), ok=False)  # <20ms remains invalid for Busy.
        self.assertEqual(self.config.read_bytes(), before)
        self.command('set-busy-tone', str(REPO/'assets/audio/squelch-close.wav'))
        self.assertTrue(self.status()['busytonefile'].startswith('/etc/mumble-funk/sounds/busy-'))
        self.command('reset-busy-tone')
        self.assertEqual(self.status()['busytonefile'], '')
        self.command('enable'); self.command('disable')
        self.assertEqual(self.status()['audiofeedback'], 'false')

    def test_standard_legacy_custom_and_reset_preserve_all_other_settings(self):
        before = self.status(); before.pop('busytonefile')
        self.command('reset-busy-tone')
        self.assertEqual(self.status()['busytonefile'], '')  # Historical reset remains synthetic.
        original = self.config.read_bytes()
        subprocess.run(self.install, check=True, capture_output=True)
        self.assertEqual(self.config.read_bytes(), original)
        self.config.write_bytes(original.replace(b'busytonefile=\n', b''))
        original = self.config.read_bytes()
        self.assertEqual(self.status()['busytonefile'], '')  # Missing is synthetic too.
        subprocess.run(self.install, check=True, capture_output=True)
        self.assertEqual(self.config.read_bytes(), original)
        self.command('use-standard-busy-tone')
        self.assertEqual(self.status()['busytonefile'], AUDIO.BUSY_STANDARD)
        selected = self.status(); selected.pop('busytonefile')
        self.assertEqual(selected, before)  # Never auto-enables or changes volumes/boost/Squelch.
        self.command('set-busy-tone', str(REPO/'assets/audio/busy-tone.wav'))
        custom = self.status()['busytonefile']
        saved = self.root/custom.lstrip('/')
        self.assertEqual(saved.read_bytes(), (REPO/'assets/audio/busy-tone.wav').read_bytes())
        self.command('boost','12'); self.command('volume','100')
        original = self.config.read_bytes()
        subprocess.run(self.install, check=True, capture_output=True)
        self.assertEqual(self.config.read_bytes(), original)
        self.assertEqual(saved.read_bytes(), (REPO/'assets/audio/busy-tone.wav').read_bytes())
        self.command('reset-busy-tone'); self.assertEqual(self.status()['busytonefile'],'')
        self.command('use-standard-busy-tone'); self.assertEqual(self.status()['busytonefile'],AUDIO.BUSY_STANDARD)
        original = self.config.read_bytes()
        (self.sound/'busy-tone.wav').write_bytes(b'invalid')
        self.command('use-standard-busy-tone', ok=False)
        self.assertEqual(self.config.read_bytes(), original)
        self.assertEqual(saved.read_bytes(), (REPO/'assets/audio/busy-tone.wav').read_bytes())

    def test_existing_file_or_symlink_at_new_standard_path_is_not_overwritten(self):
        target=self.sound/'busy-tone.wav'
        target.write_bytes(b'operator-owned recording')
        before={p:p.read_bytes() for p in self.root.rglob('*') if p.is_file() and not p.is_symlink()}
        result=subprocess.run(self.install,capture_output=True)
        self.assertNotEqual(result.returncode,0)
        for p,content in before.items(): self.assertEqual(p.read_bytes(),content)
        own=self.root/'own.wav';own.write_bytes(b'own file')
        target.unlink();target.symlink_to(own)
        result=subprocess.run(self.install,capture_output=True)
        self.assertNotEqual(result.returncode,0)
        self.assertTrue(target.is_symlink());self.assertEqual(own.read_bytes(),b'own file')

    def test_bad_or_unsafe_files_do_not_change_configuration(self):
        original = self.config.read_bytes()
        bad = self.root/'bad.wav'; bad.write_bytes(b'corrupt')
        huge = self.root/'huge.wav'; huge.write_bytes(b'x'*131073)
        link = self.root/'link.wav'; link.symlink_to(REPO/'assets/audio/squelch-open.wav')
        fifo = self.root/'pipe.wav'; os.mkfifo(fifo)
        for path in (bad, huge, link, fifo, self.root/'missing.wav', self.root):
            for signal in ('open', 'close'):
                self.command('set-'+signal, str(path), ok=False)
                self.assertEqual(self.config.read_bytes(), original)

    def test_duplicates_and_other_sections_preserved(self):
        self.config.write_text(self.config.read_text()+'\n[Other]\nsquelch=true\nkeep=value\n')
        self.command('disable-squelch')
        self.assertIn('[Other]\nsquelch=true\nkeep=value\n', self.config.read_text())
        self.config.write_text('squelchopenfile=/duplicate\n'+self.config.read_text())
        before = self.config.read_bytes()
        self.command('set-open', str(REPO/'assets/audio/squelch-open.wav'), ok=False)
        self.assertEqual(self.config.read_bytes(), before)

    def test_enable_requires_simplex_and_valid_assets(self):
        self.config.write_text(self.config.read_text().replace('simplex=true', 'simplex=false'))
        original = self.config.read_bytes()
        self.command('enable-squelch', ok=False)
        self.assertEqual(self.config.read_bytes(), original)

        self.config.write_bytes(original.replace(b'simplex=false', b'simplex=true').replace(b'squelch-open.wav', b'missing.wav'))
        original = self.config.read_bytes()
        self.command('enable-squelch', ok=False)
        self.assertEqual(self.config.read_bytes(), original)
        self.command('enable-open', ok=False)
        self.assertEqual(self.config.read_bytes(), original)

    def test_container_storage_and_status_without_systemd(self):
        code = "import importlib.util,sys;s=importlib.util.spec_from_file_location('audio',sys.argv[1]);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);sys.exit(m.main(sys.argv[2:],container=True))"
        command = [sys.executable, '-c', code, str(REPO/'scripts/funk/audio.py'), 'set-open', str(REPO/'assets/audio/squelch-open.wav'), '--root', str(self.root), '--binary', str(BINARY)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        value = self.status()['squelchopenfile']
        boost = [sys.executable, '-c', code, str(REPO/'scripts/funk/audio.py'), 'boost', '9', '--root', str(self.root), '--binary', str(BINARY)]
        result = subprocess.run(boost, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.status()['feedbackboostdb'], '9')
        self.assertTrue(value.startswith('/var/lib/mumble-funk/audio/open-'))
        self.assertEqual((self.root/value.lstrip('/')).stat().st_mode & 0o777, 0o640)


if __name__ == '__main__':
    unittest.main(verbosity=2)
