// Copyright The Mumble-Funk Contributors. All rights reserved.
// Use of this source code is governed by a BSD-style license in the LICENSE file.
#include "AudioFeedback.h"
#include "AudioSignal.h"
#include <algorithm>
#include <exception>
#include <set>
#include <stdexcept>

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

bool AudioFeedback::configureNoise(const QString &file, int volume) {
    if (volume < 0 || volume > 100) throw std::invalid_argument("Noise volume must be 0..100");
    if (file.isEmpty() || volume == 0) return false;
    auto pcm = RadioNoise::load(file);
    std::array<NoiseSlot, RadioNoise::MaxInstances> slots;
    for (auto &slot : slots) slot.producer.reset(new RadioNoise(pcm, volume, 0, true));
    m_random.seed(std::random_device{}());
    m_noisePCM = std::move(pcm); m_noiseSlots = std::move(slots);
    return true;
}
bool AudioFeedback::requestNoise(SquelchPlan::Cue cue) {
    std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
    if (!m_noisePCM || !lock || m_noiseRequests.size() >= MaxPending) return false;
    cue.kind = SquelchPlan::Kind::Noise; m_noiseRequests.push_back(cue); return true;
}
void AudioFeedback::updateNoise(std::int64_t now, const CueGate &gate, const std::deque<SquelchPlan::Cue> &requests) {
    for (const auto &cue : requests) {
        if (!gate || !gate(cue, false) || now < cue.createdUs || now - cue.createdUs > SquelchPlan::MaxCueAgeUs) continue;
        auto old = m_noise.find(cue.recipient.connection);
        if (old != m_noise.end() && old->second.cue.transmission == cue.transmission) continue;
        if (old == m_noise.end() && m_noise.size() >= MaxRecipients) continue;
        std::size_t start = 0; bool found = false;
        for (const auto &state : m_noise) if (state.second.cue.transmission == cue.transmission) {
            // Original offset is retained separately from each recipient cursor.
            start = state.second.start; found = true; break;
        }
        if (!found) start = RadioNoise::chooseStart(m_noisePCM->size(), m_random);
        auto play = m_playback.find(cue.recipient.connection);
        if (play != m_playback.end() && play->second.request.signal == Signal::Noise && play->second.active) {
            play->second.active = false; play->second.availableAt = now + 60000;
        }
        m_noise[cue.recipient.connection] = {cue, start, -1, start};
    }
    for (auto it = m_noise.begin(); it != m_noise.end();) {
        auto &state = it->second;
        if (gate && gate(state.cue, false)) { ++it; continue; }
        auto play = m_playback.find(it->first);
        if (play != m_playback.end() && play->second.active && play->second.request.signal == Signal::Noise) {
            auto end = state.cue; end.kind = SquelchPlan::Kind::NoiseEnd;
            if (gate && gate(end, false)) {
                play->second.request.cue = end; play->second.finishEarly = true; ++it; continue;
            }
            play->second.active = false; play->second.availableAt = now + 60000;
        }
        it = m_noise.erase(it);
    }
    // Pool operations are try-lock only. Never create, destroy or join a thread
    // while the voice lock is held. Retry a contended retirement next tick.
    for (std::size_t i = 0; i < m_noiseSlots.size(); ++i) {
        auto &slot = m_noiseSlots[i]; if (!slot.connection) continue;
        auto state = m_noise.find(slot.connection); auto play = m_playback.find(slot.connection);
        if (state != m_noise.end() && state->second.slot == int(i) && state->second.cue.transmission == slot.transmission
            && play != m_playback.end() && play->second.active && play->second.request.signal == Signal::Noise) continue;
        if (slot.producer->tryPause()) {
            if (state != m_noise.end() && state->second.slot == int(i)) state->second.slot = -1;
            slot.connection = 0;
        }
    }
}
void AudioFeedback::startNoise(std::int64_t now, const CueGate &gate, const std::set<std::uint64_t> &waiting) {
    for (auto &entry : m_noise) {
        auto &state = entry.second; auto play = m_playback.find(entry.first);
        if (waiting.count(entry.first) || state.slot >= 0 || !gate || !gate(state.cue, false)) continue;
        if (play != m_playback.end() && (play->second.active || now < play->second.availableAt)) continue;
        if (play == m_playback.end() && m_playback.size() >= MaxRecipients) continue;
        for (std::size_t i = 0; i < m_noiseSlots.size(); ++i) {
            auto &slot = m_noiseSlots[i];
            if (slot.connection || !slot.producer->tryRestart(state.nextSample)) continue;
            slot.connection = entry.first; slot.transmission = state.cue.transmission; state.slot = int(i);
            const auto frame = play == m_playback.end() ? 0 : play->second.nextFrame + 100;
            const auto busy = play == m_playback.end() ? -1000000 : play->second.lastBusy;
            Request request{Signal::Noise, {state.cue.recipient.connection, state.cue.recipient.session,
                state.cue.recipient.channel}, now, state.cue};
            m_playback[entry.first] = {request, now, now, frame, 0, true, busy, 0};
            break;
        }
    }
}

