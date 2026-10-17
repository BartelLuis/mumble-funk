// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file at the root of the Mumble source tree.

#include "Mumble.pb.h"
#include "MumbleProtocol.h"
#include "SelfSignedCertificate.h"
#include "VoiceDiagnostics.h"
#include "crypto/CryptStateOCB2.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtCore/QtEndian>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QUdpSocket>
#include <QtTest/QtTest>

#include <atomic>
#include <thread>

namespace {
VoiceDiagnosticRecord packet(std::int64_t time, std::uint64_t frame, bool last = false) {
	VoiceDiagnosticRecord record;
	record.connection   = 1;
	record.session      = 7;
	record.channel      = 2;
	record.userName     = QStringLiteral("Alice");
	record.payloadBytes = 20;
	record.timeUs       = time;
	record.frame        = frame;
	record.last         = last;
	return record;
}

QList< QJsonObject > readEvents(const QString &path) {
	QFile file(path);
	QList< QJsonObject > events;
	if (file.open(QIODevice::ReadOnly)) {
		while (!file.atEnd()) {
			const QByteArray line = file.readLine();
			if (line.endsWith('\n')) {
				events.append(QJsonDocument::fromJson(line).object());
			}
		}
	}
	return events;
}

int countEvent(const QList< QJsonObject > &events, const QString &name) {
	int count = 0;
	for (const QJsonObject &event : events) {
		if (event.value(QStringLiteral("event")) == name) {
			++count;
		}
	}
	return count;
}

class ServerProcess : public QProcess {
public:
	~ServerProcess() override {
		if (state() != QProcess::NotRunning) {
			terminate();
			if (!waitForFinished(3000)) {
				kill();
				waitForFinished(3000);
			}
		}
	}
};

class Client {
public:
	QSslSocket socket;
	CryptStateOCB2 crypt;
	unsigned int session = 0;

	bool send(Mumble::Protocol::TCPMessageType type, const google::protobuf::Message &message) {
		return sendRaw(type, QByteArray::fromStdString(message.SerializeAsString()));
	}
	bool sendRaw(Mumble::Protocol::TCPMessageType type, const QByteArray &payload) {
		QByteArray header(6, '\0');
		qToBigEndian< quint16 >(static_cast< quint16 >(type), header.data());
		qToBigEndian< quint32 >(static_cast< quint32 >(payload.size()), header.data() + 2);
		const QByteArray message = header + payload;
		return socket.write(message) == message.size() && socket.flush();
	}
	bool bytes(int count) {
		QElapsedTimer timer;
		timer.start();
		while (socket.bytesAvailable() < count) {
			if (timer.elapsed() > 5000 || socket.state() == QAbstractSocket::UnconnectedState) {
				return false;
			}
			socket.waitForReadyRead(50);
		}
		return true;
	}
	bool receive(Mumble::Protocol::TCPMessageType wanted, QByteArray &payload) {
		QElapsedTimer timer;
		timer.start();
		while (timer.elapsed() < 5000 && bytes(6)) {
			const QByteArray header = socket.read(6);
			const quint16 type      = qFromBigEndian< quint16 >(header.constData());
			const quint32 length    = qFromBigEndian< quint32 >(header.constData() + 2);
			if (length > 1000000 || !bytes(static_cast< int >(length))) {
				return false;
			}
			payload = socket.read(length);
			if (type == static_cast< quint16 >(Mumble::Protocol::TCPMessageType::CryptSetup)) {
				MumbleProto::CryptSetup setup;
				if (!setup.ParseFromArray(payload.data(), payload.size())
					|| !crypt.setKey(setup.key(), setup.client_nonce(), setup.server_nonce())) {
					return false;
				}
			}
			if (type == static_cast< quint16 >(wanted)) {
				return true;
			}
		}
		return false;
	}
	bool connect(quint16 port) {
		QObject::connect(&socket, QOverload< const QList< QSslError > & >::of(&QSslSocket::sslErrors),
						 [&]() { socket.ignoreSslErrors(); });
		socket.connectToHostEncrypted(QStringLiteral("127.0.0.1"), port);
		return socket.waitForEncrypted(5000);
	}
	bool login(quint16 port, const std::string &name) {
		if (!connect(port)) {
			return false;
		}
		MumbleProto::Version version;
		version.set_version_v1(0x10205);
		MumbleProto::Authenticate auth;
		auth.set_username(name);
		auth.set_opus(true);
		QByteArray data;
		if (!send(Mumble::Protocol::TCPMessageType::Version, version)
			|| !send(Mumble::Protocol::TCPMessageType::Authenticate, auth)
			|| !receive(Mumble::Protocol::TCPMessageType::ServerSync, data)) {
			return false;
		}
		MumbleProto::ServerSync sync;
		if (!sync.ParseFromArray(data.data(), data.size())) {
			return false;
		}
		session = sync.session();
		return crypt.isValid();
	}
};
} // namespace

