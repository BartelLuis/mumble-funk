"""Unit checks for Docker-only lifecycle logic, with temporary directories."""
import importlib.util
import io
import os
from pathlib import Path
import sqlite3
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[1] / 'runtime.py'

class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory();self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name)
        with patch.dict(os.environ,{'FUNK_TEST_ROOT':str(self.root)}):
            spec=importlib.util.spec_from_file_location('container_runtime',SOURCE)
            self.runtime=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.runtime)
        for directory in self.runtime.DIRECTORIES.values(): directory.mkdir(parents=True)
        (self.runtime.DIRECTORIES['state']/'audio').mkdir()
        self.calls=[]
        def initialize_database(args,**kwargs):
            self.calls.append(args)
            with sqlite3.connect(self.runtime.DATABASE) as db:
                db.execute('CREATE TABLE config(server_id INTEGER,key TEXT,value TEXT)')
            return type('Result',(),{'returncode':0})()
        self.database=patch.object(self.runtime.subprocess,'run',side_effect=initialize_database)
        self.database.start();self.addCleanup(self.database.stop)

    def test_first_configuration_secure_and_repeated_start_preserves(self):
        self.runtime.initialize();original=self.runtime.CONFIG.read_bytes()
        values=self.runtime.settings()
        self.assertEqual(values['simplex'],'true');self.assertEqual(values['simplexretrytimeout'],'500')
        self.assertEqual(len(values['serverpassword']),48);self.assertNotIn('serverpassword=',str(self.calls))
        self.assertEqual(self.runtime.CONFIG.stat().st_mode & 0o777,0o600)
        self.assertIn('-disablesu',self.calls[0])
        self.runtime.initialize();self.assertEqual(self.runtime.CONFIG.read_bytes(),original);self.assertEqual(len(self.calls),1)

    def test_missing_config_never_resets_existing_database(self):
        self.runtime.DATABASE.write_bytes(b'protected')
        with self.assertRaises(RuntimeError):self.runtime.initialize()
        self.assertEqual(self.runtime.DATABASE.read_bytes(),b'protected')

    def test_missing_database_without_bootstrap_is_refused(self):
        self.runtime.initialize();self.runtime.DATABASE.unlink()
        with self.assertRaises(RuntimeError):self.runtime.initialize()
        self.assertEqual(len(self.calls),1)

    def test_lock_prevents_two_instances_and_offline_edits(self):
        self.runtime.initialize()
        with self.runtime.instance_lock():
            with self.assertRaises(RuntimeError):
                self.runtime.configure('simplex','false')

    def test_configuration_changes_preserved(self):
        self.runtime.initialize();password=self.runtime.settings()['serverpassword']
        self.runtime.configure('simplexdiagnostics','false');self.runtime.configure('simplextimeout','700')
        self.runtime.initialize();values=self.runtime.settings()
        self.assertEqual(values['simplexdiagnostics'],'false');self.assertEqual(values['simplextimeout'],'700')
        self.assertEqual(values['serverpassword'],password)
        with self.assertRaises(RuntimeError):self.runtime.configure('database','/etc/foreign.sqlite')

    def test_reserved_paths_and_duplicates_fail_closed(self):
        self.runtime.initialize()
        original=self.runtime.CONFIG.read_text()
        for text in (original+'port=60000\n',original.replace('port=64739','port=64738'),original.replace('/var/lib/mumble-funk/server.sqlite','/etc/other.sqlite')):
            self.runtime.CONFIG.write_text(text)
            with self.assertRaises(RuntimeError):self.runtime.settings()

if __name__=='__main__':unittest.main()
