# Squelch Phase 2B — experimental development handoff

Repository: https://github.com/Shawna7han/mumble-funk
Original dev base: `f82f5aeda5111711b8e0c351e3f8b2400fc4a90a`.
Feature branch: `feature/squelch-audio`; PR target: `dev`.
Mumble server basis: 1.5.939. This is NOT a released or hearing-tested version.

## Current authorization and test strategy

The owner has explicitly authorized publication of this feature branch and a
**Draft** PR against dev. Earlier Phase 2A local-only restrictions in historical
commits have been superseded. No direct dev/stable changes, force pushes, merges,
releases, deployments, existing server updates or Docker-instance changes.
Use the GitHub No-Reply author/committer identity in AGENTS.md.

The owner now uses a dedicated Ubuntu Proxmox snapshot VM. Snapshot management and
installation require the owner's separate approval; Codex must not reset/install
it. Use the normal service `mumble-funk`, installation `/opt/mumble-funk`, config
`/etc/mumble-funk/server.ini`, state `/var/lib/mumble-funk`, log `/var/log/mumble-funk`.
Native: **64739 TCP/UDP**. Docker: **64740 TCP/UDP host → 64739 TCP/UDP container**.
Preserve operator overrides, .env, project names, volumes and all existing data.
The previous plan for a parallel Ubuntu preview on port 64742 is CANCELLED.
Do not create another preview service, installation root or parallel-instance setup.
Other VMs remain untouched. GitHub is now the requested development backup;
repeating the extensive local ZIP/patch export is no longer required.

## What is implemented

- Optional `squelch=false` by default, gated by `simplex=true` and `audiofeedback=true`.
- Actual Open/Close delivery through the existing private feedback transport.
- Separate WAV files and volumes, loaded/encoded at startup; no file IO or encoding
  in voice processing. Invalid files disable Squelch while Busy/speech continue.
- Lifecycle and recipient generations in SquelchPlan, shared with server dispatch.
- One AudioFeedback scheduler owns each recipient's effect sequence, with Busy
  priority and an independent Busy cooldown. No competing private output counters.
- Original speech payloads, session identities, sequence numbers and terminators
  are forwarded intact. No decoding, mixing, buffering or rewriting of speech.
- Original CC0 Open/Close assets preserved byte-for-byte; no third-party resources
  or new dependencies added. No client/protocol/bot changes or new audio ports.

## Architecture and important code paths

`Server::processMsg` first performs the existing Simplex and normal receiver/send
path. Only after forwarding does it validate the Opus header, take at most 64
actual receiver snapshots from AudioReceiverBuffer and call SquelchPlan::observe.
Self, deaf/self-deaf users, foreign channels, listeners and whispers follow the
existing Simplex receiver policy. The planner cannot grant a blocked transmission.

SquelchPlan receives accepted-packet/lifecycle information independently of tone
synthesis. Identities include connection generation, real session, channel and a
monotonic transmission generation. Session numbers alone are insufficient.
Open occurs on the first accepted valid nonempty Opus packet; standalone empty
markers do not start it. Close is produced on a confirmed end, even on a single
payload+terminator packet. It is consumed only after Open was consumed. Duplicate
or stale input remains subject to the existing Simplex decision.

Timeout, disconnect, channel changes and relevant mute/deaf/permission changes
invalidate optional work. Channel out-and-back must invalidate even if the final
channel ID is unchanged. New transmissions replace the channel generation, making
unsent old Close invalid. Sender disconnect or recipient session reuse cannot
redirect pending signals. Timeout is suspected, so it does not synthesize Close.

The Simplex event callback only invalidates on non-confirmed releases. Confirmed
release is reported *before* final forwarding by Simplex; Close must be scheduled
from the post-forwarding path, not directly from that callback. SimplexController
itself and its lock/ownership/retry rules are unchanged.

`AudioFeedback` sends pre-encoded signals through TCP UDPTunnel under the
recipient's **own existing session** (the existing Busy/server-loopback mechanism).
Voice retains the actual speaker session and can use TCP or encrypted UDP.
No fabricated identities or extra users are announced. Separate client decoders
avoid injecting independently encoded cues into the speech decoder.

A main-thread 20-ms timer revalidates source and recipient generation, channel,
authentication, Opus support, mute/deaf state and writable socket. With Squelch on,
it attempts the voice write lock without waiting, making validation plus delivery
atomic against concurrent UDP lifecycle changes. A busy voice path causes the
optional timer turn to yield. The planner and request enqueue also use try-lock
in the post-forwarding producer path. No user pointer survives delayed dispatch.
Lock order is voice → Simplex → planner on producers; dispatch uses voice → planner
without calling Simplex. Do not invert this order in future changes.

## Busy arbitration, sequences and timing

Sequences use Mumble's 10-ms units: each 20-ms packet advances by two. All private
signals to a connection use the same output sequence state, with a +100 sequence
gap between independent clips (this is NOT a client playback clock). Preserve that
state across channel cancellation when Squelch is enabled; a move is not a new
session. Idle records expire after 60 seconds. A new connection gets new state.

