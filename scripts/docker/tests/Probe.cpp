// Copyright The Mumble-Funk Contributors. All rights reserved.
// Use of this source code is governed by the BSD-style license in LICENSE.
#include "../../../src/tests/TestSimplexIntegration/Harness.h"
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <opus.h>
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
 const bool feedback = argc > 2 && QString::fromLocal8Bit(argv[2]) == "--audio-feedback";
 bool validCount=true;
 const int expectedPackets=argc>3?QString::fromLocal8Bit(argv[3]).toInt(&validCount):13;
 check(validCount && expectedPackets>=1 && expectedPackets<=64,"Invalid expected Busy packet count");
 Client a,b,observer;
 check(login(a,port,"DockerAlice",password),"TCP/TLS login A failed");
 check(login(b,port,"DockerBob",password),"TCP/TLS login B failed");
 check(login(observer,port,"DockerObserver",password),"TCP/TLS observer login failed");
 check(a.speak(),"TCP send failed"); check(observer.expect(a),"TCP voice not forwarded"); b.audio();
 check(b.speak(true) && b.udpBarrier(),"Encrypted UDP voice/ping failed");
 check(observer.expect(b,false),"Blocked UDP speech leaked");
 if(feedback) {
  const auto tone=b.audio(std::max(400,expectedPackets*20+400),expectedPackets);
  check(tone.size()==expectedPackets,"Private busy tone missing or wrong duration");
  int error=0;
  std::unique_ptr<OpusDecoder,decltype(&opus_decoder_destroy)> decoder(opus_decoder_create(48000,1,&error),opus_decoder_destroy);
  check(bool(decoder) && error==OPUS_OK,"Cannot initialize feedback decoder");
  for(int i=0;i<tone.size();++i) {
   check(tone[i].session==b.session,"Busy tone has wrong recipient identity");
   check(tone[i].frame==static_cast<std::uint64_t>(i*2),"Busy tone frame sequence changed");
   check(tone[i].last==(i+1==expectedPackets),"Busy tone end marker missing");
   check(!tone[i].payload.isEmpty() && tone[i].payload!=audioContent,"Busy tone payload invalid");
   std::array<float,960> samples{};
   check(opus_decode_float(decoder.get(),reinterpret_cast<const unsigned char*>(tone[i].payload.data()),tone[i].payload.size(),samples.data(),960,0)==960,"Busy tone not decodable");
   for(float value:samples) check(std::isfinite(value) && std::abs(value)<1,"Busy tone decoded level unsafe");
  }
  check(a.audio().isEmpty() && observer.audio().isEmpty(),"Private busy tone leaked to channel");
 }

 check(a.speak(false,true) && observer.expect(a,true,true),"Owner terminator not forwarded");
 check(b.speak() && observer.expect(b,false),"Rejected sender automatically took over");
 check(b.speak(false,true) && b.barrier(),"Rejected end marker failed");
 check(b.speak(true) && b.udpBarrier() && observer.expect(b),"Encrypted UDP voice did not acquire released channel");
 check(b.speak(true,true) && b.udpBarrier() && observer.expect(b,true,true),"UDP terminator failed");
 if(feedback) std::cout<<"Private busy tone verified; ";
 std::cout<<"Published TCP + encrypted UDP speech and Simplex verified"<<std::endl;
 return 0;
}
