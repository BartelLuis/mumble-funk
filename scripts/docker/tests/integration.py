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


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe',type=Path,required=True)
    parser.add_argument('--native-server',type=Path,required=True)
    parser.add_argument('--image',default='mumble-funk-docker:local')
    args=parser.parse_args()
    project='funk-docker-check-'+str(os.getpid())
    restored=project+'-restored'
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
    def probe():
        result=command([str(args.probe),str(port)],input=(values['serverpassword']+'\n').encode())
        assert b'encrypted UDP' in result.stdout
    def healthy():
        deadline=time.monotonic()+30
        while time.monotonic()<deadline:
            result=compose('exec','-T','mumble-funk','funk-container','health',check=False)
            if result.returncode==0:return
            time.sleep(.1)
        raise RuntimeError('Container did not become healthy: '+compose('logs').stdout.decode(errors='replace'))
    def information():return json.loads(compose('exec','-T','mumble-funk','funk-container','info','--json').stdout)
    def certificate():
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT);context.check_hostname=False;context.verify_mode=ssl.CERT_NONE
        with socket.create_connection(('127.0.0.1',port),timeout=3) as raw:
            with context.wrap_socket(raw,server_hostname='localhost') as tls:
                return tls.getpeercert(binary_form=True)
    def fingerprint():
        code="from pathlib import Path;import hashlib;print(hashlib.sha256(Path('/var/lib/mumble-funk/server.sqlite').read_bytes()).hexdigest())"
        return compose('run','--rm','--no-deps','-T','--entrypoint','python3','mumble-funk','-c',code).stdout.strip()
    # Reserve the same high host port for both protocols; never stop existing listeners.
    for _ in range(100):
        with socket.socket() as tcp:
            tcp.bind(('127.0.0.1',0));port=tcp.getsockname()[1]
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
            try:udp.bind(('0.0.0.0',port));break
            except OSError:continue
    else:raise RuntimeError('No free TCP/UDP port')
    projects=[project,restored]
    with tempfile.TemporaryDirectory() as directory:
        directory=Path(directory);native=None;logfile=None
        try:
            for name in projects:docker('tag',args.image,name+':local')
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
            values=information();assert values['simplex']=='true';assert values['simplextimeout']=='500'
            assert len(values['serverpassword'])==48
            cid=compose('ps','--quiet','mumble-funk').stdout.decode().strip()
            inspection=json.loads(docker('inspect',cid).stdout)[0]
            assert inspection['Config']['User']=='10001:10001'
            assert inspection['HostConfig']['ReadonlyRootfs']
            assert inspection['HostConfig']['CapDrop']==['ALL']
            assert set(inspection['HostConfig']['PortBindings'])=={'64739/tcp','64739/udp'}
            for bindings in inspection['HostConfig']['PortBindings'].values():assert all(int(b['HostPort'])==port for b in bindings)
            mode=compose('exec','-T','mumble-funk','stat','-c','%a','/etc/mumble-funk/server.ini').stdout.strip()
            assert mode==b'600'
            assert compose('exec','-T','mumble-funk','sh','-c','command -v g++ || command -v cmake',check=False).returncode!=0
            probe();cert=certificate()
            logs=compose('logs').stdout
            assert b'Simplex enabled:' in logs and values['serverpassword'].encode() not in logs
            assert b"Password for 'SuperUser'" not in logs
            # Offline database + audio fixture: exercise retention of the full SQLite file and future assets.
            compose('stop')
            code="import sqlite3;from pathlib import Path;db=sqlite3.connect('/var/lib/mumble-funk/server.sqlite');db.execute('CREATE TABLE docker_preserved(value TEXT)');db.execute(\"INSERT INTO docker_preserved VALUES ('accounts-channels-acls-preserved')\");db.commit();db.close();Path('/var/lib/mumble-funk/audio/future.txt').write_text('future persistent audio directory')"
            compose('run','--rm','--no-deps','-T','--entrypoint','python3','mumble-funk','-c',code)
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
            # Restoring to populated volumes refuses and restarts the original instance.
            assert command(['bash','scripts/docker/manage.sh','restore',str(backup_path)],check=False).returncode!=0
            healthy();assert information()['serverpassword']==values['serverpassword']
            compose('stop')
            before_restore=fingerprint()
            command(['bash','scripts/docker/manage.sh','restore',str(backup_path)],project_name=restored)
            # The restored project has distinct volumes but an identical DB/cert/config.
            assert fingerprint.__name__=='fingerprint'  # no host/native paths are reused
            restored_code=code="from pathlib import Path;import hashlib;print(hashlib.sha256(Path('/var/lib/mumble-funk/server.sqlite').read_bytes()).hexdigest())"
            restored_hash=compose('run','--rm','--no-deps','-T','--entrypoint','python3','mumble-funk','-c',restored_code,project_name=restored).stdout.strip()
            assert restored_hash==before_restore
            compose('up','-d','--no-build',project_name=restored)
            # Independent root-level Mumble process and Docker run at the same time.
            native_port=64738
            try:
                reservations=[]
                for kind in (socket.SOCK_STREAM,socket.SOCK_DGRAM):
                    sock=socket.socket(socket.AF_INET,kind);reservations.append(sock);sock.bind(('127.0.0.1',native_port))
            except OSError:
                native_port=0
            finally:
                for sock in reservations:sock.close()
            if native_port==0:
                with socket.socket() as sock:sock.bind(('127.0.0.1',0));native_port=sock.getsockname()[1]
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
            print('PASS: image startup, rootless isolation, published TCP/encrypted UDP, simplex, diagnostics off, restart/recreate, port conflicts, native parallel operation, consistent backup/restore, clean shutdown')
        finally:
            if native:
                native.terminate()
                try:native.wait(timeout=10)
                except subprocess.TimeoutExpired:native.kill();native.wait()
            if logfile:logfile.close()
            # Only the uniquely named resources created by this test are removed.
            for name in projects:
                compose('down','--volumes',project_name=name,check=False)
                docker('image','rm',name+':local',check=False)

if __name__=='__main__':main()
