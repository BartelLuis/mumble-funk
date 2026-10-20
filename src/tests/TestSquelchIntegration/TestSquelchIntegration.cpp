// Copyright The Mumble-Funk Contributors. All rights reserved.
// BSD-style license: see LICENSE. Real local server; no external installations.
#include "../TestSimplexIntegration/Harness.h"
#include "AudioSignal.h"
#include <array>
#include <cmath>
#include <memory>
#include <opus.h>
using namespace SimplexTest;
static bool sendOpus(Client &client, const QByteArray &payload, std::uint64_t frame, bool last, bool udp) {
	Mumble::Protocol::AudioData audio;
	audio.frameNumber=frame; audio.isLastFrame=last; audio.targetOrContext=0;
	audio.payload={reinterpret_cast<const Mumble::Protocol::byte *>(payload.constData()),std::size_t(payload.size())};
	Mumble::Protocol::UDPAudioEncoder<Mumble::Protocol::Role::Client> encoder(client.version);
	const auto encoded=encoder.encodeAudioPacket(audio);
	const QByteArray bytes(reinterpret_cast<const char *>(encoded.data()),int(encoded.size()));
	return udp ? client.sendEncrypted(bytes) : client.sendRaw(Type::UDPTunnel,bytes);
}
static QList<Received> receiveUDP(Client &client,int count) {
	QList<Received> result; QElapsedTimer timer; timer.start();
	while (timer.elapsed()<2000 && result.size()<count) {
		if (!client.udp.hasPendingDatagrams()) client.udp.waitForReadyRead(10);
		while (client.udp.hasPendingDatagrams()) {
			QByteArray encrypted(int(client.udp.pendingDatagramSize()),'\0');
			if (client.udp.readDatagram(encrypted.data(),encrypted.size()) != encrypted.size() || encrypted.size()<5) return {};
			QByteArray plain(encrypted.size()-4,'\0');
			if (!client.crypt.decrypt(reinterpret_cast<const unsigned char *>(encrypted.constData()),
				reinterpret_cast<unsigned char *>(plain.data()),unsigned(encrypted.size()))) return {};
			Mumble::Protocol::UDPDecoder<Mumble::Protocol::Role::Client> decoder(client.version);
			if (!decoder.decode({reinterpret_cast<const Mumble::Protocol::byte *>(plain.constData()),std::size_t(plain.size())})) return {};
			if (decoder.getMessageType()!=Mumble::Protocol::UDPMessageType::Audio) continue;
			const auto a=decoder.getAudioData();
			result.append({a.senderSession,a.frameNumber,a.isLastFrame,
				QByteArray(reinterpret_cast<const char *>(a.payload.data()),int(a.payload.size()))});
		}
	}
	return result;
}

