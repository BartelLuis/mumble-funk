# Squelch Phase 2A — local development handoff

## Scope and safety boundary

Repository: https://github.com/Shawna7han/mumble-funk
Base: `f82f5aeda5111711b8e0c351e3f8b2400fc4a90a` (observed `origin/dev`).
Local branch: `feature/squelch-audio`. No upstream configured for this branch.
All work stays local. No GitHub write, push, pull request, workflow trigger,
publication, deployment or changes to existing servers are authorized.
The final external `SQUELCH-HANDOFF.md` records the exact local tip and changed files;
a commit cannot include its own resulting hash.

**Original milestone A:** architecture inspected and original WAV assets preserved.
See the resume checkpoint below for subsequent code and tests.
This is not an enabled Squelch engine. On-air Open/Close are not implemented.

## Findings from the existing code

- `Server::processMsg` handles authenticated TCP UDPTunnel and decrypted UDP.
  Callers hold `qrwlVoiceThread`. `SimplexController::Transmission` keeps the
  controller mutex until forwarding has finished. Do not invert this lock order
  or call back into Simplex from its event sink.
- Simplex permits only ordinary speech in the current channel. Its receiver buffer
  excludes the sender, deaf/self-deaf users, linked channels, listeners and whisper.
  Eligibility incorporates mute/suppress, bandwidth and existing permission updates.
  Reuse this decision and actual eligible receiver set; do not infer rights from
  channel membership alone. A granted diagnostic event alone is not a playback API.
- `diagnosticConnection` is the connection generation; `uiSession` can be reused.
  Delayed actions need connection, session, channel and transmission generation,
  revalidated under the voice lock at dispatch. Channel changes out and back need
  explicit invalidation as well, not just equality of the final channel ID.
- Simplex already emits grant/release events, including a confirmed marker,
  suspected timeout, permission loss, channel change, disconnect and shutdown.
  A marker can occur on the very first accepted packet. A release event happens
  inside `begin` *before* the caller forwards that final packet. An event callback
  therefore must not blindly schedule Close before speech forwarding completes.
- `AudioFeedback` is a bounded private Busy scheduler. It sends pre-encoded Opus
  with the recipient's own session through TCP UDPTunnel, even for UDP senders.
  Its one-second cooldown, private audience, gain and sequence space must survive.
  Reusing this own-session identity for Squelch without arbitration can collide
  with an already playing Busy stream. There is no unused independent stream-ID
  field in the ordinary Mumble packet that would fix this.

## Architecture decision and unresolved audible ordering

Preserve ordinary speech forwarding and the Busy engine. Prepare assets and a
bounded event-planning component separately; do not enable a speculative splice.

### Same sender identity: not transparent

`src/mumble/AudioOutput.cpp::addFrameToBuffer` maps sender objects to
`AudioOutputSpeech` instances. `AudioOutputSpeech.cpp` creates one stateful Opus
Decoder and one Speex jitter buffer per instance. The packet frame number is in
10-ms units. Packet duration must be checked using the Opus header, not inferred
from a sequence increment of one. 20-ms packets occupy two sequence units.

Prepending independent Opus packets and shifting speech frame numbers can order
packets in that jitter buffer, but it also changes the decoder state used for
speech. There is no wire-level instruction to reset that decoder at the boundary.
A marker with empty payload is ignored by this client's `addFrameToBuffer`;
a nonempty marker is consumed asynchronously during playback. Sending a marker
and then speech is not a synchronous reset/acknowledgment protocol.

A local libopus experiment with an independent Open encoder and deterministic
speech-like sine input demonstrated different decoded samples for identical
speech packets when the Open packets were prepended. The difference alone does
not establish audible syllable loss; it does establish that packet-byte equality
is insufficient to prove transparent audio. This must be tested across SILK,
hybrid/CELT modes, packet sizes, loss and repeated talkspurts before acceptance.

### Separate identities: no shared playback clock

