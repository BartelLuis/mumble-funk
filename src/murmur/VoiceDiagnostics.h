// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file at the root of the Mumble source tree.

#ifndef MUMBLE_MURMUR_VOICEDIAGNOSTICS_H_
#define MUMBLE_MURMUR_VOICEDIAGNOSTICS_H_

#include <QtCore/QJsonObject>
#include <QtCore/QString>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

// Values only: never keep users, channels or borrowed audio buffers in the queue.
struct VoiceDiagnosticRecord {
	enum class Kind { Packet, ChannelChanged, Disconnected };
	enum class Transport { UDP, TCP };

	Kind kind                = Kind::Packet;
	Transport transport      = Transport::UDP;
	std::int64_t timeUs      = 0;
	std::uint64_t connection = 0;
	unsigned int session     = 0;
	int channel              = -1;
	QString userName;
	QString channelName;
	std::uint32_t target     = 0;
	std::uint64_t frame      = 0;
	int codec                = 0;
	std::size_t payloadBytes = 0;
	bool last                = false;
};

// Single-owner state machine, also used with a deterministic clock in tests.
class VoiceDiagnosticTracker {
public:
	using Sink = std::function< void(QJsonObject) >;
	VoiceDiagnosticTracker(std::int64_t timeoutUs, Sink sink);
	void process(const VoiceDiagnosticRecord &record);
	void expire(std::int64_t nowUs);
	void interrupt(std::int64_t nowUs, const QString &reason);

private:
	struct Stream {
		VoiceDiagnosticRecord last;
		std::uint64_t id           = 0;
		std::uint64_t highestFrame = 0;
		bool active                = false;
	};
	std::int64_t m_timeoutUs;
	Sink m_sink;
	std::map< std::uint64_t, Stream > m_streams;
	std::uint64_t m_nextStream = 0;
	void emitEvent(const QString &event, const Stream &stream, std::int64_t timeUs, const QString &reason = QString());
};

class VoiceDiagnostics {
public:
	struct Config {
		QString filePath;
		int server                = 1;
		int timeoutMs             = 500;
		std::int64_t maxBytes     = 10 * 1024 * 1024;
		std::size_t queueCapacity = 4096;
	};

	explicit VoiceDiagnostics(Config config);
	~VoiceDiagnostics();
	VoiceDiagnostics(const VoiceDiagnostics &) = delete;
	VoiceDiagnostics &operator=(const VoiceDiagnostics &) = delete;
	// Never wait for the writer or a competing producer. Loss is counted.
	bool submit(VoiceDiagnosticRecord record);

private:
	using Clock = std::chrono::steady_clock;
	Config m_config;
	Clock::time_point m_started;
	std::vector< VoiceDiagnosticRecord > m_queue;
	std::size_t m_head = 0;
	std::size_t m_size = 0;
	std::mutex m_mutex;
	std::condition_variable m_ready;
	bool m_stopping = false;
	std::atomic< bool > m_accepting{ true };
	std::atomic< std::uint64_t > m_dropped{ 0 };
	std::thread m_writer;
	std::int64_t nowUs() const;
	void run();
};

#endif
