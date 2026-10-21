// Copyright The Mumble-Funk Contributors. All rights reserved.
// Use of this source code is governed by a BSD-style license in the LICENSE file.
#include "AudioFeedback.h"
#include "AudioSignal.h"
#include <algorithm>
#include <exception>
#include <set>

AudioFeedback::AudioFeedback(int volume, const QString &busyFile, int boostDb) : m_boostDb(boostDb) {
	std::vector< float > pcm;
	if (!busyFile.isEmpty()) {
		try {
			pcm          = AudioSignal::loadWav(busyFile);
			m_customBusy = true;
		} catch (const std::exception &error) {
			m_warning = QString::fromUtf8(error.what());
		}
	}
	if (pcm.empty())
		pcm = AudioSignal::syntheticBusy();
	m_busy = AudioSignal::encode(pcm, volume, m_boostDb);
}

const std::vector< QByteArray > &AudioFeedback::clip(Signal signal) const {
	if (signal == Signal::SquelchOpen) return m_open;
	if (signal == Signal::SquelchClose) return m_close;
	return m_busy;
}

bool AudioFeedback::request(Signal signal, Recipient recipient, std::int64_t now) {
	std::unique_lock< std::mutex > lock(m_mutex, std::try_to_lock);
	if (signal != Signal::Busy || !lock || m_pending.size() >= MaxPending || !recipient.connection)
		return false; // Deliberately omit feedback under overload; never delay speech.
	m_pending.push_back({ signal, recipient, now });
	return true;
}

void AudioFeedback::configureSquelch(const QString &openFile, const QString &closeFile, int openVolume, int closeVolume, bool openEnabled, bool closeEnabled) {
	// All file access and Opus encoding happen before the voice thread starts.
	auto open = openEnabled ? AudioSignal::encodeWavSignal(openFile, openVolume, m_boostDb) : std::vector<QByteArray>{};
	auto close = closeEnabled ? AudioSignal::encodeWavSignal(closeFile, closeVolume, m_boostDb) : std::vector<QByteArray>{};
	m_open = std::move(open); m_close = std::move(close); m_squelchConfigured = true;
}
bool AudioFeedback::requestSquelch(const SquelchPlan::Cue &cue) {
	std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
	const auto signal = cue.kind == SquelchPlan::Kind::Open ? Signal::SquelchOpen : Signal::SquelchClose;
	if (!lock || m_cues.size() >= MaxPending || clip(signal).empty()) return false;
	m_cues.push_back({cue.kind == SquelchPlan::Kind::Open ? Signal::SquelchOpen : Signal::SquelchClose,
		{cue.recipient.connection, cue.recipient.session, cue.recipient.channel}, cue.createdUs, cue});
	return true;
}