Independent decoder states avoid contaminating speech. But separate sender
streams have independent adaptive jitter buffers and are mixed by the client.
A server-side 40-ms delay does not establish that the receiver has already played
40 ms of a different stream. Mixing TCP tones with UDP speech adds independent
transport timing. Even one TCP connection orders receipt, not separate stream
playback. Fabricated unknown sessions are ignored by `ServerHandler::handleVoicePacket`.
Announcing synthetic users would introduce a separate protocol/UI/permission
lifecycle; no such users or bot connections are introduced here.

### Bounded delay is not a complete proof

A future scheduler could pre-encode signals once, prioritize speech, cap buffered
speech in bytes/time and omit optional cues on overload. It would still require
an explicit identity and client-playout strategy. Rapid sender changes are
particularly important: already transmitted Close audio cannot be recalled from
an unchanged client's jitter buffer. Do not delay Simplex release or permit a
blocked sender to take ownership to solve an audio scheduling problem.

No decode/mix/re-encode speech pipeline is introduced. A single controlled output
encoder could avoid independent codec-state splicing but would be a material
architecture change and must be justified and measured, not quietly added.

## Proposed conservative lifecycle rules

- Open is based on the first **accepted, valid Opus payload** of a new transmission,
  once per eligible recipient. Never on a denied attempt or an empty marker.
- Close is eligible once after the last accepted speech packet of a **confirmed**
  end, and only for recipients whose Open was actually dispatched.
- Timeout is suspected, not an acknowledged end: omit Close initially. The same
  applies to permission loss, channel changes, disconnect and server shutdown.
  This deliberately sacrifices an optional effect rather than risk a stale tone.
- A new transmission in the channel cancels pending Close from its predecessor.
  Revalidate both connection generations and recipient eligibility at delivery.
  This cancels only unsent work, not audio already buffered on the client.
- Never reuse sequence numbers for different Opus payloads in one output stream.
  Preserve source frame numbers and terminators while the routing decision is open.
- Default off; no speech buffering, tone emission or filesystem work per packet
  is connected to the server in the current development checkpoint.

## Original assets

48 kHz, signed 16-bit PCM little endian, mono. Original WAV bytes unchanged.

| File | Samples | Duration | SHA-256 |
| --- | ---: | ---: | --- |
| `assets/audio/squelch-open.wav` | 897 | 18.6875 ms | `81573e3a11db089de7cf2bbb2918e0747a2edde12923c399ac5042513d038770` |
| `assets/audio/squelch-close.wav` | 3122 | 65.0416667 ms | `8d9091e0e4ee36433eb8aa707a7fa4a19ccec8ce5dee85ef1d91fa478afd0e31` |

Close reaches PCM peak 32767; retain the existing attenuation-only peak ceiling
of 0.2 before applying volume. Do not normalize upward. The current loader requires
20 ms and adds 20 ms silence for encoder lookahead. A signal-specific short-file
policy is needed without loosening Busy's existing file contract. Padding and
encoder lookahead must be included in actual latency calculations.

The maintainer supplies these two originals under CC0 1.0 Universal; see
`assets/audio/LICENSE.md`. No downloaded samples, new audio libraries, copied
third-party code or device-specific fallback sounds are introduced.

## Checkpoint A validation

Read-only remote comparison confirmed the requested dev base. WAV header,
duration, peak and SHA-256 checked. Client decoder and server lock/routing paths
inspected. Standalone local Opus splice experiment executed. No Squelch server
integration or new CTest run exists at this checkpoint.

## Remaining work for GPT-6.1 Sol Hoch

1. Read the final updates below before touching integration. Keep the no-publish
   restriction until explicit authorization, even if continuing in a new task.
2. Resolve the audible ordering/codec-state decision with real Windows-Mumble and
   Android-Mumla clients. Do not mistake protocol-packet tests for hearing tests.
3. Only then connect lifecycle events to the accepted forwarding path under the
   established locks. Confirm final-packet-before-Close ordering and bounded queues.
4. Complete opt-in configuration, validation, separate preview setup, WAV management
   and Docker packaging. Native 64739, Docker host 64740/container 64739; no extra ports.
   Existing `.env`, volumes, installations and default branches remain untouched.
