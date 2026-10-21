#!/usr/bin/env python3
"""Real Docker/Compose tests. Creates only uniquely named disposable test resources."""
import argparse
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import socket
import sqlite3
import ssl
import subprocess
import tarfile
import tempfile
import time

REPO = Path(__file__).resolve().parents[3]


def check_compose_env_ports():
    """Resolve real Compose fixtures; never touch an operator's .env or containers."""
    current = os.environ.copy()
    for key in ('FUNK_HOST_PORT', 'COMPOSE_PROJECT_NAME', 'COMPOSE_FILE',
                'COMPOSE_ENV_FILES', 'COMPOSE_DISABLE_ENV_FILE'):
        current.pop(key, None)
    source = (REPO / 'compose.yaml').read_bytes()
    assert source.count(b'${FUNK_HOST_PORT:-64740}') == 2
    # Historical Compose differs only in its fallback, not its resource names.
    legacy = source.replace(b'${FUNK_HOST_PORT:-64740}', b'${FUNK_HOST_PORT:-64739}')
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        compose_file = directory / 'compose.yaml'
        env_file = directory / '.env'

        def resolve(compose, original):
            compose_file.write_bytes(compose)
            if original is None:
                env_file.unlink(missing_ok=True)
            else:
                env_file.write_bytes(original)
                env_file.chmod(0o600)
            # No --env-file: exercise actual implicit .env discovery AND absence.
            result = subprocess.run(
                ['docker', 'compose', '--project-directory', str(directory),
                 '-f', str(compose_file), 'config', '--format', 'json'],
                env=current, capture_output=True, check=True)
            if original is None:
                assert not env_file.exists(), 'Compose must not create .env'
            else:
                assert env_file.read_bytes() == original, 'Existing .env must not be rewritten'
                assert env_file.stat().st_mode & 0o777 == 0o600
            return json.loads(result.stdout)

        prefix = b'# Existing operator settings must survive\nCOMPOSE_PROJECT_NAME=funk-port-config-check\n'
        fixtures = [
            ('no .env', None, 64740),
            ('no port setting', prefix, 64740),
            ('empty port setting', prefix + b'FUNK_HOST_PORT=\n', 64740),
            ('explicit 64740', prefix + b'FUNK_HOST_PORT=64740\n', 64740),
            ('legacy explicit 64739', prefix + b'FUNK_HOST_PORT=64739\n', 64739),
            ('custom port', prefix + b'FUNK_HOST_PORT=65001\n', 65001),
            ('new .env.example', (REPO / '.env.example').read_bytes(), 64740),
        ]
        for label, original, expected in fixtures:
            config = resolve(source, original)
            expected_name = ('funk-port-config-check' if original is not None
                             and original.startswith(prefix) else 'mumble-funk-docker')
            assert config['name'] == expected_name, label
            ports = config['services']['mumble-funk']['ports']
            assert len(ports) == 2, 'No extra audio port may be published'
            assert {(entry['protocol'], int(entry['published']), int(entry['target']))
                    for entry in ports} == {('tcp', expected, 64739), ('udp', expected, 64739)}, label
            if label in ('explicit 64740', 'legacy explicit 64739', 'custom port'):
                before = resolve(legacy, original)
                assert before == config == resolve(source, original), 'Update changed existing Compose settings: ' + label
                assert resolve(legacy, original) == before, 'Rollback changed existing settings'
            print(f'PASS: Compose {label}, host {expected} TCP/UDP -> 64739, .env untouched')
        # Without a pinned port the old default really differs. Updates must stop
        # at the documented before/after comparison, then explicitly pin the old
        # published port before recreating a container (no automatic migration).
        old = resolve(legacy, prefix)
        new = resolve(source, prefix)
        assert old['services']['mumble-funk']['ports'] != new['services']['mumble-funk']['ports']
        pinned = prefix + b'FUNK_HOST_PORT=64739\n'
        assert resolve(source, pinned) == resolve(legacy, pinned)
        print('PASS: unpinned legacy port change detected; explicit pin preserves update/rollback')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe',type=Path,required=True)
    parser.add_argument('--native-server',type=Path,required=True)
    parser.add_argument('--host-port',type=int,help='Explicit free TCP/UDP host port, e.g. 64740')
    parser.add_argument('--ports-only',action='store_true',help='Only published-port and optional private-audio checks')
    parser.add_argument('--audio-feedback',action='store_true',help='Also check private feedback in the isolated project')
    parser.add_argument('--image',default='mumble-funk-docker:local')
    parser.add_argument('--runtime-file',type=Path,help='Development-only read-only helper mount; omit for final image verification')
    parser.add_argument('--previous-image',help='Optional older local image for a real image-update retention test')
    args=parser.parse_args()
    check_compose_env_ports()
    project='funk-docker-check-'+str(os.getpid())
    restored=project+'-restored'
    upgrade=project+'-upgrade'
    env=os.environ.copy()
    for key in ('DOCKER_CONTEXT','DOCKER_TLS','DOCKER_TLS_VERIFY','DOCKER_CERT_PATH'):
        env.pop(key,None)
    env['DOCKER_HOST']='unix:///var/run/docker.sock'
    def command(argv,check=True,input=None,project_name=project):
        current=env.copy();current['COMPOSE_PROJECT_NAME']=project_name
        current['FUNK_HOST_PORT']=str(port)
        result=subprocess.run(argv,cwd=REPO,env=current,input=input,capture_output=True)
        if check and result.returncode:
            raise RuntimeError(' '.join(map(str,argv[:6]))+'\n'+result.stderr.decode(errors='replace'))
        return result
    def docker(*argv,**kwargs):return command(['docker',*argv],**kwargs)
    def compose(*argv,**kwargs):return docker('compose',*argv,**kwargs)
    def probe(password=None):
        result=command([str(args.probe),str(port)],input=((password if password is not None else values['serverpassword'])+'\n').encode())
        assert b'encrypted UDP' in result.stdout
    def healthy(project_name=project):
        deadline=time.monotonic()+30
        while time.monotonic()<deadline:
            result=compose('exec','-T','mumble-funk','funk-container','health',check=False,project_name=project_name)
            if result.returncode==0:return
            time.sleep(.1)
        raise RuntimeError('Container did not become healthy: '+compose('logs').stdout.decode(errors='replace'))
    def information(project_name=project):return json.loads(compose('exec','-T','mumble-funk','funk-container','info','--json',project_name=project_name).stdout)
    def certificate():
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT);context.check_hostname=False;context.verify_mode=ssl.CERT_NONE
        with socket.create_connection(('127.0.0.1',port),timeout=3) as raw:
            with context.wrap_socket(raw,server_hostname='localhost') as tls:
                return tls.getpeercert(binary_form=True)
    def fingerprint(project_name=project):
        code="import sqlite3,hashlib;db=sqlite3.connect('/var/lib/mumble-funk/server.sqlite');tables=[(name,db.execute('SELECT * FROM \"'+name+'\" ORDER BY rowid').fetchall()) for (name,) in db.execute(\"SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' AND name!='slog' ORDER BY name\")];print(hashlib.sha256(repr(tables).encode()).hexdigest())"
        return compose('run','--rm','--no-deps','-T','--entrypoint','python3','mumble-funk','-c',code,project_name=project_name).stdout.strip()
    # Reserve the same high host port for both protocols; never stop existing listeners.
    for _ in range(100):
        with socket.socket() as tcp:
            tcp.bind(('127.0.0.1',args.host_port or 0));port=tcp.getsockname()[1]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
            try:udp.bind(('0.0.0.0',port));break
            except OSError:
                if args.host_port:raise
                continue
    else:raise RuntimeError('No free TCP/UDP port')
    projects=[project,restored]+([upgrade] if args.previous_image else [])
    owned_projects=[]
    with tempfile.TemporaryDirectory(dir=os.environ.get('FUNK_TEST_WORKDIR')) as directory:
        directory=Path(directory);native=None;logfile=None
        if args.runtime_file:
            override=directory/'runtime.compose.yaml'
            override.write_text('services:\n  mumble-funk:\n    volumes:\n      - '+json.dumps(str(args.runtime_file.resolve())+':/usr/local/bin/funk-container:ro')+'\n')
            env['COMPOSE_FILE']=str(REPO/'compose.yaml')+os.pathsep+str(override)
        try:
            for name in projects:
                if docker('ps','--all','--quiet','--filter','label=com.docker.compose.project='+name).stdout.strip():
                    raise RuntimeError('Refusing existing test project: '+name)
                if docker('image','inspect',name+':local',check=False).returncode==0:
                    raise RuntimeError('Refusing existing image tag: '+name)
                owned_projects.append(name)
                docker('tag',args.image,name+':local')
            compose('config','--quiet')
            # A foreign process using the desired host port must survive an attempted start.
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as occupied:
                occupied.bind(('0.0.0.0',port))
                result=compose('up','-d','--no-build',check=False)
                assert result.returncode!=0,'Occupied UDP port unexpectedly published'
                assert occupied.getsockname()[1]==port
            compose('down')
            # Also protect foreign TCP listeners.
            with socket.socket() as occupied:
                occupied.bind(('0.0.0.0',port));occupied.listen()
                assert compose('up','-d','--no-build',check=False).returncode!=0
                assert occupied.getsockname()[1]==port
            compose('down')
            compose('up','-d','--no-build');healthy()
            values=information();assert values['simplex']=='true';assert values['simplextimeout']=='500';assert values['simplexretrytimeout']=='500'
            assert len(values['serverpassword'])==48
            cid=compose('ps','--quiet','mumble-funk').stdout.decode().strip()
            inspection=json.loads(docker('inspect',cid).stdout)[0]
            assert inspection['Config']['User']=='10001:10001'
            assert inspection['HostConfig']['ReadonlyRootfs']
            assert inspection['HostConfig']['CapDrop']==['ALL']
            assert inspection['HostConfig']['RestartPolicy']['Name']=='unless-stopped'
            assert set(inspection['HostConfig']['PortBindings'])=={'64739/tcp','64739/udp'}
            for bindings in inspection['HostConfig']['PortBindings'].values():assert all(int(b['HostPort'])==port for b in bindings)
            mode=compose('exec','-T','mumble-funk','stat','-c','%a','/etc/mumble-funk/server.ini').stdout.strip()
            assert mode==b'600'
            assert compose('exec','-T','mumble-funk','sh','-c','command -v g++ || command -v cmake',check=False).returncode!=0
            probe();cert=certificate()
            logs=compose('logs').stdout
            assert b'Simplex enabled:' in logs and values['serverpassword'].encode() not in logs
            assert b"Password for 'SuperUser'" not in logs
            assert b'Experimental audio feedback enabled:' not in logs, 'Audio must default off in Docker'
            if args.audio_feedback:
                original_config=compose('exec','-T','mumble-funk','cat','/etc/mumble-funk/server.ini').stdout
                state=json.loads(command(['bash','scripts/docker/audio.sh','status']).stdout)
                assert state['squelch']=='false' and state['audiofeedback']=='false' and state['feedbackboostdb']=='0'
                assert state['busytonefile']=='/usr/local/share/mumble-funk/audio/busy-tone.wav'
                command(['bash','scripts/docker/audio.sh','boost','6']);healthy()
                state=json.loads(command(['bash','scripts/docker/audio.sh','status']).stdout)
                assert state['feedbackboostdb']=='6' and state['squelch']=='false' and state['audiofeedback']=='false'
                assert command(['bash','scripts/docker/audio.sh','boost','13'],check=False).returncode!=0
                assert values['serverpassword'] not in json.dumps(state)
                asset_check="import hashlib,os;from pathlib import Path;p=Path('/usr/local/share/mumble-funk/audio');assert os.getuid()==10001;assert hashlib.sha256((p/'busy-tone.wav').read_bytes()).hexdigest()=='c24e7af67bc9aac5b040fad1118f006d79540a34b427d064e9a5596d0f11d107';assert hashlib.sha256((p/'squelch-open.wav').read_bytes()).hexdigest()=='81573e3a11db089de7cf2bbb2918e0747a2edde12923c399ac5042513d038770';assert hashlib.sha256((p/'squelch-close.wav').read_bytes()).hexdigest()=='8d9091e0e4ee36433eb8aa707a7fa4a19ccec8ce5dee85ef1d91fa478afd0e31'"
                compose('exec','-T','mumble-funk','python3','-c',asset_check)
                assert compose('exec','-T','mumble-funk','funk-container','audio','enable-squelch',check=False).returncode!=0
                # Exercise the operator wrapper on this test's unique project.
                command(['bash','scripts/docker/audio.sh','enable-squelch']);healthy()
                command(['bash','scripts/docker/audio.sh','enable-timeout-close']);healthy()
                command(['bash','scripts/docker/audio.sh','disable-open']);healthy()
                command(['bash','scripts/docker/audio.sh','close-volume','29']);healthy()
                command(['bash','scripts/docker/audio.sh','set-close',str(REPO/'assets/audio/squelch-close.wav')]);healthy()
                state=json.loads(command(['bash','scripts/docker/audio.sh','status']).stdout)
                custom_close=state['squelchclosefile']
                assert custom_close.startswith('/var/lib/mumble-funk/audio/close-')
                assert state['squelchopen']=='false' and state['squelchclose']=='true' and state['squelchclosevolume']=='29'
                assert state['squelchclosetimeoutfallback']=='true' and state['feedbackboostdb']=='6'
                assert compose('exec','-T','mumble-funk','stat','-c','%a',custom_close).stdout.strip()==b'640'
                compose('up','-d','--no-build','--force-recreate');healthy()
                assert json.loads(command(['bash','scripts/docker/audio.sh','status']).stdout)==state
                assert b'Experimental Squelch' in compose('logs').stdout
                probe()
                command(['bash','scripts/docker/audio.sh','boost','0']);healthy()
                command(['bash','scripts/docker/audio.sh','reset-close']);healthy()
                command(['bash','scripts/docker/audio.sh','enable-open']);healthy()
                command(['bash','scripts/docker/audio.sh','disable-timeout-close']);healthy()
                command(['bash','scripts/docker/audio.sh','disable-squelch']);healthy()
                compose('stop')
                # Keep deterministic ownership while the probe listens to the tone;
                # only this disposable audio fixture uses longer leases. Restore bytes below.
                enable="from pathlib import Path;p=Path('/etc/mumble-funk/server.ini');t=p.read_text().replace('simplextimeout=500\\n','simplextimeout=5000\\n').replace('simplexretrytimeout=500\\n','simplexretrytimeout=5000\\n').replace('audiofeedback=false\\n','audiofeedback=true\\n');p.write_text(t)"
                compose('run','--rm','--no-deps','-T','--entrypoint','python3','mumble-funk','-c',enable)
                compose('up','-d','--no-build');healthy()
                # Explicit expected counts are derived from the fixed fixture's
                # 57468 samples + unchanged 960-sample Busy tail: ceil(/960)=61.
                # Never infer expectations from received packet counts.
                for selection,packets,gain in [('use-standard-busy-tone',61,'0'),('reset-busy-tone',13,'0'),('set-busy-tone',61,'12')]:
                    action=['bash','scripts/docker/audio.sh',selection]
                    if selection=='set-busy-tone': action.append(str(REPO/'assets/audio/busy-tone.wav'))
                    command(action);healthy()
                    command(['bash','scripts/docker/audio.sh','boost',gain]);healthy()
                    state=json.loads(command(['bash','scripts/docker/audio.sh','status']).stdout)
                    if selection=='set-busy-tone':
                        custom_busy=state['busytonefile']
                        assert custom_busy.startswith('/var/lib/mumble-funk/audio/busy-')
                    elif selection=='reset-busy-tone': assert state['busytonefile']==''
                    else: assert state['busytonefile']=='/usr/local/share/mumble-funk/audio/busy-tone.wav'
                    result=command([str(args.probe),str(port),'--audio-feedback',str(packets)],input=(values['serverpassword']+'\n').encode())
                    assert b'Private busy tone verified' in result.stdout
                compose('stop')
                restore="import sys;from pathlib import Path;Path('/etc/mumble-funk/server.ini').write_bytes(sys.stdin.buffer.read())"
                compose('run','--rm','--no-deps','-T','--entrypoint','python3','mumble-funk','-c',restore,input=original_config)
                compose('up','-d','--no-build');healthy()
                assert information()==values and certificate()==cert
                print('PASS: private busy feedback over published ports, no channel leakage; default disabled')
            if args.ports_only:
                print(f'PASS: host TCP/UDP {port} -> container TCP/UDP 64739, existing Mumble audio connection')
                return
            # Offline database + audio fixture: exercise retention of the full SQLite file and future assets.
            compose('stop')
            code="import sqlite3;from pathlib import Path;db=sqlite3.connect('/var/lib/mumble-funk/server.sqlite');db.execute('CREATE TABLE docker_preserved(value TEXT)');db.execute(\"INSERT INTO docker_preserved VALUES ('accounts-channels-acls-preserved')\");db.execute(\"INSERT INTO users(server_id,user_id,name) VALUES(1,42,'PersistedUser')\");db.execute(\"INSERT INTO channels(server_id,channel_id,parent_id,name,inheritacl) VALUES(1,42,0,'PersistedChannel',1)\");db.execute(\"INSERT INTO acl(server_id,channel_id,priority,user_id,apply_here,apply_sub,grantpriv,revokepriv) VALUES(1,42,0,42,1,1,14,0)\");db.commit();db.close();Path('/var/lib/mumble-funk/audio/future.txt').write_text('future persistent audio directory')"
            compose('run','--rm','--no-deps','-T','--entrypoint','python3','mumble-funk','-c',code)
            if args.audio_feedback:
                compose('run','--rm','--no-deps','-T','mumble-funk','audio','boost','9')
                # info --json includes every INI setting. Only this deliberately
                # modified gain may differ; retain full equality for all others.
                values = dict(values, feedbackboostdb='9')
                assert json.loads(compose('run','--rm','--no-deps','-T','mumble-funk','audio','status').stdout)['feedbackboostdb']=='9'
            admin_password='test-only-'+os.urandom(24).hex()
            result=compose('run','--rm','--no-deps','-T','mumble-funk','admin-password',input=((admin_password+'\n')*2).encode())
            assert admin_password.encode() not in result.stdout+result.stderr
            check_admin="import sqlite3;db=sqlite3.connect('/var/lib/mumble-funk/server.sqlite');assert db.execute('SELECT pw FROM users WHERE server_id=1 AND user_id=0').fetchone()[0]"
            compose('run','--rm','--no-deps','-T','--entrypoint','python3','mumble-funk','-c',check_admin)
            first=fingerprint()
            compose('up','-d','--force-recreate','--no-build');healthy();assert certificate()==cert;assert information()==values
            compose('stop');assert fingerprint()==first
            compose('run','--rm','--no-deps','-T','mumble-funk','set','simplexdiagnostics','false')
            compose('up','-d','--no-build');healthy();probe()
            assert information()['simplexdiagnostics']=='false'
            compose('restart');healthy();assert certificate()==cert;probe()
            # Offline operations must refuse while the server holds its inherited lock.
            assert compose('exec','-T','mumble-funk','funk-container','backup',check=False).returncode!=0
            backup_path=directory/'safe.tar.gz'
            command(['bash','scripts/docker/manage.sh','backup',str(backup_path)])
            healthy();assert backup_path.stat().st_mode & 0o777==0o600
            with tarfile.open(backup_path) as archive:
                names=archive.getnames();assert 'config/server.ini' in names and 'state/server.sqlite' in names and 'state/audio/future.txt' in names
                if args.audio_feedback:
                    assert b'feedbackboostdb=9\n' in archive.extractfile('config/server.ini').read()
                    assert archive.extractfile('state/audio/'+Path(custom_busy).name).read()==(REPO/'assets/audio/busy-tone.wav').read_bytes()
                    saved=archive.extractfile('state/audio/'+Path(custom_close).name).read()
                    assert saved==(REPO/'assets/audio/squelch-close.wav').read_bytes()
            # Restoring to populated volumes refuses and restarts the original instance.
            assert command(['bash','scripts/docker/manage.sh','restore',str(backup_path)],check=False).returncode!=0
            healthy();assert information()['serverpassword']==values['serverpassword']
            compose('stop')
            before_restore=fingerprint()
            command(['bash','scripts/docker/manage.sh','restore',str(backup_path)],project_name=restored)
            # The restored project has distinct volumes but an identical DB/cert/config.
            restored_code="import sqlite3,hashlib;db=sqlite3.connect('/var/lib/mumble-funk/server.sqlite');tables=[(name,db.execute('SELECT * FROM \"'+name+'\" ORDER BY rowid').fetchall()) for (name,) in db.execute(\"SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' AND name!='slog' ORDER BY name\")];print(hashlib.sha256(repr(tables).encode()).hexdigest())"
            restored_hash=compose('run','--rm','--no-deps','-T','--entrypoint','python3','mumble-funk','-c',restored_code,project_name=restored).stdout.strip()
            assert restored_hash==before_restore
            if args.audio_feedback:
                assert json.loads(compose('run','--rm','--no-deps','-T','mumble-funk','audio','status',project_name=restored).stdout)['feedbackboostdb']=='9'
                assert compose('run','--rm','--no-deps','-T','--entrypoint','cat','mumble-funk',custom_busy,project_name=restored).stdout==(REPO/'assets/audio/busy-tone.wav').read_bytes()
                saved=compose('run','--rm','--no-deps','-T','--entrypoint','cat','mumble-funk',custom_close,project_name=restored).stdout
                assert saved==(REPO/'assets/audio/squelch-close.wav').read_bytes()
            compose('up','-d','--no-build',project_name=restored)
            # Independent root-level Mumble process and Docker run at the same time.
            with socket.socket() as sock:
                sock.bind(('127.0.0.1',0));native_port=sock.getsockname()[1]
            config=directory/'native.ini'
            config.write_text(f'host=127.0.0.1\nport={native_port}\ndatabase={directory}/native.sqlite\nlogfile=\nice=\nbonjour=false\n')
            original=config.read_bytes();logfile=open(directory/'native.log','wb')
            native=subprocess.Popen([str(args.native_server),'-ini',str(config),'-fg'],stdout=logfile,stderr=subprocess.STDOUT)
            for _ in range(100):
                try:
                    with socket.create_connection(('127.0.0.1',native_port),timeout=.1):break
                except OSError:time.sleep(.05)
            else:raise RuntimeError('Native reference server did not start')
            assert native.poll() is None
            # Run the published-port protocol probe against the restored container.
            deadline=time.monotonic()+20
            while time.monotonic()<deadline:
                if compose('exec','-T','mumble-funk','funk-container','health',project_name=restored,check=False).returncode==0:break
                time.sleep(.1)
            probe();assert certificate()==cert;assert config.read_bytes()==original;assert native.poll() is None
            compose('stop',project_name=restored)
            stopped=compose('ps','--all','--quiet','mumble-funk',project_name=restored).stdout.decode().strip()
            state=json.loads(docker('inspect',stopped).stdout)[0]['State']
            assert not state['OOMKilled'] and state['ExitCode'] in (0,15)
            logs=compose('logs',project_name=restored).stdout
            assert b'Shutting down' in logs and values['serverpassword'].encode() not in logs
            if args.previous_image:
                docker('tag',args.previous_image,upgrade+':local')
                compose('up','-d','--no-build',project_name=upgrade);healthy(upgrade)
                old=information(upgrade);old_certificate=certificate()
                old_id=compose('ps','--quiet','mumble-funk',project_name=upgrade).stdout.decode().strip()
                old_image=json.loads(docker('inspect',old_id).stdout)[0]['Image']
                compose('stop',project_name=upgrade)
                compose('run','--rm','--no-deps','-T','--entrypoint','python3','mumble-funk','-c',code,project_name=upgrade)
                compose('run','--rm','--no-deps','-T','mumble-funk','admin-password',input=((admin_password+'\n')*2).encode(),project_name=upgrade)
                old_state=fingerprint(upgrade)
                docker('tag',args.image,upgrade+':local')
                compose('up','-d','--no-build','--force-recreate',project_name=upgrade);healthy(upgrade)
                new_id=compose('ps','--quiet','mumble-funk',project_name=upgrade).stdout.decode().strip()
                assert old_image!=json.loads(docker('inspect',new_id).stdout)[0]['Image']
                assert information(upgrade)==old and certificate()==old_certificate
                probe(old['serverpassword'])
                compose('stop',project_name=upgrade)
                assert fingerprint(upgrade)==old_state
                # A true rollback to the previous binary, using the same named
                # volumes, must retain all stored accounts/channels/ACLs/settings.
                docker('tag',args.previous_image,upgrade+':local')
                compose('up','-d','--no-build','--force-recreate',project_name=upgrade);healthy(upgrade)
                assert information(upgrade)==old and certificate()==old_certificate
                compose('stop',project_name=upgrade)
                assert fingerprint(upgrade)==old_state
                print('PASS: real image update AND rollback preserve full SQLite state, password, configuration and certificate')
            print('PASS: image startup, rootless isolation, published TCP/encrypted UDP, simplex, diagnostics off, restart/recreate, port conflicts, native parallel operation, consistent backup/restore, clean shutdown')
        finally:
            if native:
                native.terminate()
                try:native.wait(timeout=10)
                except subprocess.TimeoutExpired:native.kill();native.wait()
            if logfile:logfile.close()
            # Only the uniquely named resources created by this test are removed.
            for name in owned_projects:
                compose('down','--volumes',project_name=name,check=False)
                docker('image','rm',name+':local',check=False)

if __name__=='__main__':main()
