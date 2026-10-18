// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license in the LICENSE file.
#include "Harness.h"
using namespace SimplexTest;

class TestSimplexIntegration : public QObject {
	Q_OBJECT
private slots:
	void ownership_data();
	void ownership();
	void timeoutAndDisconnect();
	void lifecycleAndPermissions();
	void alternateRoutes_data();
	void alternateRoutes();
	void restartAndLoggingDisabled();
};

void TestSimplexIntegration::ownership_data() {
	QTest::addColumn< QString >("setting");
	QTest::addColumn< bool >("modern");
	QTest::newRow("default-off-legacy") << QString() << false;
	QTest::newRow("explicit-off-legacy") << QString("false") << false;
	QTest::newRow("on-legacy") << QString("true") << false;
	QTest::newRow("explicit-off-modern") << QString("false") << true;
	QTest::newRow("on-modern") << QString("true") << true;
}

void TestSimplexIntegration::ownership() {
	QFETCH(QString, setting);
	QFETCH(bool, modern);
	const bool enabled = setting == "true";
	Fixture f;
	QVERIFY(f.start(setting));
	Client a, b, observer;
	QVERIFY(a.login(f.port, "Alice", modern));
	QVERIFY(b.login(f.port, "Bob", modern));
	QVERIFY(observer.login(f.port, "Observer", modern));
	QVERIFY(a.speak());
	QVERIFY(observer.expect(a));
	QVERIFY(b.speak(true));
	QVERIFY(b.udpBarrier());
	QVERIFY(observer.expect(b, !enabled));
	QVERIFY(a.speak(false, true));
	QVERIFY(observer.expect(a, true, true));
	QVERIFY(b.speak());
	QVERIFY(observer.expect(b, !enabled)); // Held PTT must not become audible.
	QVERIFY(b.speak(false, true, 0, modern));
	QVERIFY(observer.expect(b, !enabled, true, modern));
	QVERIFY(b.speak());
	QVERIFY(observer.expect(b));
	QVERIFY(b.speak(false, true));
	QVERIFY(observer.expect(b, true, true));
	// Nearly simultaneous traffic enters through both server threads.
	for (int i = 0; i < 8; ++i) {
		if (i % 2) {
			QVERIFY(b.speak(true));
			QVERIFY(a.speak());
		} else {
			QVERIFY(a.speak(true));
			QVERIFY(b.speak());
		}
		// Wait until both server ingress paths have processed their packets before
		// sending end markers. This keeps scheduler delay out of the race assertion.
		QVERIFY((i % 2 ? b : a).udpBarrier());
		QVERIFY(a.barrier());
		QVERIFY(b.barrier());
		const auto received = observer.audio(100, enabled ? 1 : 2);
		QCOMPARE(received.size(), enabled ? 1 : 2);
		for (const auto &p : received) {
			QVERIFY(p.session == a.session || p.session == b.session);
			QCOMPARE(p.payload, audioContent);
		}
		QVERIFY(a.speak(false, true));
		QVERIFY(b.speak(false, true));
		QVERIFY(a.barrier());
		QVERIFY(b.barrier());
		QCOMPARE(observer.audio(100, enabled ? 1 : 2).size(), enabled ? 1 : 2);
	}
	// The existing packet diagnostic sees both accepted and denied UDP/TCP data.
	QTRY_VERIFY(f.events("voice").size() > 30);
	int udp = 0, tcp = 0;
	for (const auto &e : f.events("voice")) {
		if (e.value("event") != "packet")
			continue;
		udp += e.value("transport") == "udp";
		tcp += e.value("transport") == "tcp";
	}
	QVERIFY(udp >= 8);
	QVERIFY(tcp >= 8);
	// Forwarding assertions above are authoritative. Diagnostics are asynchronous
	// and explicitly allowed to drop records under contention. Drain them and
	// account for reported loss rather than requiring a lossless event stream.
	f.server.stop();
	if (enabled) {
		const auto events = f.events("simplex");
		QVERIFY(!events.isEmpty());
		QCOMPARE(events.last().value("event").toString(), QString("simplex_log_stopped"));
		const auto lost = events.last().value("dropped_total").toString().toULongLong();
		QVERIFY(f.count("granted") > 0);
		QVERIFY(f.count("denied") > 0);
		QVERIFY(f.count("released", "marker_confirmed") > 0);
		QVERIFY(static_cast< qulonglong >(f.count("granted")) + lost >= 10);
		QVERIFY(static_cast< qulonglong >(f.count("denied")) + lost >= 9);
		QVERIFY(static_cast< qulonglong >(f.count("released", "marker_confirmed")) + lost >= 10);
	} else {
		QVERIFY(!QFile::exists(f.directory.filePath("simplex.1.jsonl")));
	}
	for (const QString &prefix : { QString("voice"), QString("simplex") }) {
		QFile file(f.directory.filePath(prefix + ".1.jsonl"));
		if (file.open(QIODevice::ReadOnly))
			QVERIFY(!file.readAll().contains(audioContent));
	}
}

