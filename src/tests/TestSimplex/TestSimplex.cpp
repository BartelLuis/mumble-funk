// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license in the LICENSE file.
#include "SimplexController.h"
#include "SimplexLogger.h"
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTemporaryDir>
#include <QtTest/QtTest>
#include <atomic>
#include <thread>

namespace {
SimplexController::Packet packet(std::uint64_t user, std::uint64_t frame, bool last = false, unsigned int channel = 0) {
	SimplexController::Packet p;
	p.context.connection = user;
	p.context.session    = static_cast< unsigned int >(user);
	p.context.channel    = channel;
	p.context.user       = QStringLiteral("User%1").arg(user);
	p.frame              = frame;
	p.last               = last;
	return p;
}
struct Fixture {
	std::int64_t now = 0;
	std::vector< SimplexController::Event > events;
	SimplexController engine{ 500, 1000, [this](const SimplexController::Event &e) { events.push_back(e); },
							  [this] { return now; } };
	bool send(SimplexController::Packet p) { return engine.begin(p).allowed; }
	int count(const QString &name, const QString &reason = {}) {
		int n = 0;
		for (const auto &e : events)
			if (e.event == name && (reason.isEmpty() || e.reason == reason))
				++n;
		return n;
	}
};
QByteArray readFile(const QString &path) {
	QFile f(path);
	return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
} // namespace

class TestSimplex : public QObject {
	Q_OBJECT
private slots:
	void exclusiveAndIndependent();
	void simultaneousAndForwardingGuard();
	void continuousSilence();
	void blockedNeedsNewAttempt();
	void timeoutAndRapidPresses();
	void staleMarkersAndDuplicatePackets();
	void channelAndPermissionChanges();
	void disconnectAndRestart();
	void alternateTargetsAndEligibility();
	void timeoutBounds();
	void asynchronousLog();
};

void TestSimplex::exclusiveAndIndependent() {
	Fixture f;
	QVERIFY(f.send(packet(1, 0)));
	QVERIFY(!f.send(packet(2, 0)));
	QVERIFY(f.send(packet(3, 0, false, 1)));
	QVERIFY(!f.send(packet(4, 0, false, 1)));
	QVERIFY(f.send(packet(1, 2, true)));
	QVERIFY(f.send(packet(5, 0)));
	QVERIFY(f.send(packet(3, 2, false, 1)));
	QCOMPARE(f.count("granted"), 3);
	QCOMPARE(f.count("released", "marker_confirmed"), 1);
}

void TestSimplex::simultaneousAndForwardingGuard() {
	for (int repeat = 0; repeat < 100; ++repeat) {
		SimplexController engine(500, 1000);
		std::atomic< int > ready{ 0 }, accepted{ 0 };
		auto send = [&](std::uint64_t id) {
			++ready;
			while (ready < 2)
				std::this_thread::yield();
			auto p = packet(id, 0);
			p.udp  = id == 1;
			if (engine.begin(p).allowed)
				++accepted;
		};
		std::thread udp(send, 1), tcp(send, 2);
		udp.join();
		tcp.join();
		QCOMPARE(accepted.load(), 1);
	}
	SimplexController engine(500, 1000);
	std::atomic< bool > entered{ false }, finished{ false };
	auto last = engine.begin(packet(1, 0, true));
	QVERIFY(last.allowed);
	std::thread next([&] {
		entered     = true;
		auto result = engine.begin(packet(2, 0));
		finished    = result.allowed;
	});
	while (!entered)
		std::this_thread::yield();
	QTest::qWait(10);
	const bool overtook = finished.load();
	last.lock.unlock();
	next.join();
	QVERIFY(!overtook);
	QVERIFY(finished.load());
}

void TestSimplex::continuousSilence() {
	Fixture f;
	for (int i = 0; i < 1500; ++i) {
		f.now = i * 20000;
		QVERIFY(f.send(packet(1, static_cast< std::uint64_t >(i * 2))));
		f.engine.expire();
		QVERIFY(!f.send(packet(2, static_cast< std::uint64_t >(i * 2))));
	}
	QCOMPARE(f.count("granted"), 1);
	QCOMPARE(f.count("released"), 0);
	QCOMPARE(f.count("denied"), 1); // No per-packet log storm.
}

void TestSimplex::blockedNeedsNewAttempt() {
	Fixture f;
	QVERIFY(f.send(packet(1, 0)));
	QVERIFY(!f.send(packet(2, 0)));
	QVERIFY(f.send(packet(1, 2, true)));
	for (int i = 1; i <= 100; ++i) {
		f.now += 20000;
		QVERIFY(!f.send(packet(2, static_cast< std::uint64_t >(i * 2))));
		f.engine.expire();
	}
	QVERIFY(!f.send(packet(2, 202, true)));
	QVERIFY(f.send(packet(2, 204)));
	QCOMPARE(f.count("granted"), 2);
	QCOMPARE(f.count("denied_end", "marker_confirmed"), 1);
	QVERIFY(f.events.at(3).packets >= 100);
}

void TestSimplex::timeoutAndRapidPresses() {
	Fixture f;
	QVERIFY(f.send(packet(1, 10)));
	QVERIFY(!f.send(packet(2, 10)));
	f.now = 499999;
	f.engine.expire();
	QCOMPARE(f.count("released"), 0);
	QVERIFY(!f.send(packet(2, 12)));
	f.now = 500000;
	f.engine.expire();
	QCOMPARE(f.count("released", "timeout_suspected"), 1);
	QVERIFY(!f.send(packet(2, 14)));
	// A new physical press cannot be distinguished from continuing packets here.
	f.now = 600000;
	QVERIFY(!f.send(packet(2, 16)));
	QVERIFY(f.send(packet(3, 0)));
	f.now = 1100000;
	f.engine.expire();
	f.now = 1599999;
	QVERIFY(!f.send(packet(2, 18)));
	f.now += 1000000;
	QVERIFY(f.send(packet(2, 0))); // Quiet interval permits sequence restart.
	f.engine.expire();             // No obsolete timeout callback can clear the new lease.
	QVERIFY(!f.send(packet(4, 0)));
	QCOMPARE(f.count("released"), 2);
}

void TestSimplex::staleMarkersAndDuplicatePackets() {
	Fixture f;
	QVERIFY(f.send(packet(1, 10)));
	QVERIFY(f.send(packet(1, 20)));
	QVERIFY(!f.send(packet(1, 10, true)));
	QVERIFY(!f.send(packet(1, 20, true)));  // Duplicate with payload is not a boundary.
	QVERIFY(!f.send(packet(2, 100, true))); // Cannot release user 1's lease.
	QVERIFY(!f.send(packet(2, 102)));
	QCOMPARE(f.count("released"), 0);
	auto end    = packet(1, 20, true);
	end.payload = false;
	QVERIFY(f.send(end));
	QVERIFY(!f.send(end)); // Replayed end-only packet cannot acquire a channel.
	QVERIFY(f.send(packet(3, 0)));
	QVERIFY(!f.send(packet(1, 22, true)));
	QVERIFY(!f.send(packet(4, 0)));
	QCOMPARE(f.count("released"), 1);
	Fixture rapid;
	QVERIFY(rapid.send(packet(1, 10, true)));
	QVERIFY(!rapid.send(packet(1, 0))); // Sequence reset without quiet time: fail closed.
	QVERIFY(!rapid.send(packet(1, 2)));
	rapid.now = 1000000;
	QVERIFY(rapid.send(packet(1, 0)));
	Fixture duplicates;
	QVERIFY(duplicates.send(packet(1, 10)));
	duplicates.now = 400000;
	QVERIFY(!duplicates.send(packet(1, 10)));
	duplicates.now = 500000;
	duplicates.engine.expire();
	QCOMPARE(duplicates.count("released"), 1);
}

void TestSimplex::channelAndPermissionChanges() {
	Fixture f;
	QVERIFY(f.send(packet(1, 0)));
	f.engine.invalidate(1, "channel_changed");
	QVERIFY(f.send(packet(2, 0)));
	QVERIFY(!f.send(packet(1, 2, false, 1)));
	QVERIFY(!f.send(packet(1, 4, true, 1)));
	QVERIFY(f.send(packet(1, 6, false, 1)));
	f.engine.invalidate(1, "permission_lost");
	QVERIFY(f.send(packet(3, 0, false, 1)));
	QVERIFY(!f.send(packet(1, 8, false, 1)));
	QCOMPARE(f.count("released", "channel_changed"), 1);
	QCOMPARE(f.count("released", "permission_lost"), 1);
	Fixture idle;
	QVERIFY(idle.send(packet(1, 0, true)));
	idle.engine.invalidate(1, "channel_changed");
	QVERIFY(idle.send(packet(1, 2, false, 1))); // Completed attempt may start anew after moving.
	QVERIFY(idle.send(packet(2, 0)));
}

void TestSimplex::disconnectAndRestart() {
	Fixture f;
	QVERIFY(f.send(packet(1, 0)));
	f.engine.disconnect(1);
	f.engine.disconnect(1);
	QVERIFY(f.send(packet(2, 0)));
	f.engine.disconnect(1); // Late lifecycle event for an old connection.
	QVERIFY(!f.send(packet(3, 0)));
	f.engine.shutdown();
	QVERIFY(f.send(packet(3, 0)));
	QCOMPARE(f.count("released", "disconnected"), 1);
	QCOMPARE(f.count("released", "server_stop"), 1);
}

void TestSimplex::alternateTargetsAndEligibility() {
	Fixture f;
	for (unsigned int target : { 1U, 30U, 31U }) {
		auto p   = packet(target, 0);
		p.target = target;
		QVERIFY(!f.send(p));
	}
	auto p     = packet(100, 0);
	p.eligible = false;
	QVERIFY(!f.send(p));
	QVERIFY(f.send(packet(200, 0)));
	QVERIFY(!f.send(packet(100, 2)));
	p          = packet(200, 2);
	p.eligible = false;
	QVERIFY(!f.send(p));
	QVERIFY(f.send(packet(201, 0)));
	QCOMPARE(f.count("released", "not_eligible"), 1);
}

void TestSimplex::timeoutBounds() {
	SimplexController low(-1, 0), high(100000, 1);
	QCOMPARE(low.timeoutMs(), 50);
	QCOMPARE(low.retryMs(), 50);
	QCOMPARE(high.timeoutMs(), 60000);
	QCOMPARE(high.retryMs(), 60000);
}

void TestSimplex::asynchronousLog() {
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	SimplexLogger::Config config;
	config.path     = dir.filePath("simplex.jsonl");
	config.maxBytes = 4096;
	config.denials  = false;
	{
		SimplexLogger logger(config);
		SimplexController::Event e;
		e.context.user = "LoggerUser";
		e.event        = "denied";
		logger.submit(e);
		e.event = "granted";
		for (int i = 0; i < 500; ++i) {
			logger.submit(e);
			if (i % 10 == 0)
				QTest::qWait(1);
		}
	}
	const auto data = readFile(config.path);
	QVERIFY(data.contains("simplex_log_stopped"));
	QVERIFY(!data.contains("\"denied\""));
	QVERIFY(QFile::exists(config.path + ".1"));
	QVERIFY(data.size() <= 4096);
	QVERIFY(readFile(config.path + ".1").size() <= 4096);
	config.path = dir.filePath("missing/simplex.jsonl");
	{
		SimplexLogger logger(config);
		SimplexController engine(500, 1000, [&](const SimplexController::Event &e) { logger.submit(e); });
		QVERIFY(engine.begin(packet(1, 0)).allowed);
		QVERIFY(!engine.begin(packet(2, 0)).allowed);
	}
}

QTEST_GUILESS_MAIN(TestSimplex)
#include "TestSimplex.moc"
