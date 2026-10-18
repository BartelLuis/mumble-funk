# Mumble-Funk

**Server-side simplex radio communication for Mumble**

Mumble-Funk is an independent, open-source fork of the Mumble server, based on
**Mumble Server 1.5.939**. It adds optional radio-style channel access: one user
transmits at a time, and the server enforces the rule for unchanged Mumble clients.

Ordinary Mumble channels allow several people to speak at once. Mumble-Funk provides
a shared radio channel where the first accepted sender holds the right to transmit
and competing transmissions are discarded.

**Experimental alpha — Simplex Engine v0.2.** A working prototype is available;
production readiness and long-term reliability have not been established.

[Deutsch](README.de.md) · [Install on Ubuntu](docs/funk/installation-ubuntu.md) ·
[Enable Simplex / update](docs/funk/simplex.md) ·
[Verification report](docs/funk/SIMPLEX-STATUS.md)

## Project at a glance

| Item | Current status |
| --- | --- |
| Development branch | [`funk-v0.1`](https://github.com/Shawna7han/mumble-funk/tree/funk-v0.1) |
| Server baseline | Mumble 1.5.939, official tag `v1.5.939` |
| Radio engine | Simplex v0.2, implemented; **disabled by default** |
| Supported installation targets | Ubuntu Server **24.04 LTS** and **26.04 LTS** |
| Clients | Existing Mumble clients; focus on Mumla for Android and Mumble for Windows |
| Parallel installation | Funk server on **64739 TCP/UDP**, separate from standard Mumble on 64738 |
| License | [BSD 3-Clause](LICENSE), with existing third-party notices retained |

## What works today

| Feature | Implemented behavior |
| --- | --- |
| Exclusive transmission | One authorized sender per channel; different channels operate independently. |
| Concurrent attempts | Atomic allocation shared by UDP and TCP prevents two simultaneous owners. |
| Rejected transmissions | A continuously arriving rejected transmission stays blocked when the channel becomes free. A new attempt is required. |
| End markers | A valid marker from the current owner releases the channel; another user's marker cannot release it. |
| Configurable timeouts | Default channel timeout: **500 ms** without valid new packets. Default quiet interval for inferring a new attempt: **1,000 ms**. |
| Lifecycle handling | Channel changes, detected disconnects and loss of speaking permission release ownership. Ownership is not persisted across restarts. |
| Transport | Incoming UDP audio and audio tunneled over TCP use the same Simplex control. |
| Existing clients | No client modifications, protocol extensions or additional client messages are required. |
| Server diagnostics | Optional voice-packet diagnostics and separate Simplex grant/denial/release diagnostics; metadata only, no recorded speech. |
| Optional operation | With Simplex disabled, normal Mumble voice routing remains available. |

Audio payloads are forwarded without decoding and re-encoding for the radio rules.
Diagnostic file output runs in background threads with bounded queues; records may
be dropped under load without changing transmission ownership.

### A simple radio example

1. Alice starts transmitting in a free channel. The server accepts her first
   eligible packet and gives her the channel.
2. Bob presses PTT while Alice is transmitting. His packets are discarded.
3. Alice stops. Her end marker, or the configured packet timeout, releases the channel.
4. Bob's continuing packet stream **does not automatically become audible**.
   He must end that attempt and start a new one.

The server observes packets, not physical buttons. Without an end marker, a new
attempt is inferred only after the configured quiet interval. For initial tests,
release PTT, wait about two seconds, then press again. A sufficiently long network
gap can also look like a new attempt; this cannot be resolved exactly without
client or protocol changes.

## Client compatibility and test evidence

| Client | Evidence so far |
| --- | --- |
| **Mumla — Android** | Initial operator-reported tests confirmed connection, two-way voice and PTT packet behavior. The observed transmissions continued through speaking pauses; three observed attempts had no confirmed end marker. |
| **Mumble Desktop — Windows** | Initial operator-reported tests confirmed connection, two-way voice and PTT packet behavior, including an observed end marker on release. |
| Other Mumble clients | The existing protocol is retained, but broad client/version compatibility has not been practically validated. |

These initial device observations informed the implementation. They are **not a
completed real-device validation of Simplex v0.2** and do not guarantee identical
behavior across client versions or networks.

The recorded automated verification includes **18/18 server test programs** and
**5/5 installation tests**. Targeted Simplex and existing PTT-diagnostic tests also
passed after the development environment was resumed. Integration tests use a real
server process and simulated clients to check actual packet forwarding, UDP/TCP
competition, legacy and newer packet formats, lifecycle events and disabled-mode
routing. They do not test sound playback on an Android or Windows device.

See the [verification report](docs/funk/SIMPLEX-STATUS.md) for test dates,
environments, coverage and remaining checks. These are recorded results, not a
claim of continuously passing CI.

## Known limitations

- **Physical PTT is not directly visible to the server.** Missing end markers,
  delayed packets and long network interruptions can make transmission boundaries
  ambiguous.
- Unchanged clients have **no reliable native busy indication** for a
  server-rejected transmission. Their local transmit indicator may still light up.
- Rapid PTT changes are not fully optimized. Short pauses without an end marker,
  or sequence resets, may remain blocked until enough packet silence is observed.
- In enabled radio mode, speech stays inside the sender's current channel.
  **Whisper, linked-channel forwarding, extra channel-listener reception and server
  loopback do not provide alternative voice routes.** Their saved settings or
  client UI may still exist.
- No integrated radio audio effects, busy tone, roger beep or squelch are implemented.
- Practical testing is limited; **no completed long-term or load test** establishes
  production reliability.

## Installation and documentation

Use the existing guides below; they contain the maintained commands rather than
an additional installation recipe here. The operational guides are currently
**in German**. The supported installation targets are Ubuntu Server 24.04 LTS and
26.04 LTS, including point releases.

The supplied installation runs `mumble-funk.service` on **64739 TCP/UDP** with its
own configuration, database and logs. It is designed to coexist with an existing
standard Mumble server on 64738. Existing Mumla and Mumble clients connect to the
additional server without modification.

| Task | Guide |
| --- | --- |
| First installation, prerequisites and systemd service | [Ubuntu installation](docs/funk/installation-ubuntu.md) |
| Update, enable Simplex, understand settings or roll back | [Simplex v0.2 guide](docs/funk/simplex.md) |
| Test with Android, Windows and an optional third observer | [Practical test plan](docs/funk/simplex.md#6-praxistest-mit-android-und-windows) |
| Inspect incoming packet timing and end markers | [PTT / voice diagnostics](docs/funk/diagnose.md) |
| Review automated verification and its limits | [Implementation and test status](docs/funk/SIMPLEX-STATUS.md) |
| Understand synchronization and ownership | [Server locking notes](docs/dev/MurmurLocking.md#optional-simplexcontroller-funk-v02) |

**Installing an update does not enable Simplex.** Activation is a separate server
configuration step described in the Simplex guide; configuration changes require
a restart of the Funk service.

## Roadmap

| Stage | Scope |
| --- | --- |
| **Implemented** | Simplex Engine v0.2, optional server-side diagnostics and the parallel Ubuntu installation. |
| **Current development goals** | Broader real-device tests; improve rapid PTT changes within protocol limits; examine packet loss, delayed packets and network outages; strengthen stability and reliability through longer runs. |
| **Possible future work — not committed** | Busy tone, roger beep, analog radio audio effects, squelch, digital radio profiles and extended configuration. |

Future ideas are exploratory. They have no promised delivery date or guaranteed
implementation and are not features of the current prototype.

## Contributing and reporting issues

Report fork-specific problems in [Mumble-Funk issues](https://github.com/Shawna7han/mumble-funk/issues).
Include the server commit, client/platform versions, enabled settings and steps to
reproduce the behavior. Diagnostic excerpts can help; remove passwords and personal
information before sharing them. See the [commit guidelines](COMMIT_GUIDELINES.md)
for code contributions.

## Upstream and license

Mumble-Funk is an **independent project**, derived from
[Mumble](https://github.com/mumble-voip/mumble). It is not an official Mumble release
and does not claim endorsement or support from the original Mumble developers.
The upstream project's website is [mumble.info](https://www.mumble.info/).

> Copyright (C) 2005-2022 The Mumble Developers
>
> All rights reserved.

The source is distributed under the terms in [LICENSE](LICENSE) (BSD 3-Clause).
Original copyright and license notices remain in place. Bundled components retain
their own terms; see [3rdPartyLicenses](3rdPartyLicenses/) and the notices in the
respective source files and submodules.
