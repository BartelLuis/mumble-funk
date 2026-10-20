// Copyright The Mumble-Funk Contributors. All rights reserved.
// Use of this source code is governed by a BSD-style license in the LICENSE file.
#ifndef MUMBLE_MURMUR_SQUELCHPLAN_H_
#define MUMBLE_MURMUR_SQUELCHPLAN_H_

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

// Phase 2A planning model, deliberately NOT connected to Server::processMsg.
// It plans optional cues, never grants speech, stores audio or sends packets.
// The caller must supply the actual Simplex decision and eligible receiver set.
// A future dispatcher must hold the server voice lock while revalidating identity,
// membership and permissions AND dispatching; consume alone is not an audio API.
class SquelchPlan {
public:
	struct Identity {
		std::uint64_t connection;
		unsigned int session;
		unsigned int channel;
		bool operator==(const Identity &other) const {
			return connection == other.connection && session == other.session && channel == other.channel;
		}
	};
	enum class Kind { Open, Close };
	struct Cue {
		Kind kind;
		Identity sender;
		Identity recipient;
		std::uint64_t transmission;
		std::int64_t createdUs;
	};
	static constexpr std::size_t MaxChannels = 128;
	static constexpr std::size_t MaxRecipients = 64;
	using Clock = std::function<std::int64_t()>;
	static constexpr std::int64_t MaxCueAgeUs = 100000;
	explicit SquelchPlan(bool enabled = false, Clock clock = {});
	// validOpus refers to a validated nonempty payload. Empty markers cannot start
	// a stream. Caller-accepted payloads stay untouched even when planning fails.
	std::vector<Cue> observe(Identity sender, bool allowed, std::uint64_t frame,
		bool payload, bool last, bool validOpus, const std::vector<Identity> &eligibleRecipients);
	// One-shot reservation immediately before a FUTURE dispatcher sends a cue.
	// If Open was omitted, Close cannot be consumed. False = omit optional effect.
	bool consume(const Cue &cue, Identity currentSender, Identity currentRecipient, bool stillEligible);
	// Call on channel leave (including out-and-back), permission loss, timeout or
	// disconnect. These uncertain boundaries cancel; they do not synthesize Close.
	void invalidate(std::uint64_t connection);
	void clear();
	std::size_t size() const;

private:
	struct Recipient {
		Identity identity;
		bool opened = false;
		bool closed = false;
	};
	struct Stream {
		Identity sender;
		std::uint64_t generation;
		std::uint64_t highestFrame;
		bool ended = false;
		std::vector<Recipient> recipients;
	};
	bool m_enabled;
	Clock m_clock;
	mutable std::mutex m_mutex;
	std::map<unsigned int, Stream> m_channels;
	std::uint64_t m_nextGeneration = 1;
};
#endif