Busy has a separate 128-entry producer queue, so optional cues cannot fill it.
An active optional signal is ended with its next nonempty packet/terminator when
Busy arrives. Busy then follows a 60-ms packet-duration/retirement gap, normally
within about 80 ms, with all original Busy packets. No Busy packets are shortened
or replaced. Squelch queued behind active/waiting Busy is omitted. Busy waveform,
custom WAV, gain and one-second cooldown remain intact. The old Busy-only path is
preserved when Squelch is disabled. This is best-effort audio, not a timing guarantee
under a stalled event loop or client jitter. Effects may be omitted under overload.

Bounds: 128 optional queued cues, 64 playback recipients, 128 lifecycle channels,
64 recipients per transmission. Ended planner records remain until invalidated or
replaced; capacity exhaustion omits new effects, not speech. A production expansion
may need smarter bounded eviction. No burst catch-up after timer stalls.

Open must start within 100 ms; Close may start within 300 ms. These are server queue
expiry bounds, not measured device latency. Once started, each packet is checked
against the current lifecycle. Already delivered client-buffered audio cannot be
recalled. A short grace gap is a pragmatic measure, not proof of perfect jitterbuffer
retirement on every client. Rapid handoffs can omit optional cues or overlap them
with speech. Real-device audibility remains the acceptance gate.

## Opus lookahead and original assets

Open: 48 kHz/16-bit/mono, 897 samples / 18.6875 ms.
SHA-256 `81573e3a11db089de7cf2bbb2918e0747a2edde12923c399ac5042513d038770`.
Close: 48 kHz/16-bit/mono, 3122 samples / 65.0416667 ms.
SHA-256 `8d9091e0e4ee36433eb8aa707a7fa4a19ccec8ce5dee85ef1d91fa478afd0e31`.
Both are supplied by the maintainer under CC0; see assets/audio/LICENSE.md.

`AudioSignal::encodeWavSignal` uses the existing encoder configuration and queries
OPUS_GET_LOOKAHEAD instead of blindly adding 960 samples. The current encoder reports
312 samples / 6.5 ms. Open needs 897+312=1209 samples, still **two packets / 40 ms**.
A single packet would omit 249 nonzero source samples; the tail energy is small
(about 0.365% of source energy) but not zero. No tail truncation, speed change or
alternative encoder mode was forced. Open encoding is identical to the previous
two packets. Close now uses **four packets / 80 ms**, instead of five / 100 ms;
the retained decoded prefix is identical. Busy uses its unchanged original API.
No speech delay is added to wait for the 40-ms Open.

Phase 2A found small PCM differences when prepending independently encoded Opus
to the same speech decoder, and independent clocks across Speex buffers. Those
measurements do not prove audible speech damage. The chosen separate private
feedback stream avoids that speech-state splice; it deliberately accepts overlap.
No further mixing/architecture experiment is required before real-client trials.

## Configuration and installation status

See [squelch-preview.md](squelch-preview.md) for the configuration contract and tests.
Defaults: squelch=false; squelchopenfile/squelchclosefile empty;
squelchopenvolume/squelchclosevolume=50 (clamped 0–100). Files must be PCM WAV,
mono, signed 16-bit LE, 8/16/24/48 kHz, 1–1000 ms, at most 128 KiB. Missing/invalid
files disable Squelch with a startup log; there is no synthetic Squelch fallback.
Both paths should be absolute and readable by the service account. File/volume
changes need a server restart, not recompilation. No hot reload or management
command for Squelch exists yet. AudioFeedback=false disables all signals.

The normal installer/source guard still rejects this feature branch. It accepts
stable, funk-v0.1 and published version tags. Do not disguise the experimental
branch as stable. Feature/version selection for the normal snapshot-VM installation,
asset installation/update retention and friendly WAV management are still Sol work.
Docker packaging/configuration of Squelch assets is also pending; defaults and
existing ports/volumes were NOT modified. No test-VM installation was performed.

## Tests performed and actual results

Build: GCC 14, Qt 5.15.15, Opus 1.5.2, Release, BUILD_NUMBER=939,
client=OFF, server=ON, tests=ON, ICE=ON, plugins/overlay/LTO=OFF.
Pinned existing dependencies reused; no new third-party package or source code.

- Full CTest run after the two fixes: **23/23 programs passed**, 111.08 seconds.
  Includes existing Simplex/Busy integrations and disabled-feature passthrough.
- TestSquelchIntegration: all eight legacy/protobuf × TCP/encrypted-UDP ingress ×
  TCP/encrypted-UDP voice egress combinations. Signals use authenticated TCP.
  Checks every first/last speech payload, source, sequence and marker, exact
  decoded speech samples, signal decoding, expected audience and private Busy.
- Confirmed empty end and one-packet transmission (Open + Close), suspected timeout,
  sender/recipient channel out-and-back, disconnect, receiver deaf-and-return,
  actual disconnected session number reuse, explicit/master disable and bad WAVs.