class TestVoiceDiagnostics : public QObject {
	Q_OBJECT
private slots:
	void markerAndGaps();
	void timeoutAndRestart();
	void continuousStream();
	void contextsAndLateMarkers();
	void endOnly();
	void writerAndIdleTimeout();
	void boundedConcurrentQueue();
	void rotationAndFileErrors();
	void forwarding_data();
	void forwarding();
};

void TestVoiceDiagnostics::markerAndGaps() {
	QList< QJsonObject > events;
	VoiceDiagnosticTracker tracker(500000, [&](QJsonObject event) { events.append(event); });
	tracker.process(packet(0, 0));
	VoiceDiagnosticRecord second = packet(20000, 2);
	second.transport             = VoiceDiagnosticRecord::Transport::TCP;
	tracker.process(second);
	tracker.process(packet(40000, 4, true));
	tracker.expire(1000000);
	QCOMPARE(countEvent(events, "stream_start"), 1);
	QCOMPARE(countEvent(events, "packet"), 3);
	QCOMPARE(countEvent(events, "end_marker"), 1);
	QCOMPARE(countEvent(events, "stream_end_marker"), 1);
	QCOMPARE(countEvent(events, "stream_end_timeout"), 0);
	QCOMPARE(events.at(2).value("gap_us").toString(), QStringLiteral("20000"));
	QCOMPARE(events.at(2).value("transport").toString(), QStringLiteral("tcp"));
	QCOMPARE(events.at(2).value("channel").toInt(), 2);
	QCOMPARE(events.at(2).value("session").toInt(), 7);
	QCOMPARE(events.at(2).value("user").toString(), QStringLiteral("Alice"));
}

void TestVoiceDiagnostics::timeoutAndRestart() {
	QList< QJsonObject > events;
	VoiceDiagnosticTracker tracker(1000, [&](QJsonObject event) { events.append(event); });
	tracker.process(packet(100, 100));
	tracker.expire(1099);
	QCOMPARE(countEvent(events, "stream_end_timeout"), 0);
	tracker.expire(1100);
	tracker.expire(1150);
	QCOMPARE(countEvent(events, "stream_end_timeout"), 1);
	QCOMPARE(events.last().value("reason").toString(), QStringLiteral("suspected_only"));
	QCOMPARE(events.last().value("time_us").toString(), QStringLiteral("1100"));
	tracker.process(packet(1200, 0, true));
	QCOMPARE(countEvent(events, "stream_start"), 2);
	QCOMPARE(countEvent(events, "stream_end_marker"), 1);
	QCOMPARE(events.at(4).value("gap_us").toString(), QStringLiteral("1100"));
}

void TestVoiceDiagnostics::continuousStream() {
	QList< QJsonObject > events;
	VoiceDiagnosticTracker tracker(500000, [&](QJsonObject event) { events.append(event); });
	for (int i = 0; i < 1500; ++i) {
		const std::int64_t time = static_cast< std::int64_t >(i) * 20000;
		tracker.process(packet(time, static_cast< std::uint64_t >(i * 2)));
		tracker.expire(time + 19000);
	}
	QCOMPARE(countEvent(events, "stream_start"), 1);
	QCOMPARE(countEvent(events, "packet"), 1500);
	QCOMPARE(countEvent(events, "stream_end_timeout"), 0);
	tracker.expire(1499 * 20000 + 500000);
	QCOMPARE(countEvent(events, "stream_end_timeout"), 1);
}

void TestVoiceDiagnostics::contextsAndLateMarkers() {
	QList< QJsonObject > events;
	VoiceDiagnosticTracker tracker(500000, [&](QJsonObject event) { events.append(event); });
	tracker.process(packet(0, 10));
	tracker.process(packet(100, 2, true));
	QCOMPARE(countEvent(events, "end_marker"), 1);
	QCOMPARE(countEvent(events, "stream_end_marker"), 0);
	VoiceDiagnosticRecord changed = packet(200, 12);
	changed.channel               = 3;
	changed.kind                  = VoiceDiagnosticRecord::Kind::ChannelChanged;
	tracker.process(changed);
	QCOMPARE(countEvent(events, "stream_interrupted"), 1);
	tracker.process(packet(300, 0));
	VoiceDiagnosticRecord other = packet(400, 0, true);
	other.connection            = 2;
	tracker.process(other);
	QCOMPARE(countEvent(events, "stream_start"), 3);
	changed.kind   = VoiceDiagnosticRecord::Kind::Disconnected;
	changed.timeUs = 500;
	tracker.process(changed);
	tracker.expire(1000000);
	QCOMPARE(countEvent(events, "stream_end_timeout"), 0);
	QCOMPARE(countEvent(events, "disconnected"), 1);
}