void AudioFeedback::tick(std::int64_t now, const Delivery &deliver, const CueGate &gate) {
	std::deque< Request > pending, cues;
	{
		std::lock_guard< std::mutex > lock(m_mutex);
		pending.swap(m_pending);
		cues.swap(m_cues);
	}
	// Keep cooldown/sequence state bounded, including disconnected/idle clients.
	for (auto it = m_playback.begin(); it != m_playback.end();) {
		if (!it->second.active && now - it->second.lastStart >= 60000000)
			it = m_playback.erase(it);
		else
			++it;
	}
	std::deque<Request> deferredBusy;
	std::set<std::uint64_t> busyWaiting;
	for (const auto &request : pending) {
		if (now - request.time > 300000 || now < request.time)
			continue;
		auto it = m_playback.find(request.recipient.connection);
		if (it != m_playback.end()) {
			if ((it->second.active && it->second.request.signal == Signal::Busy) || now - it->second.lastBusy < 1000000)
				continue;
			if (it->second.request.signal != Signal::Busy && (it->second.active || now < it->second.availableAt)) {
				// End an optional effect at its next packet boundary, then let the
				// client retire that stream. Never splice Busy into an unterminated clip.
				it->second.finishEarly = true;
				deferredBusy.push_back(request); busyWaiting.insert(request.recipient.connection);
				continue;
			}
			const auto sequence = it->second.nextFrame + 100; // Gap between independent jitter-buffer streams.
			it->second          = { request, now, now, sequence, 0, true, now, 0 };
		} else if (m_playback.size() < MaxRecipients) {
			m_playback.emplace(request.recipient.connection, Playback{ request, now, now, 0, 0, true, now, 0 });
		}
	}
	std::deque<Request> deferred;
	for (const auto &request : cues) {
		if (busyWaiting.count(request.recipient.connection) || !gate || now < request.time || now - request.time > (request.signal == Signal::SquelchClose ? SquelchPlan::MaxCloseCueAgeUs : SquelchPlan::MaxCueAgeUs)) continue;
		auto it = m_playback.find(request.recipient.connection);
		if (it != m_playback.end() && it->second.active && it->second.request.signal == Signal::Busy)
			continue; // Busy wins; optional Squelch does not queue behind it.
		if (it != m_playback.end() && (it->second.active || now < it->second.availableAt)) {
			deferred.push_back(request); continue;
		}
		if (it == m_playback.end() && m_playback.size() >= MaxRecipients) continue;
		if (!gate(request.cue, true)) continue;
		if (it == m_playback.end()) {
			m_playback.emplace(request.recipient.connection, Playback{request, now, now, 0, 0, true});
		} else {
			const auto lastBusy = it->second.lastBusy;
			const auto sequence = it->second.nextFrame + 100;
			it->second = {request, now, now, sequence, 0, true, lastBusy, 0};
		}
	}
	if (!deferredBusy.empty()) {
		std::lock_guard<std::mutex> lock(m_mutex);
		while (!deferredBusy.empty() && m_pending.size() < MaxPending) {
			m_pending.push_front(deferredBusy.back()); deferredBusy.pop_back();
		}
	}
	if (!deferred.empty()) {
		std::lock_guard<std::mutex> lock(m_mutex);
		// Older Open/Close requests precede newly enqueued ones, with a hard cap.
		while (!deferred.empty() && m_cues.size() < MaxPending) {
			m_cues.push_front(deferred.back()); deferred.pop_back();
		}
	}
	for (auto &entry : m_playback) {
		auto &play = entry.second;
		if (!play.active)
			continue;
		if (play.request.signal != Signal::Busy && (!gate || !gate(play.request.cue, false))) {
			play.active = false; continue;
		}
		// Do not burst/catch up after a stalled event loop: abandon obsolete tones.
		if (now - play.started > static_cast< std::int64_t >(play.index) * 20000 + 100000) {
			play.active = false;
			continue;
		}
		if (now - play.started < static_cast< std::int64_t >(play.index) * 20000)
			continue;
		const auto &packets = clip(play.request.signal);
		const bool last     = play.finishEarly || play.index + 1 == packets.size();
		if (!deliver(play.request.recipient, packets.at(play.index), play.nextFrame, last) || last)
			play.active = false;
		if (!play.active) play.availableAt = now + 60000; // Packet duration + small playback retirement gap.
		++play.index;
		play.nextFrame += 2; // Mumble sequence units are 10 ms, Opus packets here are 20 ms.
	}
}

void AudioFeedback::cancel(std::uint64_t connection) {
	std::lock_guard< std::mutex > lock(m_mutex);
	m_pending.erase(
		std::remove_if(m_pending.begin(), m_pending.end(),
					   [connection](const Request &request) { return request.recipient.connection == connection; }),
		m_pending.end());
	m_cues.erase(std::remove_if(m_cues.begin(), m_cues.end(), [connection](const Request &r) {
		return r.recipient.connection == connection || r.cue.sender.connection == connection;
	}), m_cues.end());
	if (!m_squelchConfigured) {
		m_playback.erase(connection); // Preserve the original Busy-only behaviour.
	} else {
		auto it = m_playback.find(connection);
		if (it != m_playback.end()) {
			// A channel move is not a new session: retain its output sequence space.
			it->second.active = false;
			it->second.lastBusy = -1000000;
		}
	}
}
