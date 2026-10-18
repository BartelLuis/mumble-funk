// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license in the LICENSE file.
#include "AudioFeedback.h"
#include "AudioSignal.h"
#include <algorithm>
#include <exception>

AudioFeedback::AudioFeedback(int volume, const QString &busyFile) {
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
	m_busy = AudioSignal::encode(pcm, volume);
}

const std::vector< QByteArray > &AudioFeedback::clip(Signal signal) const {
	(void) signal;
	return m_busy;
}

bool AudioFeedback::request(Signal signal, Recipient recipient, std::int64_t now) {
	std::unique_lock< std::mutex > lock(m_mutex, std::try_to_lock);
	if (!lock || m_pending.size() >= MaxPending || !recipient.connection)
		return false; // Deliberately omit feedback under overload; never delay speech.
	m_pending.push_back({ signal, recipient, now });
	return true;
}

void AudioFeedback::tick(std::int64_t now, const Delivery &deliver) {
	std::deque< Request > pending;
	{
		std::lock_guard< std::mutex > lock(m_mutex);
		pending.swap(m_pending);
	}
	// Keep cooldown/sequence state bounded, including disconnected/idle clients.
	for (auto it = m_playback.begin(); it != m_playback.end();) {
		if (!it->second.active && now - it->second.lastStart >= 60000000)
			it = m_playback.erase(it);
		else
			++it;
	}
	for (const auto &request : pending) {
		if (now - request.time > 300000 || now < request.time)
			continue;
		auto it = m_playback.find(request.recipient.connection);
		if (it != m_playback.end()) {
			if (it->second.active || now - it->second.lastStart < 1000000)
				continue;
			const auto sequence = it->second.nextFrame + 100; // Gap between independent jitter-buffer streams.
			it->second          = { request, now, now, sequence, 0, true };
		} else if (m_playback.size() < MaxRecipients) {
			m_playback.emplace(request.recipient.connection, Playback{ request, now, now, 0, 0, true });
		}
	}
	for (auto &entry : m_playback) {
		auto &play = entry.second;
		if (!play.active)
			continue;
		// Do not burst/catch up after a stalled event loop: abandon obsolete tones.
		if (now - play.started > static_cast< std::int64_t >(play.index) * 20000 + 100000) {
			play.active = false;
			continue;
		}
		if (now - play.started < static_cast< std::int64_t >(play.index) * 20000)
			continue;
		const auto &packets = clip(play.request.signal);
		const bool last     = play.index + 1 == packets.size();
		if (!deliver(play.request.recipient, packets.at(play.index), play.nextFrame, last) || last)
			play.active = false;
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
	m_playback.erase(connection);
}
