# Mumble-Funk

**Server-side simplex radio communication for Mumble**

**This branch: experimental standard Busy tone (`feature/busy-tone-asset`).**
Includes the merged Squelch and Audio Boost work. New Ubuntu/Docker configurations
select the maintainer-provided CC0 Busy WAV; updates preserve existing selections,
custom files and settings. Busy accepts 20–1250 ms; Squelch limits and speech
forwarding stay unchanged. Effects default **off**. No release or device hearing-test
approval. Native installation requires `--experimental-busy-tone`.
[Install/update, tone selection and hearing test (German)](docs/funk/busy-tone.md) ·
[Squelch behaviour and WAV management](docs/funk/squelch-preview.md) ·
[Technical handoff](docs/funk/SQUELCH-DEVELOPMENT-HANDOFF.md).

New Docker installations from this development candidate default to **host 64740
TCP/UDP → container 64739 TCP/UDP**. Native Ubuntu remains **64739 TCP/UDP**.
Explicit existing/custom host ports are preserved. Older installations without an
explicit port must pin their current port before updating; see
[update protection and rollback](docs/funk/docker.md#9-aktualisieren).
The following installation overview describes the separate, unchanged Stable candidate;
its historical Docker fallback is still 64739.

**Stable candidate — proposed release `funk-v0.2.1`.** Ubuntu and Docker now share
one source revision on [`stable`](https://github.com/Shawna7han/mumble-funk/tree/stable).
Simplex Engine v0.2 remains an experimental alpha; final Docker acceptance on the
operator's host is still required. No new version tag or release is published.
`dev` is the development branch; `master` remains the historical upstream reference.

[Deutsch](README.de.md) · [Integration evidence](docs/funk/STABLE-INTEGRATION-STATUS.md) ·
[Development](docs/funk/development.md) · [Release plan](docs/funk/release-plan.md)

Mumble-Funk is an independent, open-source fork of the Mumble server, based on
**Mumble Server 1.5.939**. It adds optional radio-style channel access: one user
transmits at a time, and the server enforces the rule for unchanged Mumble clients.

Ordinary Mumble channels allow several people to speak at once. Mumble-Funk provides
a shared radio channel where the first accepted sender holds the right to transmit
and competing transmissions are discarded.

**Experimental alpha — Simplex Engine v0.2.** A working prototype is available;
production readiness and long-term reliability have not been established.

[Deutsch](README.de.md) · [Automatic setup](docs/funk/setup.md) ·
[Enable Simplex / update](docs/funk/simplex.md) ·
[Verification report](docs/funk/SIMPLEX-STATUS.md)

## Choose an installation

| Route | Source | Operation | New installation defaults |
| --- | --- | --- | --- |
| Ubuntu Server 24.04 / 26.04 LTS | `stable` | Dedicated `mumble-funk` systemd service | Host 64739 TCP/UDP, Simplex enabled, timeout/retry 500/500 ms |
| Docker Compose v2 on Linux | The same `stable` commit | Rootless container, persistent named volumes | Container 64739 TCP/UDP; host port configurable; Simplex and timeout/retry identical |

[Docker installation and backup](docs/funk/docker.md) ·
[Safe migration from the Docker feature branch](docs/funk/docker-stable-migration.md).
Use host **64740** to run Docker alongside the native Funk server on 64739.
Existing `.env`, project name, service and volume names remain compatible.
Do not change the Compose project name during an update.

Both builds use Mumble 1.5.939 plus the same Funk sources. Record `git rev-parse HEAD`
for an exact version; the Mumble version alone does not identify a Funk release.
Audio Feedback is **not included in Stable**. It is integrated experimentally in
`dev` and this development candidate; use the [current feature guide](docs/funk/busy-tone.md).

## Quick installation — Ubuntu 24.04 / 26.04 LTS

Run as a normal user with **sudo access**, not as root. You need Internet access,
at least 2 GiB RAM including swap (4 GiB RAM recommended), and 8 GiB free disk space.

```bash
sudo apt update && sudo apt install -y git

git clone --branch stable --recurse-submodules \
  https://github.com/Shawna7han/mumble-funk.git

cd mumble-funk
bash scripts/funk/setup.sh
```

The setup installs missing dependencies, builds the existing source, runs all
server tests, and deploys only after they pass. A **new installation enables
Simplex automatically** on **64739 TCP/UDP**, with 500 ms channel and retry timeouts.
Existing configurations, passwords, databases and certificates are preserved.

After completion, retrieve connection details and the random access password locally:

```bash
sudo mumble-funk-info
```

Firewall/router rules are not changed. For an occupied port, updates, troubleshooting
and safe removal, see the [setup guide](docs/funk/setup.md) (German). If the source
directory already exists, use its update instructions instead of cloning over it.

## Project at a glance

| Item | Current status |
| --- | --- |
| Common installation source | [`stable`](https://github.com/Shawna7han/mumble-funk/tree/stable), candidate pending final acceptance |
| Development | [`dev`](https://github.com/Shawna7han/mumble-funk/tree/dev); future changes through pull requests |
| Historical Simplex / Ubuntu branch | [`funk-v0.1`](https://github.com/Shawna7han/mumble-funk/tree/funk-v0.1), unchanged |
| Server baseline | Mumble 1.5.939, official tag `v1.5.939` |
| Radio engine | Simplex v0.2, implemented; core default off, **new automated installations enable it** |
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
| Configurable timeouts | Channel timeout: **500 ms** without valid new packets. Core retry default: **1,000 ms**; a new `setup.sh` installation configures **500 ms**. Both are configurable. |
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

These initial observations informed the implementation. The operator has since
reported successful initial Simplex v0.2 tests with Android-Mumla and Windows-Mumble
on Ubuntu Server 26.04.1. This remains limited practical evidence, not a comprehensive
client/version/network validation or a completed long-term test.

The historical v0.2 verification includes **18/18 server test programs** and
**5/5 installation tests**. Targeted Simplex and existing PTT-diagnostic tests also
passed after the development environment was resumed. Integration tests use a real
server process and simulated clients to check actual packet forwarding, UDP/TCP
competition, legacy and newer packet formats, lifecycle events and disabled-mode
routing. They do not test sound playback on an Android or Windows device.

The new integration is checked separately in the [integration report](docs/funk/STABLE-INTEGRATION-STATUS.md), including the authenticated UDP nonce regression and removal of Docker test workarounds.

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
- Stable has no audio effects. This feature branch adds optional private Busy
  and experimental Squelch Open/Close. Roger beep and background noise are not implemented.
- Practical testing is limited; **no completed long-term or load test** establishes
  production reliability.

## Installation and documentation

The quick commands above use the automatic setup. The existing manual installation
guide remains available for the technical deployment details. The operational
guides are currently **in German**. The supported installation targets are Ubuntu Server 24.04 LTS and
26.04 LTS, including point releases.

The supplied installation runs `mumble-funk.service` on **64739 TCP/UDP** with its
own configuration, database and logs. It is designed to coexist with an existing
standard Mumble server on 64738. Existing Mumla and Mumble clients connect to the
additional server without modification.

| Task | Guide |
| --- | --- |
| Docker installation, persistence, backup and recovery | [Docker guide](docs/funk/docker.md) · [Stable migration](docs/funk/docker-stable-migration.md) |
| Automatic setup, connection details, updates and removal | [Simple setup guide](docs/funk/setup.md) |
| Manual installation, prerequisites and systemd details | [Ubuntu installation](docs/funk/installation-ubuntu.md) |
| Update, enable Simplex, understand settings or roll back | [Simplex v0.2 guide](docs/funk/simplex.md) |
| Test with Android, Windows and an optional third observer | [Practical test plan](docs/funk/simplex.md#6-praxistest-mit-android-und-windows) |
| Inspect incoming packet timing and end markers | [PTT / voice diagnostics](docs/funk/diagnose.md) |
| Review automated verification and its limits | [Simplex verification](docs/funk/SIMPLEX-STATUS.md) · [Setup verification](docs/funk/SETUP-STATUS.md) |
| Understand synchronization and ownership | [Server locking notes](docs/dev/MurmurLocking.md#optional-simplexcontroller-funk-v02) |

**Existing installations keep their settings**, including a previously disabled
Simplex mode. For deliberate configuration changes, use the Simplex guide.
The manual `install.sh` path retains its opt-in default; only new installations
through `setup.sh` automatically enable Simplex.

## Roadmap

| Stage | Scope |
| --- | --- |
| **Implemented** | Simplex Engine v0.2, optional diagnostics, Ubuntu setup and Docker Compose from a common commit. |
| **Current development goals** | Broader real-device tests; improve rapid PTT changes within protocol limits; examine packet loss, delayed packets and network outages; strengthen stability and reliability through longer runs. |
| **Experimental audio branch** | Existing private busy tone integrated; disabled by default. |
| **Possible future work — not committed** | Roger beep, analog radio audio effects, squelch, digital radio profiles and extended configuration. |

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