static QString asset(const char *name) { return QStringLiteral(SQUELCH_ASSET_DIR) + "/" + name; }
static QByteArray settings() {
	return "audiofeedback=true\nsquelch=true\nsquelchopenfile=" + asset("squelch-open.wav").toUtf8()
		+ "\nsquelchclosefile=" + asset("squelch-close.wav").toUtf8() + "\n";
}
static std::vector<QByteArray> speech() {
	std::vector<float> pcm(960*8);
	for (std::size_t i=0;i<pcm.size();++i) {
		const double t=double(i)/48000;
		// Deterministic voiced harmonics + syllabic envelope, not a human recording.
		pcm[i]=float(.12*(.6+.4*std::cos(2*3.141592653589793*7*t))
			*(std::sin(2*3.141592653589793*233*t)+.25*std::sin(2*3.141592653589793*877*t)));
	}
	return AudioSignal::encode(pcm,100);
}
static QList<Received> from(const QList<Received> &all,unsigned session) {
	QList<Received> result; for (const auto &p:all) if (p.session==session) result.append(p); return result;
}
class TestSquelchIntegration:public QObject {
	Q_OBJECT
private slots:
	void transport_data(); void transport();
	void disabled_data(); void disabled();
	void boundaries_data(); void boundaries();
	void reusedSession();
	void rapidHandoff_data(); void rapidHandoff();
};
void TestSquelchIntegration::transport_data() {
	QTest::addColumn<bool>("modern"); QTest::addColumn<bool>("udpIn"); QTest::addColumn<bool>("udpOut");
	for (bool modern:{false,true}) for(bool in:{false,true}) for(bool out:{false,true})
		QTest::newRow(qPrintable(QString("%1-in-%2-out-%3").arg(modern ? "protobuf":"legacy").arg(in?"UDP":"TCP").arg(out?"UDP":"TCP"))) << modern << in << out;
}
void TestSquelchIntegration::transport() {
	QFETCH(bool,modern); QFETCH(bool,udpIn); QFETCH(bool,udpOut);
	Fixture f; QVERIFY(f.start("true",5000,5000,true,settings()));
	Client admin,a,b,observer,outsider;
	QVERIFY(admin.login(f.port,"SuperUser",modern)); QVERIFY(a.login(f.port,"Speaker",modern));
	QVERIFY(b.login(f.port,"Rejected",modern)); QVERIFY(observer.login(f.port,"Listener",modern));
	QVERIFY(outsider.login(f.port,"OtherChannel",modern));
	const auto other=channel(admin,"Other"); QVERIFY(other); QVERIFY(outsider.move(other));
	if (udpIn) { QVERIFY(a.udp.bind(QHostAddress(QHostAddress::LocalHost),0)); QVERIFY(a.udpBarrier()); }
	if (udpOut) { QVERIFY(observer.udp.bind(QHostAddress(QHostAddress::LocalHost),0)); QVERIFY(observer.udpBarrier()); }
	const auto voice=speech();
	for (int i=0;i<4;++i) { QTest::qWait(20); QVERIFY(sendOpus(a,voice[std::size_t(i)],std::uint64_t(i*2),false,udpIn)); }
	if (udpIn) QVERIFY(a.udpBarrier()); else QVERIFY(a.barrier());
	auto received=observer.audio(120); QList<Received> heard=udpOut ? receiveUDP(observer,4) : from(received,a.session);
	auto cues=from(received,observer.session);
	QCOMPARE(heard.size(),4); QCOMPARE(cues.size(),2);
	const auto open=AudioSignal::encodeWavSignal(asset("squelch-open.wav"),50);
	const auto close=AudioSignal::encodeWavSignal(asset("squelch-close.wav"),50);
	QCOMPARE(open.size(),std::size_t(2)); QCOMPARE(close.size(),std::size_t(4));
	for (int i=0;i<2;++i) { QCOMPARE(cues[i].payload,open[std::size_t(i)]); QCOMPARE(cues[i].last,i==1); QCOMPARE(cues[i].frame,std::uint64_t(i*2)); }
	b.audio(100); QVERIFY(b.speak()); QVERIFY(b.barrier());
	const auto busy=b.audio(400); QCOMPARE(busy.size(),13);
	const auto standard=AudioSignal::encode(AudioSignal::syntheticBusy(),50);
	for (int i=0;i<13;++i) { QCOMPARE(busy[i].session,b.session); QCOMPARE(busy[i].payload,standard[std::size_t(i)]); QCOMPARE(busy[i].last,i==12);
		if(i) QCOMPARE(busy[i].frame,busy[i-1].frame+2); }
	for (int i=4;i<8;++i) { QTest::qWait(20); QVERIFY(sendOpus(a,voice[std::size_t(i)],std::uint64_t(i*2),i==7,udpIn)); }
	if (udpIn) QVERIFY(a.udpBarrier()); else QVERIFY(a.barrier());
	received=observer.audio(250); heard+=udpOut ? receiveUDP(observer,4) : from(received,a.session);
	cues+=from(received,observer.session);
	QCOMPARE(heard.size(),8); QCOMPARE(cues.size(),6);
	for(int i=0;i<4;++i) { QCOMPARE(cues[i+2].payload,close[std::size_t(i)]); QCOMPARE(cues[i+2].last,i==3);
		QVERIFY(cues[i+2].frame>cues[i+1].frame); if(i) QCOMPARE(cues[i+2].frame,cues[i+1].frame+2); }
	int error=0;
	std::unique_ptr<OpusDecoder,decltype(&opus_decoder_destroy)> expected(opus_decoder_create(48000,1,&error),opus_decoder_destroy);
	std::unique_ptr<OpusDecoder,decltype(&opus_decoder_destroy)> decoded(opus_decoder_create(48000,1,&error),opus_decoder_destroy);
	QVERIFY(expected && decoded);
	for(int i=0;i<8;++i) {
		QCOMPARE(heard[i].payload,voice[std::size_t(i)]); QCOMPARE(heard[i].frame,std::uint64_t(i*2)); QCOMPARE(heard[i].last,i==7);
		std::array<float,960>x{},y{};
		QCOMPARE(opus_decode_float(expected.get(),reinterpret_cast<const unsigned char*>(voice[std::size_t(i)].data()),voice[std::size_t(i)].size(),x.data(),960,0),960);
		QCOMPARE(opus_decode_float(decoded.get(),reinterpret_cast<const unsigned char*>(heard[i].payload.data()),heard[i].payload.size(),y.data(),960,0),960);
		QVERIFY(x==y); // First and last samples included, independent of the tone decoder.
	}
	// Decode the receiver-session stream continuously as well as speech separately.
	QCOMPARE(opus_decoder_ctl(decoded.get(),OPUS_RESET_STATE),OPUS_OK);
	double energy=0;
	for (const auto &p:cues) {
		std::array<float,960>x{};
		QCOMPARE(opus_decode_float(decoded.get(),reinterpret_cast<const unsigned char*>(p.payload.data()),p.payload.size(),x.data(),960,0),960);
		for(float v:x) { QVERIFY(std::isfinite(v)); QVERIFY(std::abs(v)<.5f); energy+=v*v; }
	}
	QVERIFY(energy>.001);
	QVERIFY(a.audio(100).isEmpty()); QVERIFY(outsider.audio(100).isEmpty());
	QVERIFY(b.speak()); QVERIFY(b.barrier()); QVERIFY(observer.audio(100).isEmpty());
	if(udpOut) { QTest::qWait(40); QVERIFY(!observer.udp.hasPendingDatagrams()); }
	f.server.stop(); QCOMPARE(f.count("denied","not_eligible"),0);
}
void TestSquelchIntegration::disabled_data() {
	QTest::addColumn<QByteArray>("override");
	QTest::newRow("explicit-off") << QByteArray("squelch=false\n");
	QTest::newRow("master-off") << QByteArray("audiofeedback=false\n");
	QTest::newRow("missing-file") << QByteArray("squelchopenfile=/nonexistent/squelch.wav\n");
	QTest::newRow("corrupt-file") << QByteArray("squelchopenfile=/dev/null\n");
}
void TestSquelchIntegration::disabled() {
	QFETCH(QByteArray,override); Fixture f; QVERIFY(f.start("true",5000,5000,true,settings()+override));
	Client a,b; QVERIFY(a.login(f.port,"A")); QVERIFY(b.login(f.port,"B"));
	const auto voice=speech(); QTest::qWait(30); QVERIFY(sendOpus(a,voice[0],0,true,false)); QVERIFY(a.barrier());
	const auto packets=b.audio(200); QCOMPARE(packets.size(),1); QCOMPARE(packets[0].payload,voice[0]); QCOMPARE(packets[0].session,a.session);
	QVERIFY(a.audio().isEmpty());
	// A broken optional WAV must not disable Busy or alter the owner's speech.
	QVERIFY(sendOpus(a,voice[1],2,false,false)); QVERIFY(a.barrier());
	QCOMPARE(from(b.audio(50),a.session).size(),1);
	QVERIFY(b.speak()); QVERIFY(b.barrier());
	const auto busy=b.audio(400);
	QCOMPARE(busy.size(),override.contains("audiofeedback=false")?0:13);
	for(const auto &packet:busy) QCOMPARE(packet.session,b.session);
}
void TestSquelchIntegration::boundaries_data() {
	QTest::addColumn<int>("boundary");
	QTest::newRow("confirmed-empty-marker") << 0;
	QTest::newRow("timeout-omits-close") << 1;
	QTest::newRow("receiver-channel-out-and-back") << 2;
	QTest::newRow("source-channel-out-and-back") << 3;
	QTest::newRow("source-disconnect") << 4;
	QTest::newRow("receiver-deaf-and-return") << 5;
	QTest::newRow("one-packet-transmission") << 6;
}
void TestSquelchIntegration::boundaries() {
	QFETCH(int,boundary); Fixture f; QVERIFY(f.start("true",boundary==1?500:5000,5000,true,settings()+"busytone=false\n"));
	Client admin,a,b; QVERIFY(admin.login(f.port,"SuperUser")); QVERIFY(a.login(f.port,"A")); QVERIFY(b.login(f.port,"B"));
	const auto other=channel(admin,"Other"); QVERIFY(other);
	const auto voice=speech(); QTest::qWait(30); QVERIFY(sendOpus(a,voice[0],0,boundary==6,false)); QVERIFY(a.barrier());
	auto received=b.audio(boundary==6?300:150); auto cues=from(received,b.session);
	QCOMPARE(from(received,a.session).size(),1); QCOMPARE(cues.size(),boundary==6?6:2);
	if(boundary==6) return;
	if(boundary==2) { QVERIFY(b.move(other)); QVERIFY(b.move(0)); }
	if(boundary==3) { QVERIFY(a.move(other)); QVERIFY(a.move(0)); }
	if(boundary==4) { a.socket.abort(); QTest::qWait(100); }
	if(boundary==5) {
		MumbleProto::UserState m; m.set_session(b.session); m.set_self_deaf(true); m.set_self_mute(true);
		QVERIFY(b.send(Type::UserState,m)); QVERIFY(b.barrier()); m.set_self_deaf(false); m.set_self_mute(false);
		QVERIFY(b.send(Type::UserState,m)); QVERIFY(b.barrier());
	}
	if(boundary==1) { QTest::qWait(550); }
	if(boundary!=4) { QVERIFY(sendOpus(a,{},2,true,false)); QVERIFY(a.barrier()); }
	received=b.audio(180); cues=from(received,b.session);
	QCOMPARE(cues.size(),boundary==0?4:0);
	QVERIFY(a.audio().isEmpty());
}
void TestSquelchIntegration::reusedSession() {
	Fixture f; QVERIFY(f.start("true",5000,5000,true,settings()+"busytone=false\nusers=2\n"));
	Client a,b; QVERIFY(a.login(f.port,"A")); QVERIFY(b.login(f.port,"B"));
	const auto voice=speech(); QTest::qWait(30); QVERIFY(sendOpus(a,voice[0],0,false,false)); QVERIFY(a.barrier());
	QCOMPARE(from(b.audio(100),b.session).size(),2);
	const auto session=b.session; b.socket.abort();
	auto removed=[&](unsigned id) { QByteArray data; return a.receive(Type::UserRemove,data,[id](const QByteArray &bytes) {
		MumbleProto::UserRemove m; return m.ParseFromArray(bytes.data(),bytes.size()) && m.session()==id; }); };
	QVERIFY(removed(session)); bool reused=false;
	for(int i=0;i<6;++i) {
		Client replacement; QVERIFY(replacement.login(f.port,"Replacement"));
		if(replacement.session==session) {
			reused=true; QVERIFY(sendOpus(a,voice[1],2,true,false)); QVERIFY(a.barrier());
			const auto packets=replacement.audio(180); QCOMPARE(from(packets,a.session).size(),1);
			QVERIFY(from(packets,replacement.session).isEmpty()); break;
		}
		const auto id=replacement.session; replacement.socket.abort(); QVERIFY(removed(id));
	}
	QVERIFY(reused);
}

