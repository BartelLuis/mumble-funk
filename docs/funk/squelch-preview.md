# Squelch Phase 2B — local experimental preview

This experimental preview is developed on `feature/squelch-audio`, based on dev
`f82f5aeda5111711b8e0c351e3f8b2400fc4a90a`. It is not a published release.
No existing server installation is updated by this work.

## Behaviour

With Simplex and audio feedback enabled, each eligible listener can receive Open
at the start of an accepted Opus transmission and Close after a confirmed end.
The sender receives neither of their own transmission's cues. Other channels,
linked-channel listeners and whispers do not receive these cues.

Voice packets are forwarded first, with their payloads, sender session, sequence
and end markers intact. Signals use the listener's **existing own session**, the
same private feedback mechanism already used for Busy. No bots or fabricated
sessions, client changes, additional audio ports or speech transcoding are used.
Signals travel over the existing TCP connection even when speech uses encrypted
UDP. Speech and the private effect have independent client decoders/jitter buffers.
Some overlap or a late Open is expected; this is not sample-synchronized mixing.

Busy keeps its waveform, file, gain, full packet sequence and one-second cooldown.
One scheduler owns each recipient's feedback sequence. Busy takes priority over
Squelch. An active optional cue ends at its next packet boundary; Busy follows a
short retirement gap (normally within about 80 ms). This avoids injecting Busy
into an unterminated cue. Queued Squelch for a recipient currently hearing Busy
is omitted. The existing Busy-only path retains its behaviour when Squelch is off.

Close requires an Open dispatch and a confirmed end marker. Timeout, disconnect,
channel changes, deaf/mute/permission changes invalidate optional work. A new
transmission cancels the previous generation's unsent cues. Audio already buffered
on a client cannot be recalled. Late/congested cues are omitted, never speech.
No more than 128 pending requests per signal queue, 64 playback recipients and
128 lifecycle channel records are retained. Ended records are bounded but retained
until replacement/invalidation; this preview is not a large-scale capacity release.

## Assets and the 20-ms question

Original files in `assets/audio/` remain byte-identical, licensed under CC0 by the
project owner. No Internet samples or new third-party dependencies were added.

The existing Opus audio encoder has 312 samples (6.5 ms at 48 kHz) of lookahead.
The 897-sample Open therefore needs 1209 output samples to preserve its full tail:
two 20-ms packets, **40 ms**, remain necessary with this encoder configuration.
Merely sending the first packet would omit the final 249 original samples; their
energy is small but nonzero. We do not silently truncate them or change playback
speed. This 40-ms signal does **not** impose a 40-ms delay on forwarded speech.

Signal encoding now pads only the lookahead reported by Opus, instead of a whole
extra 20-ms input frame. The supplied Close uses **four packets / 80 ms**, reduced
from five packets / 100 ms. Busy encoding is unchanged. Runtime WAVs are encoded
once at startup, not loaded or encoded in the voice forwarding path.

## Configuration for the dedicated snapshot test VM

The owner manages Proxmox snapshots and authorizes any installation separately.
Codex must not reset or install the VM automatically. Other VMs stay untouched.
The target is the normal native layout: service `mumble-funk`, `/opt/mumble-funk`,
`/etc/mumble-funk/server.ini`, `/var/lib/mumble-funk`, `/var/log/mumble-funk`,
port **64739 TCP/UDP**. No new preview service/directory or port 64742 is needed.
Docker remains **host 64740 TCP/UDP → container 64739 TCP/UDP**; preserve custom
host ports, `.env`, project names, volumes, passwords and certificates.

This phase implements server configuration but does NOT yet integrate the new
assets with setup/update commands. The normal source guard still accepts stable,
funk-v0.1 and published version tags, not this experimental feature branch.
Do not bypass it by relabeling this branch as stable. Sol's next task is explicit
feature/version installation support on the clean snapshot VM, plus WAV management
and Docker asset delivery. No new parallel-preview installer should be created.

For that later installation, place the original files in a service-readable
normal asset directory such as `/etc/mumble-funk/sounds/`. The following is the
configuration contract (not an instruction to modify an existing installation):

```ini
simplex=true
audiofeedback=true
squelch=true
squelchopenfile=/etc/mumble-funk/sounds/squelch-open.wav
squelchclosefile=/etc/mumble-funk/sounds/squelch-close.wav
squelchopenvolume=50
squelchclosevolume=50
```

INI keys belong before any section header, without duplicates. `squelch` defaults
to false; file paths default empty and both volumes to 50. Existing `busytone`,
`busytonefile` and `busytonevolume` settings continue to apply. Squelch can also
operate with `busytone=false`. Volume is separately clamped to 0–100 per cue;
0 is silent. Peak handling only attenuates, capped at 0.2 PCM before volume.
Supported files: PCM WAV, mono, signed 16-bit LE, 8/16/24/48 kHz, 1–1000 ms,
maximum 128 KiB. Use absolute paths. Invalid/missing files disable Squelch with a
startup log message; ordinary speech and Busy continue. There is no synthetic
Squelch fallback; the Busy fallback remains unchanged.

After separately authorized setup/start, `Experimental Squelch enabled` confirms
successful loading. WAV/volume changes require restarting that test server, but
no recompilation. `squelch=false` disables Squelch; `audiofeedback=false` disables
all effects. There is no hot reload. No extra audio network ports are introduced.

## Practical check with unchanged clients

Use at least two clients in the same channel and optionally one in another.
Transmit a sentence with clear first and last syllables. Check Open/Close at the
listener, no own cue at the sender, no cue in the other channel. While one client
holds the channel, press PTT on another: Busy must remain private and the blocked
client must not automatically take over. Repeat short presses, channel changes,
and reconnects. Compare `squelch=false` with `squelch=true`, and test normal UDP
and the client's TCP-only option if available.

Automated tests cannot establish what a physical device actually sounds like.
No Windows/Android device hearing test, long-term load test or arbitrary loss/jitter
qualification has been performed for this preview. Do not call it production-ready.

## Later continuous noise / mixing

`SquelchPlan` owns accepted transmission/recipient generations; `AudioFeedback`
currently renders finite clips. Signal kind, WAV selection and gain are separate
from Simplex ownership. A later optional renderer can consume the same lifecycle
and recipient validation to mix or maintain a continuous effect stream, sharing
feedback sequence ownership. It must still invalidate on transmission end/moves
and coexist with private Busy. No background noise, loop playback or mixing engine
is implemented now; disabling effects always leaves direct voice forwarding.