void TestVoiceDiagnostics::endOnly() {
	QList< QJsonObject > events;
	VoiceDiagnosticTracker tracker(500000, [&](QJsonObject event) { events.append(event); });
	VoiceDiagnosticRecord end = packet(0, 0, true);
	end.payloadBytes          = 0;
	tracker.process(end);
	QCOMPARE(countEvent(events, "stream_start"), 0);
	QCOMPARE(countEvent(events, "stream_end_marker"), 0);
	QCOMPARE(countEvent(events, "end_marker"), 1);
	tracker.process(packet(1, 1, true));
	QCOMPARE(countEvent(events, "stream_start"), 1);
	QCOMPARE(countEvent(events, "stream_end_marker"), 1);
}

void TestVoiceDiagnostics::writerAndIdleTimeout() {
	QTemporaryDir directory;
	VoiceDiagnostics::Config config;
	config.filePath  = directory.filePath("voice.jsonl");
	config.timeoutMs = 50;
	{
		VoiceDiagnostics diagnostics(config);
		QTRY_VERIFY(diagnostics.submit(packet(0, 0)));
		QTRY_COMPARE(countEvent(readEvents(config.filePath), "stream_end_timeout"), 1);
	}
	const QList< QJsonObject > events = readEvents(config.filePath);
	QCOMPARE(countEvent(events, "diagnostics_started"), 1);
	QCOMPARE(countEvent(events, "diagnostics_stopped"), 1);
	for (const QJsonObject &event : events) {
		QVERIFY(!event.isEmpty());
		QVERIFY(!event.contains("payload"));
		QVERIFY(!event.contains("audio"));
	}
}

void TestVoiceDiagnostics::boundedConcurrentQueue() {
	QTemporaryDir directory;
	VoiceDiagnostics::Config config;
	config.filePath      = directory.filePath("voice.jsonl");
	config.queueCapacity = 1;
	std::atomic< int > accepted{ 0 };
	{
		VoiceDiagnostics diagnostics(config);
		const auto produce = [&]() {
			for (int i = 0; i < 1000; ++i) {
				if (diagnostics.submit(packet(0, static_cast< std::uint64_t >(i)))) {
					++accepted;
				}
			}
		};
		std::thread udp(produce);
		std::thread tcp(produce);
		udp.join();
		tcp.join();
	}
	const QList< QJsonObject > events = readEvents(config.filePath);
	QCOMPARE(countEvent(events, "packet"), accepted.load());
	std::uint64_t dropped = 0;
	for (const QJsonObject &event : events) {
		if (event.value("event") == "diagnostic_loss") {
			dropped += event.value("dropped_records").toString().toULongLong();
		}
	}
	QCOMPARE(dropped + static_cast< std::uint64_t >(accepted.load()), std::uint64_t(2000));
	QCOMPARE(events.last().value("dropped_records").toString().toULongLong(), static_cast< qulonglong >(dropped));
}

void TestVoiceDiagnostics::rotationAndFileErrors() {
	QTemporaryDir directory;
	VoiceDiagnostics::Config config;
	config.filePath = directory.filePath("voice.jsonl");
	config.maxBytes = 4096;
	{
		VoiceDiagnostics diagnostics(config);
		for (int i = 0; i < 100; ++i) {
			// Retry in the test only; the server never waits for diagnostic capacity.
			while (!diagnostics.submit(packet(0, static_cast< std::uint64_t >(i)))) {
				std::this_thread::yield();
			}
		}
	}
	QVERIFY(QFile::exists(config.filePath + ".1"));
	QVERIFY(QFile(config.filePath).size() <= config.maxBytes);
	QVERIFY(QFile(config.filePath + ".1").size() <= config.maxBytes);
	QVERIFY(!readEvents(config.filePath).isEmpty());
	QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Voice diagnostics: cannot open .*"));
	config.filePath = directory.filePath("missing/voice.jsonl");
	{ VoiceDiagnostics diagnostics(config); }
	QVERIFY(!QFile::exists(config.filePath));
}