void TestSquelchIntegration::rapidHandoff_data() {
	QTest::addColumn<bool>("udp"); QTest::newRow("TCP") << false; QTest::newRow("encrypted-UDP") << true;
}
void TestSquelchIntegration::rapidHandoff() {
	QFETCH(bool,udp); Fixture f; QVERIFY(f.start("true",5000,5000,true,settings()));
	Client a,b,listener; QVERIFY(a.login(f.port,"A",true)); QVERIFY(b.login(f.port,"B",true)); QVERIFY(listener.login(f.port,"Listener",true));
	if(udp) { for(Client *c:{&a,&b}) { QVERIFY(c->udp.bind(QHostAddress(QHostAddress::LocalHost),0)); QVERIFY(c->udpBarrier()); } }
	const auto voice=speech(); QList<Received> expected;
	for(int turn=0;turn<6;++turn) {
		Client &source=turn%2 ? b : a;
		for(int part=0;part<2;++part) {
			QTest::qWait(20); const auto frame=std::uint64_t((turn/2)*4+part*2);
			QVERIFY(sendOpus(source,voice[std::size_t(part)],frame,part==1,udp));
			expected.append({source.session,frame,part==1,voice[std::size_t(part)]});
		}
		if(udp) QVERIFY(source.udpBarrier()); else QVERIFY(source.barrier());
	}
	const auto all=listener.audio(300); QList<Received> heard,cues;
	for(const auto &packet:all) { if(packet.session==listener.session) cues.append(packet); else heard.append(packet); }
	QCOMPARE(heard.size(),expected.size());
	for(int i=0;i<heard.size();++i) { QCOMPARE(heard[i].session,expected[i].session); QCOMPARE(heard[i].frame,expected[i].frame);
		QCOMPARE(heard[i].last,expected[i].last); QCOMPARE(heard[i].payload,expected[i].payload); }
	QVERIFY(!cues.isEmpty());
	int error=0; std::unique_ptr<OpusDecoder,decltype(&opus_decoder_destroy)> decoder(opus_decoder_create(48000,1,&error),opus_decoder_destroy); QVERIFY(decoder);
	for(int i=0;i<cues.size();++i) { if(i) QVERIFY(cues[i].frame>cues[i-1].frame);
		std::array<float,960> pcm{};
		QCOMPARE(opus_decode_float(decoder.get(),reinterpret_cast<const unsigned char*>(cues[i].payload.data()),cues[i].payload.size(),pcm.data(),960,0),960);
		for(float v:pcm) QVERIFY(std::isfinite(v));
	}
	f.server.stop(); QCOMPARE(f.count("granted"),6); QCOMPARE(f.count("denied","not_eligible"),0);
}

QTEST_MAIN(TestSquelchIntegration)
#include "TestSquelchIntegration.moc"