void TestSimplexIntegration::timeoutAndDisconnect() {
	Fixture f;
	QVERIFY(f.start("true", 500, 1000));
	Client a, b, c, observer;
	QVERIFY(a.login(f.port, "Alice"));
	QVERIFY(b.login(f.port, "Bob"));
	QVERIFY(c.login(f.port, "Charlie"));
	QVERIFY(observer.login(f.port, "Observer"));
	QVERIFY(a.speak());
	QVERIFY(observer.expect(a));
	QVERIFY(b.speak());
	QVERIFY(observer.expect(b, false));
	for (int i = 0; i < 14; ++i) {
		QVERIFY(b.speak(i % 2));
		QTest::qWait(40);
	}
	QVERIFY(observer.audio().isEmpty());
	QTRY_COMPARE(f.count("released", "timeout_suspected"), 1);
	QVERIFY(c.speak());
	QVERIFY(observer.expect(c));
	// A late marker from A cannot release C.
	QVERIFY(a.speak(false, true));
	QVERIFY(observer.expect(a, false));
	QVERIFY(b.speak());
	QVERIFY(observer.expect(b, false));
	QVERIFY(c.speak(false, true));
	QVERIFY(observer.expect(c, true, true));
	QVERIFY(b.speak());
	QVERIFY(observer.expect(b, false));
	QTest::qWait(1050);
	QVERIFY(b.sendFrame(0, false, false));
	QVERIFY(observer.expect(b));
	b.socket.abort();
	QTRY_COMPARE(f.count("released", "disconnected"), 1);
	QVERIFY(a.speak());
	QVERIFY(observer.expect(a));
}

void TestSimplexIntegration::lifecycleAndPermissions() {
	Fixture f;
	QVERIFY(f.start("true"));
	Client admin, a, b, observer, other;
	QVERIFY(admin.login(f.port, "SuperUser"));
	QVERIFY(a.login(f.port, "Alice"));
	QVERIFY(b.login(f.port, "Bob"));
	QVERIFY(observer.login(f.port, "Observer"));
	QVERIFY(other.login(f.port, "OtherObserver"));
	const auto second = channel(admin, "Second");
	QVERIFY(second != 0);
	QVERIFY(other.move(second));
	QVERIFY(a.speak());
	QVERIFY(observer.expect(a));
	QVERIFY(a.move(second));
	QVERIFY(b.speak());
	QVERIFY(observer.expect(b)); // Old channel is immediately free.
	QVERIFY(a.speak());
	QVERIFY(other.expect(a, false));
	QVERIFY(a.speak(false, true));
	QVERIFY(other.expect(a, false));
	QVERIFY(a.speak());
	QVERIFY(other.expect(a)); // New attempt in second channel.
	QVERIFY(b.speak());
	QVERIFY(observer.expect(b)); // Independent channel still works.
	QVERIFY(a.sendFrame(a.lastSent - 1, false, true));
	QVERIFY(other.expect(a, false));
	QVERIFY(a.speak());
	QVERIFY(other.expect(a));
	// Mute via the administrative UserState path must release immediately.
	MumbleProto::UserState state;
	state.set_session(a.session);
	state.set_mute(true);
	QVERIFY(admin.send(Type::UserState, state));
	QVERIFY(admin.barrier());
	QTRY_VERIFY(f.count("released", "permission_lost") >= 1);
	QVERIFY(a.speak());
	QVERIFY(other.expect(a, false));
	state.set_mute(false);
	QVERIFY(admin.send(Type::UserState, state));
	QVERIFY(admin.barrier());
	QVERIFY(a.speak());
	QVERIFY(other.expect(a, false)); // Permission restoration is not a fresh PTT.
	QVERIFY(a.speak(false, true));
	QVERIFY(other.expect(a, false));
	QVERIFY(a.speak());
	QVERIFY(other.expect(a));
	// ACL cache invalidation is another independent permission-loss path.
	MumbleProto::ACL acl;
	acl.set_channel_id(second);
	auto *rule = acl.add_acls();
	rule->set_group("all");
	rule->set_deny(0x8);
	QVERIFY(admin.send(Type::ACL, acl));
	QVERIFY(admin.barrier());
	QTRY_VERIFY(f.count("released", "permission_lost") >= 2);
	QVERIFY(a.speak());
	QVERIFY(other.expect(a, false));
	acl.clear_acls();
	QVERIFY(admin.send(Type::ACL, acl));
	QVERIFY(admin.barrier());
	QVERIFY(a.speak());
	QVERIFY(other.expect(a, false));
	QVERIFY(a.speak(false, true));
	QVERIFY(other.expect(a, false));
	QVERIFY(a.speak());
	QVERIFY(other.expect(a));
	// Self mute also invalidates ownership, without an audio end marker.
	state.Clear();
	state.set_session(a.session);
	state.set_self_mute(true);
	QVERIFY(a.send(Type::UserState, state));
	QVERIFY(a.barrier());
	QTRY_VERIFY(f.count("released", "permission_lost") >= 3);
	QVERIFY(a.speak());
	QVERIFY(other.expect(a, false));
}

