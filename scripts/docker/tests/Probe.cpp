// Copyright The Mumble-Funk Contributors. All rights reserved.
// Use of this source code is governed by the BSD-style license in LICENSE.
#include "../../../src/tests/TestSimplexIntegration/Harness.h"
#include "AudioSignal.h"
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

// Exercise the unmodified installer configuration, including its 500-ms lease.
// Expected clips use the shipped WAVs at volume 50 and boost 8, never packet counts
// inferred from the server. Owner keepalives prevent artificial timeout releases.
static void installedAudio(Client &a, Client &b, Client &observer, bool muted) {
 auto check=[](bool ok,const char *message) {if(!ok) {std::cerr<<message<<std::endl;std::exit(1);}};
 auto clip=[&](const char *name,bool busy) {
  const QString path=QStringLiteral(AUDIO_ASSET_DIR)+"/"+name;
  return busy?AudioSignal::encode(AudioSignal::loadWav(path),muted?0:50,8):AudioSignal::encodeWavSignal(path,muted?0:50,8);
 };
 auto verify=[&](const QList<Received> &packets,const std::vector<QByteArray> &expected,unsigned session) {
  if(std::size_t(packets.size())!=expected.size()) std::cerr<<"Audio session "<<session<<": received "<<packets.size()<<", expected "<<expected.size()<<std::endl;
  check(std::size_t(packets.size())==expected.size(),"Installed audio missing or wrong length");
  int error=0;
  std::unique_ptr<OpusDecoder,decltype(&opus_decoder_destroy)> decoder(opus_decoder_create(48000,1,&error),opus_decoder_destroy);
  check(bool(decoder) && error==OPUS_OK,"Installed audio decoder failed");
  for(int i=0;i<packets.size();++i) {
   check(packets[i].session==session && packets[i].payload==expected[std::size_t(i)],"Installed audio identity, volume or boost changed");
   check(packets[i].last==(i+1==packets.size()),"Installed audio terminator changed");
   if(i) check(packets[i].frame==packets[i-1].frame+2,"Installed audio sequence invalid");
   std::array<float,960> pcm{};
   check(opus_decode_float(decoder.get(),reinterpret_cast<const unsigned char*>(packets[i].payload.data()),packets[i].payload.size(),pcm.data(),960,0)==960,"Installed audio undecodable");
   for(float sample:pcm) check(std::isfinite(sample) && std::abs(sample)<1 && (!muted || std::abs(sample)<1e-7f),"Installed audio level unsafe or mute failed");
  }
 };
 std::vector<float> samples(960);
 for(std::size_t i=0;i<samples.size();++i) samples[i]=float(.1*std::sin(2*3.141592653589793*300*double(i)/48000));
 const auto voice=AudioSignal::encode(samples,100).front();
 std::uint64_t sequence=0;
 auto speak=[&](bool udp=false,bool last=false) {
  Mumble::Protocol::AudioData audio;audio.frameNumber=sequence;sequence+=2;audio.isLastFrame=last;audio.targetOrContext=0;
  audio.payload={reinterpret_cast<const Mumble::Protocol::byte*>(voice.data()),std::size_t(voice.size())};
  Mumble::Protocol::UDPAudioEncoder<Mumble::Protocol::Role::Client> encoder(a.version);
  const auto packet=encoder.encodeAudioPacket(audio);
  const QByteArray bytes(reinterpret_cast<const char*>(packet.data()),int(packet.size()));
  return udp?a.sendEncrypted(bytes):a.sendRaw(Type::UDPTunnel,bytes);
 };
 auto cues=[&](const QList<Received> &packets,unsigned session) {
  QList<Received> result;
  for(const auto &packet:packets) {
   if(packet.session==session) result.append(packet);
   else check(packet.session==a.session && packet.payload==voice,"Speech modified or wrong recipient");
  }
  return result;
 };
 // Match the existing real-Opus tests: allow the fresh bandwidth window to age.
 QTest::qWait(30);
 check(speak() && a.barrier(),"Installed owner speech failed");
 const auto first=observer.audio(150,3);
 int voices=0;for(const auto &p:first) if(p.session==a.session) {++voices;check(!p.last && p.payload==voice,"First speech changed");}
 if(voices!=1) {std::cerr<<"First voices="<<voices<<", packets="<<first.size()<<", owner="<<a.session<<", observer="<<observer.session<<std::endl;for(const auto &p:first) std::cerr<<"session="<<p.session<<" frame="<<p.frame<<" last="<<p.last<<std::endl;}
 check(voices==1,"First speech missing");
 const auto open=cues(first,observer.session);verify(open,clip("squelch-open.wav",false),observer.session);
 b.audio();check(b.speak(true) && b.udpBarrier(),"Installed encrypted UDP rejection failed");
 QList<Received> busy;QElapsedTimer timer;timer.start();
 do {
  check(speak(),"Installed owner keepalive failed");
  busy.append(cues(b.audio(20),b.session));
 } while(timer.elapsed()<1600 && (muted || busy.size()<61));
 verify(busy,muted?std::vector<QByteArray>{}:clip("busy-tone.wav",true),b.session);
 check(a.audio().isEmpty() && cues(observer.audio(),observer.session).isEmpty(),"Busy leaked or owner lost Simplex lease");
 check(speak(true,true) && a.udpBarrier(),"Installed encrypted UDP end failed");
 const auto ending=observer.audio(200);
 voices=0;for(const auto &p:ending) if(p.session==a.session) {++voices;check(p.last && p.payload==voice,"Last speech changed");}
 check(voices==1,"Last speech missing");
 const auto close=cues(ending,observer.session);verify(close,clip("squelch-close.wav",false),observer.session);
 check(close.first().frame>open.last().frame,"Close sequence reused Open");
 check(a.audio().isEmpty(),"Feedback sent to original sender");
 std::cout<<"Installed audio defaults verified: Open, Close, Busy, boost8, volume50; TCP/encrypted UDP and Simplex"<<(muted?"; volume0 silent":"")<<std::endl;
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
 if(argc>2 && (QString::fromLocal8Bit(argv[2])=="--audio-defaults" || QString::fromLocal8Bit(argv[2])=="--audio-defaults-muted")) {
  installedAudio(a,b,observer,QString::fromLocal8Bit(argv[2])=="--audio-defaults-muted");return 0;
 }
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
