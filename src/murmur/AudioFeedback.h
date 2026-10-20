// Copyright The Mumble-Funk Contributors. All rights reserved.
// Use of this source code is governed by a BSD-style license in the LICENSE file.
#ifndef MUMBLE_MURMUR_AUDIOFEEDBACK_H_
#define MUMBLE_MURMUR_AUDIOFEEDBACK_H_
#include "SquelchPlan.h"
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

// Producers only submit identity snapshots. Encoding is done once at startup;
// tick/cancel and delivery run on the server's Qt main thread, never the UDP thread.
class AudioFeedback {
public:
	enum class Signal { Busy, SquelchOpen, SquelchClose };
	// Lifecycle/recipient snapshots are separate from clips. Future continuous
	// effects can replace this bounded clip renderer, without changing Simplex.
	struct Recipient {
		std::uint64_t connection;
		unsigned int session;
		unsigned int channel;
	};
	using Delivery = std::function< bool(const Recipient &, const QByteArray &, std::uint64_t, bool) >;
	static constexpr std::size_t MaxPending    = 128;
	static constexpr std::size_t MaxRecipients = 64;
	explicit AudioFeedback(int volume, const QString &busyFile = {});
	QString warning() const { return m_warning; }
	bool customBusy() const { return m_customBusy; }
	bool request(Signal signal, Recipient recipient, std::int64_t nowUs);
	using CueGate = std::function<bool(const SquelchPlan::Cue &, bool starting)>;
	void configureSquelch(const QString &openFile, const QString &closeFile, int openVolume, int closeVolume,
		bool openEnabled = true, bool closeEnabled = true);
	bool requestSquelch(const SquelchPlan::Cue &cue);
	void tick(std::int64_t nowUs, const Delivery &deliver, const CueGate &gate = {});
	void cancel(std::uint64_t connection);
	const std::vector< QByteArray > &clip(Signal signal) const;

private:
	struct Request {
		Signal signal;
		Recipient recipient;
		std::int64_t time;
		SquelchPlan::Cue cue{};
	};
	struct Playback {
		Request request;
		std::int64_t started;
		std::int64_t lastStart;
		std::uint64_t nextFrame = 0;
		std::size_t index       = 0;
		bool active             = true;
		std::int64_t lastBusy = -1000000;
		std::int64_t availableAt = 0;
		bool finishEarly = false;
	};
	std::vector< QByteArray > m_busy, m_open, m_close;
	QString m_warning;
	bool m_customBusy = false;
	bool m_squelchConfigured = false;
	std::mutex m_mutex;
	std::deque< Request > m_pending, m_cues;
	std::map< std::uint64_t, Playback > m_playback;
};
#endif
