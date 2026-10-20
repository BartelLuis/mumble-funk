# Squelch Phase 2A — local development handoff

## Scope and safety boundary

Repository: https://github.com/Shawna7han/mumble-funk
Base: `f82f5aeda5111711b8e0c351e3f8b2400fc4a90a` (observed `origin/dev`).
Local branch: `feature/squelch-audio`. No upstream configured for this branch.
All work stays local. No GitHub write, push, pull request, workflow trigger,
publication, deployment or changes to existing servers are authorized.
The final external `SQUELCH-HANDOFF.md` records the exact local tip and changed files;
a commit cannot include its own resulting hash.

**Latest status:** assets and a tested planning foundation are saved locally.
The final validation section supersedes the historical resume checkpoint below.
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
of 0.2 before applying volume. Do not normalize upward. The default Busy loader requires
20 ms and adds 20 ms silence for encoder lookahead. The new explicitly selected
ShortSignal policy permits 1 ms, without loosening Busy's existing file contract. Padding and
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

## Final local validation after resuming

The new passthrough test now waits 20 ms before each 20-ms Opus packet.
This models capture cadence instead of a four-packet instant burst immediately
following authentication. The old failure logged `not_eligible`; Server.cpp checks
mute/suppress/self-mute and BandwidthRecord at that gate. Fast startup/burst rate
is consistent with the failure and the pacing remedy, but the old aggregate
reason alone does not prove which eligibility predicate fired. No production
rate limit, Simplex rule, transport, TLS setting or UDP helper was changed.
The test now additionally asserts zero `not_eligible` events; every packet,
sender identity, sequence, final marker and decoded sample is still checked.
This is not a fix or a reliability claim for arbitrary startup bursts.

| Check | Actual result and scope |
| --- | --- |
| Release server + 22 CTest programs build | Passed with ICE enabled; no full rebuild repeated on resume |
| Earlier complete CTest run | 21/22 passed in 99.09 s; all 20 pre-existing programs passed; only the new passthrough fixture failed |
| Corrected TestSquelchPassthrough | All 8 cases passed in 10.97 s; legacy/protobuf × TCP/encrypted-UDP input × TCP/encrypted-UDP output |
| Passthrough stability repeats | 5 further complete passes, 40 transport cases, 53.85 s; no retries suppress failures |
| TestSquelchFoundation final run | 14 case rows plus init/cleanup passed (Qt reports 16); 26 ms |
| Existing Python audio-assets tests | 8/8 passed; actual server WAV validator and isolated temp-root administration |
| Speex experiment rerun | 3/3 assertions passed against local libspeexdsp |
| All 22 CTest program coverage | Each has a successful applicable result across the above runs; not a newly repeated full-suite run |

Existing SimplexIntegration covers ownership, simultaneous attempts, timeout,
disconnect, channel moves, permissions, alternate routes and the encrypted UDP
nonce/ping boundary. Existing AudioFeedbackIntegration covers private Busy,
channel-out-and-back cancellation, reconnect, custom tones and **actual reuse**
of the disconnected session number. Those existing programs passed in the full
run; their source and production routing remain byte-identical to the base.
The new lifecycle planner tests validate model identities/invalidation only;
they do not substitute for eventual Squelch dispatch integration tests.

The real-Opus passthrough test proves sample-for-sample equality between decoding
the sent and received speech packets using fresh reference decoders, plus no
speech/Busy leakage to another channel or to the sender. It does not execute a
Windows/Mumla audio device, test arbitrary packet loss, or demonstrate audible
Open/Close. It uses temporary servers on ephemeral loopback ports only.

Reproducible final checks (from repository root, with build dependencies active):

```sh
cmake --build /workspace/build/squelch --target TestSquelchPassthrough --parallel 2
ctest --test-dir /workspace/build/squelch -R '^TestSquelchPassthrough$' --output-on-failure
ctest --test-dir /workspace/build/squelch -R '^TestSquelchPassthrough$' --repeat until-fail:5 --output-on-failure
/workspace/build/squelch/tests/TestSquelchFoundation
python3 tests/funk-install/test_audio_assets.py /workspace/build/squelch/mumble-server
python3 src/tests/TestSquelchFoundation/jitter_experiment.py
```