void TestVoiceDiagnostics::forwarding_data() {
	QTest::addColumn< QString >("setting");
	QTest::newRow("default-disabled") << QString();
	QTest::newRow("explicitly-disabled") << QStringLiteral("false");
	QTest::newRow("enabled") << QStringLiteral("true");
}

void TestVoiceDiagnostics::forwarding() {
	QFETCH(QString, setting);
	const bool enabled = setting == QStringLiteral("true");
	QTemporaryDir directory;
	QTcpServer reservation;
	QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
	const quint16 port = reservation.serverPort();
	reservation.close();
	QFile config(directory.filePath("server.ini"));
	QVERIFY(config.open(QIODevice::WriteOnly));
	QByteArray ini = "host=127.0.0.1\nport=" + QByteArray::number(port)
					 + "\ndatabase=" + directory.filePath("server.sqlite").toUtf8()
					 + "\nlogfile=\nice=\nbonjour=false\nusers=10\nbandwidth=72000\nkdfiterations=1000\n"
					 + "voicediagnosticsfile=" + directory.filePath("capture").toUtf8()
					 + "\nvoicediagnosticstimeout=100\n";
	if (!setting.isEmpty()) {
		ini += "voicediagnostics=" + setting.toUtf8() + '\n';
	}
	QCOMPARE(config.write(ini), static_cast< qint64 >(ini.size()));
	config.close();
	ServerProcess server;
	server.setProcessChannelMode(QProcess::MergedChannels);
	server.start(QStringLiteral(MUMBLE_SERVER_PATH), { "-ini", config.fileName(), "-fg" });
	QVERIFY(server.waitForStarted());
	QByteArray startup;
	QElapsedTimer timer;
	timer.start();
	while (!startup.contains("Server listening on") && timer.elapsed() < 10000) {
		server.waitForReadyRead(50);
		startup += server.readAll();
	}
	QVERIFY2(startup.contains("Server listening on"), startup.constData());
	Client sender;
	Client receiver;
	QSslCertificate certificate;
	QSslKey key;
	QVERIFY(SelfSignedCertificate::generateMumbleCertificate(QStringLiteral("DiagnosticSender"), QString(), certificate,
															 key));
	sender.socket.setLocalCertificate(certificate);
	sender.socket.setPrivateKey(key);
	QVERIFY(sender.login(port, "DiagnosticSender"));
	QVERIFY(receiver.login(port, "DiagnosticReceiver"));
	// Default ACLs allow registered users to create temporary channels.
	MumbleProto::UserState registration;
	registration.set_session(sender.session);
	registration.set_user_id(0);
	QVERIFY(sender.send(Mumble::Protocol::TCPMessageType::UserState, registration));
	QByteArray registered;
	do {
		QVERIFY(sender.receive(Mumble::Protocol::TCPMessageType::UserState, registered));
		QVERIFY(registration.ParseFromArray(registered.data(), registered.size()));
	} while (registration.session() != sender.session || !registration.has_user_id());
	QVERIFY(registration.user_id() > 0);
	Mumble::Protocol::UDPAudioEncoder< Mumble::Protocol::Role::Client > encoder(Version::fromComponents(1, 2, 5));
	Mumble::Protocol::UDPDecoder< Mumble::Protocol::Role::Client > decoder(Version::fromComponents(1, 2, 5));
	const QByteArray secret("DIAGNOSTIC_AUDIO_SECRET");
	Mumble::Protocol::AudioData audio;
	audio.payload = gsl::span< const Mumble::Protocol::byte >(
		reinterpret_cast< const Mumble::Protocol::byte * >(secret.constData()),
		static_cast< std::size_t >(secret.size()));
	QUdpSocket udp;
	QVERIFY(udp.bind(QHostAddress(QHostAddress::LocalHost), 0));
	const QString output = directory.filePath("capture.1.jsonl");
	Client unauthenticated;
	QVERIFY(unauthenticated.connect(port));
	const auto probe = encoder.encodeAudioPacket(audio);
	QVERIFY(unauthenticated.sendRaw(
		Mumble::Protocol::TCPMessageType::UDPTunnel,
		QByteArray(reinterpret_cast< const char * >(probe.data()), static_cast< int >(probe.size()))));
	unsigned int newChannel = 0;
	for (int i = 0; i < 5; ++i) {
		if (i == 4) {
			if (enabled) {
				QTRY_COMPARE(countEvent(readEvents(output), "stream_end_timeout"), 1);
			}
			MumbleProto::ChannelState channel;
			channel.set_parent(0);
			channel.set_name("DiagnosticChannel");
			channel.set_temporary(true);
			QVERIFY(sender.send(Mumble::Protocol::TCPMessageType::ChannelState, channel));
			QByteArray data;
			QVERIFY(sender.receive(Mumble::Protocol::TCPMessageType::ChannelState, data));
			QVERIFY(channel.ParseFromArray(data.data(), data.size()));
			newChannel = channel.channel_id();
			QVERIFY(newChannel != 0);
			MumbleProto::UserState move;
			move.set_session(receiver.session);
			move.set_channel_id(newChannel);
			QVERIFY(receiver.send(Mumble::Protocol::TCPMessageType::UserState, move));
			do {
				QVERIFY(receiver.receive(Mumble::Protocol::TCPMessageType::UserState, data));
				QVERIFY(move.ParseFromArray(data.data(), data.size()));
			} while (move.session() != receiver.session || !move.has_channel_id() || move.channel_id() != newChannel);
		}
		audio.frameNumber  = static_cast< std::uint64_t >(i * 2);
		audio.isLastFrame  = i == 2 || i == 4;
		const auto encoded = encoder.encodeAudioPacket(audio);
		QByteArray bytes(reinterpret_cast< const char * >(encoded.data()), static_cast< int >(encoded.size()));
		if (i == 1) {
			QByteArray encrypted(bytes.size() + 4, '\0');
			QVERIFY(sender.crypt.encrypt(reinterpret_cast< const unsigned char * >(bytes.constData()),
										 reinterpret_cast< unsigned char * >(encrypted.data()),
										 static_cast< unsigned int >(bytes.size())));
			QCOMPARE(udp.writeDatagram(encrypted, QHostAddress::LocalHost, port),
					 static_cast< qint64 >(encrypted.size()));
		} else {
			QVERIFY(sender.sendRaw(Mumble::Protocol::TCPMessageType::UDPTunnel, bytes));
		}
		QByteArray forwarded;
		QVERIFY(receiver.receive(Mumble::Protocol::TCPMessageType::UDPTunnel, forwarded));
		QVERIFY(decoder.decode(gsl::span< const Mumble::Protocol::byte >(
			reinterpret_cast< const Mumble::Protocol::byte * >(forwarded.constData()),
			static_cast< std::size_t >(forwarded.size()))));
		const Mumble::Protocol::AudioData received = decoder.getAudioData();
		QCOMPARE(received.senderSession, sender.session);
		QCOMPARE(received.frameNumber, audio.frameNumber);
		QCOMPARE(received.isLastFrame, audio.isLastFrame);
		QCOMPARE(QByteArray(reinterpret_cast< const char * >(received.payload.data()),
							static_cast< int >(received.payload.size())),
				 secret);
		QTest::qWait(20);
	}
	if (enabled) {
		QTRY_COMPARE(countEvent(readEvents(output), "stream_end_timeout"), 1);
		sender.socket.disconnectFromHost();
		QTRY_COMPARE(countEvent(readEvents(output), "disconnected"), 1);
		const QList< QJsonObject > events = readEvents(output);
		QCOMPARE(countEvent(events, "packet"), 5);
		QCOMPARE(countEvent(events, "stream_start"), 3);
		QCOMPARE(countEvent(events, "stream_end_marker"), 2);
		QVERIFY(countEvent(events, "channel_changed") >= 2);
		int udpPackets = 0;
		int tcpPackets = 0;
		for (const QJsonObject &event : events) {
			if (event.value("event") == "packet") {
				QCOMPARE(event.value("session").toInt(), static_cast< int >(sender.session));
				QCOMPARE(event.value("channel").toInt(),
						 event.value("frame").toString() == "8" ? static_cast< int >(newChannel) : 0);
				QCOMPARE(event.value("user").toString(), QStringLiteral("DiagnosticSender"));
				if (event.value("frame").toString() == "8") {
					QCOMPARE(event.value("channel_name_at_join").toString(), QStringLiteral("DiagnosticChannel"));
				}
				udpPackets += event.value("transport") == "udp" ? 1 : 0;
				tcpPackets += event.value("transport") == "tcp" ? 1 : 0;
			}
		}
		QCOMPARE(udpPackets, 1);
		QCOMPARE(tcpPackets, 4);
		QFile file(output);
		QVERIFY(file.open(QIODevice::ReadOnly));
		QVERIFY(!file.readAll().contains(secret));
	} else {
		QVERIFY(!QFile::exists(output));
	}
}

QTEST_GUILESS_MAIN(TestVoiceDiagnostics)
#include "TestVoiceDiagnostics.moc"