void TestSimplexIntegration::alternateRoutes_data() {
	QTest::addColumn< bool >("enabled");
	QTest::newRow("standard-routing") << false;
	QTest::newRow("radio-routing") << true;
}

void TestSimplexIntegration::alternateRoutes() {
	QFETCH(bool, enabled);
	Fixture f;
	QVERIFY(f.start(enabled ? "true" : "false"));
	Client admin, a, local, remote, listener;
	QVERIFY(admin.login(f.port, "SuperUser", true));
	QVERIFY(a.login(f.port, "Alice", true));
	QVERIFY(local.login(f.port, "Local", true));
	QVERIFY(remote.login(f.port, "Remote", true));
	QVERIFY(listener.login(f.port, "Listener", true));
	const auto second = channel(admin, "Second");
	const auto third  = channel(admin, "Third");
	QVERIFY(second != 0);
	QVERIFY(third != 0);
	QVERIFY(remote.move(second));
	QVERIFY(listener.move(third));
	// Link root and Second. Listener subscribes to root from an unlinked Third.
	MumbleProto::ChannelState link;
	link.set_channel_id(0);
	link.add_links_add(second);
	QVERIFY(admin.send(Type::ChannelState, link));
	QVERIFY(admin.barrier());
	MumbleProto::UserState listen;
	listen.set_session(listener.session);
	listen.add_listening_channel_add(0);
	QVERIFY(listener.send(Type::UserState, listen));
	QVERIFY(listener.barrier());
	QVERIFY(a.speak());
	QVERIFY(local.expect(a));
	QVERIFY(remote.expect(a, !enabled));
	QVERIFY(listener.expect(a, !enabled));
	QVERIFY(a.speak(false, true));
	QVERIFY(local.expect(a, true, true));
	QVERIFY(remote.expect(a, !enabled, true));
	QVERIFY(listener.expect(a, !enabled, true));
	// Direct whisper, channel whisper and server loopback must all be silent in radio mode.
	MumbleProto::VoiceTarget target;
	target.set_id(1);
	target.add_targets()->add_session(remote.session);
	QVERIFY(a.send(Type::VoiceTarget, target));
	QVERIFY(a.barrier());
	QVERIFY(a.speak(false, false, 1));
	QVERIFY(remote.expect(a, !enabled));
	QVERIFY(local.audio().isEmpty());
	target.Clear();
	target.set_id(2);
	target.add_targets()->set_channel_id(second);
	QVERIFY(a.send(Type::VoiceTarget, target));
	QVERIFY(a.barrier());
	QVERIFY(a.speak(false, false, 2));
	QVERIFY(remote.expect(a, !enabled));
	a.audio(); // Drain earlier control/normal channel traffic before loopback probe.
	QVERIFY(a.speak(false, false, 31));
	QVERIFY(a.expect(a, !enabled));
	QVERIFY(local.audio().isEmpty());
	QVERIFY(listener.audio().isEmpty());
}

void TestSimplexIntegration::restartAndLoggingDisabled() {
	Fixture f;
	QVERIFY(f.start("true", 2500, 3000, false));
	{
		Client a, b, observer;
		QVERIFY(a.login(f.port, "Alice"));
		QVERIFY(b.login(f.port, "Bob"));
		QVERIFY(observer.login(f.port, "Observer"));
		QVERIFY(a.speak());
		QVERIFY(observer.expect(a));
		QVERIFY(b.speak());
		QVERIFY(observer.expect(b, false));
		QVERIFY(f.restart()); // Stop with an occupied channel; same database after restart.
	}
	Client b, observer;
	QVERIFY(b.login(f.port, "Bob"));
	QVERIFY(observer.login(f.port, "Observer"));
	QVERIFY(b.speak());
	QVERIFY(observer.expect(b));
	QVERIFY(!QFile::exists(f.directory.filePath("simplex.1.jsonl")));
}

QTEST_GUILESS_MAIN(TestSimplexIntegration)
#include "TestSimplexIntegration.moc"