Logs in the local backup working directory: `resume-build.log`,
`resume-passthrough.log`, `passthrough-repeat.log`, `foundation-final.log`,
`audio-assets.log`, `jitter-final.log`; earlier failure logs retained there.
These scratch logs are not needed to restore source. Results are preserved here.
A full suite can be reproduced with `ctest --test-dir BUILD --output-on-failure`.
Build configuration: Release, BUILD_NUMBER=939, client=OFF, server=ON, tests=ON,
plugins=OFF, overlay=OFF, lto=OFF, CMAKE_UNITY_BUILD=OFF, ice=ON. Cloud Qt 5.15.15,
Opus 1.5.2, GCC 14; existing pinned submodules are required after restoration.

## Refined architecture conclusion and acceptance work

Opus splice measurements with the supplied Open (PCM normalized to -1..1):

| Voice bitrate | Maximum sample delta | RMS delta |
| --- | ---: | ---: |
| 16 kbit/s | 0.00109863 | 0.000436606 |
| 24 kbit/s | 0.00145715 | 0.000308192 |
| 64 kbit/s | 0.00577646 | 0.000402705 |

These small synthetic-signal differences are not evidence of intelligibility
loss or universally unacceptable audio. They disprove the stronger assertion
that a naive independent Opus prefix leaves decoded speech unchanged. Loss/PLC,
codec mode transitions, stereo and natural speech have not been evaluated.

The desktop client's `prepareSampleBuffer` observes a terminator during playback,
sets `nextalive=false`, fades audio and eventually retires the output object.
Simply marking Open's last packet as an end before appending speech is therefore
not a reliable reset boundary; retaining speech's terminator before an appended
Close is likewise not a proven way to keep Close audible. Empty terminators are
ignored by `addFrameToBuffer`. No server-side packet timing acknowledges completion
of playback. The Speex experiments use the library directly, not the complete
client mixer; Mumla internals and current deployed client versions remain to test.

Decision: preserve direct voice forwarding; keep the lifecycle model disconnected
from the server. No live Squelch switch or management command is advertised.
No extra identity/bot, output stream, port, scheduler, speech buffering or
transcoding is introduced. B/C remain incomplete; D covers the foundation and
regression checks but not audible Squelch. This checkpoint is not release-ready.

For the next GPT-6.1 Sol Hoch task:
1. Restore and verify the patch/tree/WAV hashes before changing anything. Keep all
   publication restrictions; work on a local feature branch only.
2. Reuse successful tests; do not rebuild everything without a reason. Inspect
   the aggregate startup-burst eligibility caveat if testing that traffic pattern.
3. Build a local client-playout experiment around the actual output lifecycle and
   record PCM for Open/voice/Close, including first/last speech samples, loss,
   jitter, 10/20/40/60-ms packets, SILK/hybrid/CELT and rapid PTT changes.
4. Compare same-identity splicing and independent-stream timing honestly. Require
   explicit acceptance of any measured quality/latency compromise. A full
   decode/mix/encode pipeline is not pre-approved as an unnecessary workaround.
5. Only after a justified choice, add bounded dispatch with atomic revalidation
   under the voice lock. Supply the actual Simplex decision/eligible recipients;
   consume() is merely a reservation, not delivery confirmation. Explicitly
   invalidate on timeouts/moves/disconnect/permission loss; define reclamation of
   ended channel records (currently retained, bounded at 128) and dispatch failure.
6. Preserve Busy sequence space and private routing; no bot connections. Test
   initial/final syllables and tone timing with unchanged Windows and Android
   clients before claiming audible ordering or production readiness.
7. Then finish opt-in configuration, preview asset management and installers.
   Preserve native 64739, Docker host 64740/container 64739, existing custom ports,
   .env, volumes and credentials. No existing installations may be touched.
8. Commit locally and regenerate/verify the complete binary patch and ZIP at each
   real milestone. Never call an archive inside this workspace an external backup.

No outstanding automated failure remains in the exercised checks. Unresolved:
audible architecture, live Open/Close, real-client/loss/long-duration/stress tests,
sanitizers, runtime settings and packaging. No new third-party code or dependencies,
no external audio resources, no changes to Busy waveform or client code were made.