5. Test true encrypted UDP/TCP, rapid handoffs, loss/reordering, actual session reuse,
   channel-out-and-back, mute/deaf/permission changes and resource saturation.
6. Leave Busy gain, retry suppression, fallback and custom WAV handling intact.
7. Rebuild, run the full suite, then update this handoff and verified local backups.

## Restoration

The accompanying external handoff records the exact final commit/tree, binary
patch checksums, file manifest, tests and restoration commands. Apply the patch
only to the base commit above in a separate clean checkout. It contains originals,
code, tests and documentation, but not Git history, build tools or dependencies.
No external backup is established until the owner downloads the files.

## Resume checkpoint — saved before further investigation

The pre-interruption server build completed with ICE enabled, Release, client OFF.
No build or test process was left running. All 20 existing CTest programs passed;
new TestSquelchFoundation passed; new TestSquelchPassthrough failed (21/22 total).
These are automated checks, not Windows/Android listening tests.

Implemented locally:
- AudioSignal::LengthPolicy::ShortSignal accepts 1–1000 ms PCM WAV signals.
  The default Busy policy remains 20–1000 ms with unchanged format, peak and size
  limits. Open loads only using the explicit short policy. Existing encoding
  adds 20 ms lookahead padding: supplied Open yields two 20 ms packets (40 ms),
  Close five packets (100 ms). Original WAV bytes remain unchanged.
- SquelchPlan is a test-only, disabled-by-default lifecycle model, NOT linked into
  the server. It accepts externally established routing decisions and recipients,
  caps states at 128 channels and 64 recipients, expires cues after 100 ms,
  consumes each cue once and keys it by connection/session/channel/transmission.
  Open precedes Close; Close requires a confirmed marker and consumed Open.
  Explicit invalidation handles timeout, channel changes, permission loss and
  reused sessions. New transmissions cancel unsent old cues. No audio is sent.
- TestSquelchFoundation exercises original asset hashes, malformed/short WAVs,
  padding/volume, Simplex eligibility, bounded concurrent consumption, stale cues,
  session reuse/channel invalidation, sequence units and Opus splice differences.
- TestSquelchPassthrough exercises real Opus speech and private Busy on a local
  isolated server over TLS tunnel and encrypted UDP, in eight protocol/transport
  combinations. It currently sends four packets in a burst and can fail before
  forwarding with Simplex `not_eligible`. The bandwidth gate is a hypothesis,
  not yet an established diagnosis. Preserve assertions and investigate the
  fixture; do not relax production eligibility or ignore lost packets.
- jitter_experiment.py against existing libspeexdsp passed three experiments:
  ordering within one buffer, independent stream clocks and late-prefix loss.

Existing local logs: /workspace/work/squelch-backup/{all-tests.log,
squelch-tests.log,burst-diagnostic.log,jitter.log}. The separate audio-assets
Python suite was not executed before the usage limit interrupted tool approval.
No end-to-end audible Squelch, config/management integration, Docker packaging,
real client hearing tests or sanitizer run is complete. Milestones B and C
(on-air Open/Close) are NOT complete. Milestone D is partial.

Build environment: source /workspace/.cloud-deps/funk-v0.1/activate.sh;
cmake build directory /workspace/build/squelch. Existing pinned dependencies
were reused locally, without adding third-party versions. Incremental build:
`cmake --build /workspace/build/squelch --parallel 2`.
Test command: `ctest --test-dir /workspace/build/squelch --output-on-failure`.
Socket tests require loopback access in the execution sandbox.

Immediate next steps for GPT-6.1 Sol Hoch: preserve this checkpoint, diagnose the
new passthrough fixture, run pending asset tests, record actual results, then
resolve the audible architecture before wiring any Squelch into production.
No runtime queue, new port, speech transformation or synthetic session exists.
The local backup restores the complete tracked tree and binary assets but is
not an external backup until the owner has downloaded it. The earlier attempted
browser output directory was not writable; do not invent a successful export.