- New fast-handoff cases subsequently added for TCP and encrypted UDP: alternating
  accepted speakers, all original voice packets preserved, private sequence order
  and decodability. See the final validation addendum for their repeat results.
- Foundation tests cover generation invalidation, rapid replacement, expiry,
  bounded/concurrent state, malformed assets, actual lookahead, separate volume,
  Busy arbitration/packet preservation and decoded signal prefix equality.
- Additional Python suites: native installer **5**, source guard **14**, setup **16**,
  audio security **13**, Docker helper tests **11**, WAV/admin **8**, existing audio
  preview regressions **5**: **72 checks passed**. They use temporary roots/servers.
  Python 3.13 printed nonfatal SQLite ResourceWarnings in existing Docker helper
  tests; no failure. These are not Docker runtime or VM installation tests.

Failures were kept visible and fixed, not skipped:
1. Close on a single-packet transmission expired after 100 ms, before Open's playback
   and retirement gap had elapsed. Close now has its own bounded 300-ms start limit;
   Open retains 100 ms. The test still requires all two Open and four Close packets.
2. The reuse test used graceful TLS disconnect while waiting only on another client;
   its own socket had not completed closing. Use socket.abort() to model an actual
   abrupt disconnect, then require UserRemove and actual numeric session reuse.
   No server disconnect or authentication logic was changed.

Local logs are under /workspace/work/squelch-backup/phase2b-*.log. Typical commands:

```sh
. /workspace/.cloud-deps/funk-v0.1/activate.sh
cmake --build /workspace/build/squelch --parallel 2
ctest --test-dir /workspace/build/squelch --output-on-failure
ctest --test-dir /workspace/build/squelch -R '^TestSquelchIntegration$' --repeat until-fail:3 --output-on-failure
python3 tests/funk-install/test_audio_assets.py /workspace/build/squelch/mumble-server
```

Windows-Mumble/Android-Mumla hearing tests, arbitrary network loss/jitter, long-term
load, sanitizers and the Proxmox snapshot installation have NOT been performed.
Synthetic voiced harmonics with a syllabic envelope were used as decodable input;
these are not recordings or proof of human perception. No claim of production readiness.

## Next tasks for GPT-6.1 Sol Hoch

1. Check the Draft PR and CI at the exact current head; keep it experimental and
   unmerged. Read this handoff and the operator document before editing.
2. Complete normal Ubuntu installation on the owner-managed clean snapshot VM:
   explicit authorized source selection, standard mumble-funk service and paths,
   port 64739, reproducible asset installation. No port 64742 or parallel layout.
3. Extend management to set/reset Open/Close WAVs and volumes safely, preserving
   unrelated configuration. Retain startup validation and restart semantics.
4. Add Docker asset/configuration support using the existing image/Compose project,
   ports 64740→64739 TCP/UDP, existing .env and volumes; no automatic data migration.
5. After owner authorization, test with unchanged Windows and Android clients:
   first/last syllables, effect audibility/level, quick PTT, Busy priority, channel
   transitions and reconnect. Compare with Squelch disabled. No remote VM actions
   without permission. Fix observed faults on this feature branch, not stable/dev.
6. Retain lifecycle/recipient separation for later continuous noise. A future
   renderer/mixer can extend or replace finite-clip rendering using the accepted
   transmission and invalidation events plus the shared feedback sequence owner.
   Different signal files/gains already exist. Do not implement noise/mixing now.
7. Get explicit practical approval before changing PR readiness or merging.

## Final local validation addendum

The extended TestSquelchIntegration, including fast TCP/encrypted-UDP handoffs,
passed **three complete consecutive runs**, 96.82 seconds in total. No test was
removed, skipped or relaxed to hide a server failure. The full 23-program run
preceded these two additional cases; production code did not change afterward.
All 72 additional Python checks also completed successfully. CI results must be
checked separately on GitHub; local success is not a claim of completed Actions.


## First published CI: Docker build-context correction

At published commit 96db44a980efba232eee5ec38537054df2356997, GitHub's native
server/installation tests and CodeQL passed. The Docker build compiled successfully
but failed its SquelchFoundation/Integration tests because .dockerignore excluded
all WAV files, including the two required public CC0 fixtures. Missing assets made
the test server disable optional Squelch. This was a build-context packaging error,
not evidence of a speech transport failure. The Docker runtime integration step
could not run after that build failed.

The feature branch now explicitly allows only assets/audio/squelch-open.wav and
assets/audio/squelch-close.wav into the build context; arbitrary private WAVs remain
excluded. No test assertions, runtime audio, ports or data paths were changed.
The existing two failing test programs are the regression checks in Docker's build
stage. Check the subsequent GitHub run at the branch's new head for final results.

The correction was checked with real local Docker/BuildKit scratch builds using
the original and corrected ignore files. Before: both fixtures absent. After:
both fixtures byte-identical to the originals. Dummy private WAV, .env and key
files remained excluded in both contexts. This verifies context selection without
repeating the complete server compilation; the GitHub build reruns full tests.
