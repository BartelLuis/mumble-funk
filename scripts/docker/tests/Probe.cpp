// Copyright The Mumble-Funk Contributors. All rights reserved.
// Use of this source code is governed by the BSD-style license in LICENSE.
#include "../../../src/tests/TestSimplexIntegration/Harness.h"
#include <iostream>
#include <cstdlib>
using namespace SimplexTest;

bool login(Client &client, quint16 port, const std::string &name, const std::string &password) {
 client.port=port;
 client.version=Version::fromComponents(1,5,0);
 QObject::connect(&client.socket,QOverload<const QList<QSslError> &>::of(&QSslSocket::sslErrors),[&client] {client.socket.ignoreSslErrors();});
 client.socket.connectToHostEncrypted(QStringLiteral("127.0.0.1"),port);
 if(!client.socket.waitForEncrypted(5000)) return false;
 MumbleProto::Version version; version.set_version_v1(0x10500);
 MumbleProto::Authenticate auth; auth.set_username(name); auth.set_password(password); auth.set_opus(true);
 QByteArray bytes;
 if(!client.send(Type::Version,version) || !client.send(Type::Authenticate,auth) || !client.receive(Type::ServerSync,bytes)) return false;
 MumbleProto::ServerSync sync;
 if(!sync.ParseFromArray(bytes.data(),bytes.size())) return false;
 client.session=sync.session();
 return client.crypt.isValid();
}
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv);
 if(argc<2) return 2;
 std::string password; std::getline(std::cin,password);
 const auto port=static_cast<quint16>(QString::fromLocal8Bit(argv[1]).toUInt());
 auto check=[](bool ok,const char *message) {if(!ok) {std::cerr<<message<<std::endl;std::exit(1);}};
 Client a,b,observer;
 check(login(a,port,"DockerAlice",password),"TCP/TLS login A failed");
 check(login(b,port,"DockerBob",password),"TCP/TLS login B failed");
 check(login(observer,port,"DockerObserver",password),"TCP/TLS observer login failed");
 check(a.speak(),"TCP send failed"); check(observer.expect(a),"TCP voice not forwarded"); b.audio();
 check(b.speak(true) && b.udpBarrier(),"Encrypted UDP voice/ping failed");
 check(observer.expect(b,false),"Blocked UDP speech leaked");
 check(a.speak(false,true) && observer.expect(a,true,true),"Owner terminator not forwarded");
 check(b.speak() && observer.expect(b,false),"Rejected sender automatically took over");
 check(b.speak(false,true) && b.barrier(),"Rejected end marker failed");
 check(b.speak(true) && b.udpBarrier() && observer.expect(b),"Encrypted UDP voice did not acquire released channel");
 check(b.speak(true,true) && b.udpBarrier() && observer.expect(b,true,true),"UDP terminator failed");
 std::cout<<"Published TCP + encrypted UDP speech and Simplex verified"<<std::endl;
 return 0;
}