void AudioFeedback::tick(std::int64_t now, const Delivery &deliver, const CueGate &gate) {
	std::deque< Request > pending, cues;
	std::deque<SquelchPlan::Cue> noiseRequests;
	{
		std::lock_guard< std::mutex > lock(m_mutex);
		pending.swap(m_pending);
		cues.swap(m_cues);
		noiseRequests.swap(m_noiseRequests);
	}
	if (m_noisePCM) { m_lastNoiseTick = now; updateNoise(now, gate, noiseRequests); }
	// Keep cooldown/sequence state bounded, including disconnected/idle clients.
	for (auto it = m_playback.begin(); it != m_playback.end();) {
		// Noise can run longer than the idle retention period. Retain its
		// sequence and retirement gap after its final packet, just as for a
		// recent short clip; never expire it merely because it started long ago.
		if (it->second.active && it->second.request.signal == Signal::Noise)
			it->second.lastActivity = now;
		if (!it->second.active && now - it->second.lastActivity >= 60000000)
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
	std::set<std::uint64_t> waiting = busyWaiting;
	for (const auto &request : cues) {
		waiting.insert(request.recipient.connection);
		if (busyWaiting.count(request.recipient.connection) || !gate || now < request.time || now - request.time > (request.signal == Signal::SquelchClose ? SquelchPlan::MaxCloseCueAgeUs : SquelchPlan::MaxCueAgeUs)) continue;
		auto it = m_playback.find(request.recipient.connection);
		if (it != m_playback.end() && it->second.active && it->second.request.signal == Signal::Busy)
			continue; // Busy wins; optional Squelch does not queue behind it.
		if (it != m_playback.end() && (it->second.active || now < it->second.availableAt)) {
			if (it->second.active && it->second.request.signal == Signal::Noise) it->second.finishEarly = true;
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
	if (m_noisePCM) startNoise(now, gate, waiting);
	for (auto &entry : m_playback) {
		auto &play = entry.second;
		if (!play.active)
			continue;
		if (play.request.signal != Signal::Busy && (!gate || !gate(play.request.cue, false))) {
			play.active = false; if (play.request.signal == Signal::Noise) play.availableAt = now + 60000; continue;
		}
		// Do not burst/catch up after a stalled event loop: abandon obsolete tones.
		if (now - play.started > static_cast< std::int64_t >(play.index) * 20000 + 100000) {
			play.active = false;
            if (play.request.signal == Signal::Noise) play.availableAt = now + 60000;
			continue;
		}
		if (now - play.started < static_cast< std::int64_t >(play.index) * 20000)
			continue;
		QByteArray payload;
        bool last = play.finishEarly;
        if (play.request.signal == Signal::Noise) {
            auto state = m_noise.find(entry.first);
            if (state == m_noise.end() || state->second.slot < 0) { play.active = false; continue; }
            RadioNoise::Packet packet;
            if (!m_noiseSlots[std::size_t(state->second.slot)].producer->take(packet)) continue;
            state->second.nextSample = (packet.firstSample + RadioNoise::FrameSamples) % m_noisePCM->size();
            payload = std::move(packet.opus); // Producer frame never becomes an outgoing sequence.
        } else {
            const auto &packets = clip(play.request.signal);
            last = last || play.index + 1 == packets.size(); payload = packets.at(play.index);
        }
		if (!deliver(play.request.recipient, payload, play.nextFrame, last) || last)
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
	m_noiseRequests.erase(std::remove_if(m_noiseRequests.begin(), m_noiseRequests.end(), [connection](const SquelchPlan::Cue &cue) {
        return cue.sender.connection == connection || cue.recipient.connection == connection;
    }), m_noiseRequests.end());
    for (auto it = m_noise.begin(); it != m_noise.end();) {
        if (it->first == connection || it->second.cue.sender.connection == connection) {
            auto play = m_playback.find(it->first);
            if (play != m_playback.end() && play->second.request.signal == Signal::Noise) {
                play->second.active = false; play->second.availableAt = m_lastNoiseTick + 60000;
            }
            it = m_noise.erase(it);
        } else ++it;
    }
	if (!m_squelchConfigured && !m_noisePCM) {
		m_playback.erase(connection); // Preserve the original Busy-only behaviour.
	} else {
		auto it = m_playback.find(connection);
		if (it != m_playback.end()) {
			// A channel move is not a new session: retain its output sequence space.
			it->second.active = false;
			it->second.lastBusy = -1000000;
            if (m_noisePCM) it->second.availableAt = m_lastNoiseTick + 60000;
		}
	}
}
