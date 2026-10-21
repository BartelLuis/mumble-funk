# Mumble-Funk

**Serverseitige Simplex-Funkkommunikation für Mumble**

**Dieser Branch: experimentelle Audio-Defaults (`feature/audio-defaults`).**
Neue Ubuntu- und Docker-Konfigurationen aktivieren Simplex, den mitgelieferten
CC0-Busy-Tone und Squelch Open/Close bei **50 % und +8 dB Boost**. Timeout-Close
ist bei Neuinstallationen eingeschaltet. Updates erhalten bestehende Einstellungen und eigene WAVs, auch
fehlende INI-Einträge und ausgeschaltete Effekte. Sprache, Simplex-Regeln und
Audioarchitektur bleiben unverändert. Native Installation nur ausdrücklich mit
`--experimental-audio-defaults`.
[Installation, Update und Test](docs/funk/audio-defaults.md) ·
[Busy-Tone und Fallback](docs/funk/busy-tone.md) ·
[Squelch-Verhalten](docs/funk/squelch-preview.md).

Neue Docker-Installationen dieses Entwicklungsstands verwenden **Host 64740
TCP/UDP → Container 64739 TCP/UDP**. Ubuntu nativ bleibt auf **64739 TCP/UDP**.
Vorhandene ausdrücklich gesetzte Host-Ports bleiben erhalten. Alte Installationen
ohne feste Portzeile müssen vor dem Update ihren bisherigen Port festhalten:
[Bestandsschutz und Rollback](docs/funk/docker.md#9-aktualisieren).
Die folgende Übersicht bezieht sich auf den separaten, unveränderten Stable-Kandidaten;
dessen historischer Docker-Fallback bleibt 64739.

**Gemeinsamer Stable-Kandidat; Versionsvorschlag `funk-v0.2.1`.** Ubuntu und Docker
verwenden auf `stable` denselben Quellstand. Die Simplex-Engine bleibt v0.2 und
experimentell. Die abschließende Docker-Abnahme auf dem Rechner des Kollegen und
die Freigabe stehen noch aus; es gibt keinen neuen Release oder Versionstag.
`dev` ist für Weiterentwicklung vorgesehen, `master` bleibt historische Upstream-Referenz.

[Prüfbericht der Integration](docs/funk/STABLE-INTEGRATION-STATUS.md) ·
[Entwicklungsstruktur](docs/funk/development.md) · [Releaseplan](docs/funk/release-plan.md)

Mumble-Funk ist ein unabhängiger Open-Source-Fork des Mumble-Servers auf Basis von
**Mumble Server 1.5.939**. Er ergänzt einen optionalen Funkmodus: Pro Kanal darf
nur ein berechtigter Benutzer gleichzeitig senden. Alle Regeln setzt der Server
durch. **Mumla auf Android und Mumble auf Windows bleiben unverändert.**

Normale Mumble-Kanäle lassen mehrere gleichzeitige Sprecher zu. Mumble-Funk bildet
einen gemeinsam genutzten Funkkanal nach, in dem der erste akzeptierte Sender das
Senderecht erhält und konkurrierende Aussendungen verworfen werden.

**Status: experimenteller Alpha-Prototyp mit Simplex-Engine v0.2.** Die Funktion
ist implementiert und automatisiert getestet. Eine Produktionsreife oder dauerhaft
zuverlässiger Betrieb unter allen Netzbedingungen ist damit noch nicht belegt.

[English / Hauptvorstellung](README.md) ·
[Automatische Installation](docs/funk/setup.md) ·
[Simplex aktivieren und testen](docs/funk/simplex.md)

## Ubuntu oder Docker?

Beide Wege bauen denselben Git-Commit; `git rev-parse HEAD` zeigt seine eindeutige
Kennung. Neue Installationen aktivieren Simplex mit 500 ms Timeout und 500 ms
Retry-Timeout. Unveränderte Mumla-/Windows-Mumble-Clients verbinden sich damit.
Stable enthält kein Audio-Feedback. Auf diesem Entwicklungsbranch ist der
private Busy Tone optional enthalten; dafür die [aktuelle Feature-Anleitung](docs/funk/busy-tone.md) verwenden.

- [Ubuntu-Setup](docs/funk/setup.md): Ubuntu Server 24.04 oder 26.04 LTS, Dienst
  `mumble-funk`, üblicherweise 64739 TCP/UDP.
- [Docker-Installation](docs/funk/docker.md): Container intern 64739 TCP/UDP;
  beim Parallelbetrieb Host-Port 64740 wählen.
- [Bestehenden Docker-Server sicher auf Stable umstellen](docs/funk/docker-stable-migration.md):
  Sicherung, gleicher Projektname und gleiche Volumes, Prüfung und Rückkehr.

## Schnellinstallation — Ubuntu 24.04 / 26.04 LTS

Als normaler Benutzer mit **sudo-Rechten** starten, nicht als root. Erforderlich
sind Internetzugang, mindestens 2 GiB RAM einschließlich Swap (4 GiB RAM empfohlen)
und 8 GiB freier Speicher.

```bash
sudo apt update && sudo apt install -y git

git clone --branch stable --recurse-submodules \
  https://github.com/Shawna7han/mumble-funk.git

cd mumble-funk
bash scripts/funk/setup.sh
```

Das Setup installiert fehlende Abhängigkeiten, kompiliert, führt alle Servertests
aus und richtet nur nach erfolgreichen Tests den eigenen Dienst ein.
Bei **neuen Installationen ist Simplex automatisch aktiv**, auf **64739 TCP/UDP**
mit 500 ms für Belegungstimeout und Paketruhe. Bestehende Einstellungen,
Passwörter, Datenbanken und Zertifikate bleiben erhalten.

Verbindungsdaten und zufälliges Zugangspasswort lokal abrufen:

```bash
sudo mumble-funk-info
```

Firewalls und Router werden nicht verändert. Bei einem bereits vorhandenen
Quellverzeichnis die Update-Anleitung verwenden, nicht darüber klonen.
[Alle Schritte, Alternativport, Updates und sichere Entfernung](docs/funk/setup.md).

## Was bereits funktioniert

| Funktion | Verhalten |
| --- | --- |
| Ein Sender pro Kanal | Der erste akzeptierte Sender erhält das exklusive Senderecht. Verschiedene Kanäle funktionieren unabhängig. |
| Gleichzeitige Sendeversuche | UDP und TCP teilen sich dieselbe atomare Vergabe. Es gibt nur einen Besitzer pro Kanal. |
| Abgewiesene Aussendung | Bei fortlaufenden Paketen bleibt sie stumm, auch wenn der Kanal wieder frei wird. Ein neuer Versuch ist erforderlich. |
| Ende und Timeout | Eine gültige Ende-Markierung gibt den Kanal frei. Ohne Marker gilt standardmäßig ein Timeout von 500 ms ohne gültiges neues Paket. |
| Wiederholter Versuch | Ohne Marker gilt im Kern standardmäßig 1.000 ms Paketruhe. Neue Installationen mit `setup.sh` verwenden 500 ms. Die Zeiten sind konfigurierbar. |
| Wechsel und Abbruch | Kanalwechsel, erkannter Verbindungsabbruch und Verlust der Sendeberechtigung geben das Senderecht frei. |
| Diagnose | Optionale PTT-/Sprachpaketdiagnose und eigene Simplex-Diagnose für Vergabe, Ablehnung und Freigabe. |
| Bestehende Clients | Keine Änderungen an Clients oder Mumble-Protokoll erforderlich. |

Es werden keine Sprachinhalte aufgezeichnet. Die Diagnosen speichern Metadaten;
Dateizugriffe erfolgen in Hintergrundthreads. Bei Überlast können Diagnoseeinträge
fehlen, ohne die Vergabe des Senderechts zu verändern. Für die Funklogik werden
Sprachdaten nicht dekodiert und neu kodiert.

**Neue Installationen mit `setup.sh` aktivieren Simplex automatisch.** Der Kern
und der manuelle Installationsweg bleiben standardmäßig ausgeschaltet. Bestehende
Installationen behalten ihre Einstellung; bei ausgeschaltetem Simplex gilt die
normale Mumble-Sprachweiterleitung.

## Ein einfaches Funkbeispiel

1. Anna drückt PTT und beginnt zu senden. Der freie Kanal gehört jetzt ihr.
2. Ben drückt ebenfalls PTT. Seine Pakete werden verworfen.
3. Anna lässt los. Ende-Markierung oder Timeout geben den Kanal frei.
4. Ben hält weiterhin PTT gedrückt und liefert weiter Pakete: Er wird **nicht
   automatisch hörbar**. Er muss seine Aussendung beenden und neu versuchen.

Ohne Ende-Markierung erkennt der Server einen neuen Versuch anhand einer
Paketpause. Für erste Tests: **PTT loslassen, etwa zwei Sekunden warten und neu
drücken.** Eine lange Netzunterbrechung kann allerdings genauso aussehen wie diese
Pause. Der Server kann die physische Taste nicht direkt erkennen.

## Was ist tatsächlich getestet?

| Art der Prüfung | Bisheriger Nachweis |
| --- | --- |
| Automatisierte Tests | Dokumentiert sind 18/18 erfolgreiche Servertestprogramme und 5/5 Installationstests. Nach Wiederaufnahme der Cloud-Umgebung bestanden die drei gezielt wiederholten Simplex-/PTT-Testprogramme erneut. |
| Tatsächliche Paketweiterleitung | Automatisierte Clients prüfen einen echten Serverprozess, unter anderem bei konkurrierenden UDP-/TCP-Paketen, Kanalwechseln, Rechteentzug und Neustart. |
| Erste Gerätetests | Vom Betreiber berichtete Tests mit Android-Mumla und Windows-Mumble bestätigen Verbindung, Sprachübertragung in beide Richtungen und beobachtetes PTT-Verhalten. |
| Noch ausstehend | Umfangreiche Simplex-Gerätetests sowie längere Last-, Mobilfunk- und Stabilitätstests. |

Bei den ersten Mumla-Beobachtungen kamen auch während Sprechpausen weiter Pakete;
bei drei Aussendungen wurde keine bestätigte Ende-Markierung empfangen. Beim
Windows-Client wurde eine Ende-Markierung beobachtet. Diese Ergebnisse gelten
für die beobachteten Versuche und sind keine Zusage für jede Clientversion.
Andere Mumble-Clients sind nicht umfassend praktisch geprüft.

Der Betreiber berichtet inzwischen zusätzlich von erfolgreichen ersten
Simplex-v0.2-Praxistests mit Android-Mumla und Windows-Mumble unter Ubuntu Server
26.04.1. Das ist noch keine umfassende Prüfung aller Geräte oder Netzbedingungen.
Die automatisierten Ergebnisse selbst ersetzen keine Hörprobe mit Endgeräten. Testdaten, Umgebung und Grenzen stehen im
[Prüfbericht](docs/funk/SIMPLEX-STATUS.md). Es wird kein laufender CI-Status behauptet.

## Bekannte Einschränkungen

- Keine direkte Erkennung physischer PTT-Tasten. Fehlende Marker und lange
  Netzunterbrechungen können nicht eindeutig auseinandergehalten werden.
- Keine verlässliche native Besetztanzeige in unveränderten Clients. Die lokale
  Sendeanzeige kann auch bei einer abgewiesenen Aussendung leuchten.
- Schnelle PTT-Wechsel sind noch nicht vollständig optimiert. Ohne Marker können
  kurze Pausen oder zurückgesetzte Sequenznummern zu weiteren Ablehnungen führen.
- Bei aktiviertem Simplex wird nur innerhalb des aktuellen Kanals übertragen.
  Whisper, Kanalverknüpfungen, zusätzliche Kanal-Abonnements und Server-Loopback
  bieten keine alternativen Sprachwege; ihre Clientanzeigen können bestehen bleiben.
- Stable enthält keine Audioeffekte. Dieser Branch ergänzt optionales privates
  Busy-Feedback und experimentelles Squelch Open/Close. Roger Beep und Grundrauschen
  sind nicht implementiert.
- Begrenzte Praxiserfahrung und kein abgeschlossener Langzeit-/Lasttest.

## Installation und Bedienung

Die Installationslösung unterstützt **Ubuntu Server 24.04 LTS und 26.04 LTS**,
einschließlich Punktversionen. Unser eigener Dienst `mumble-funk.service` verwendet
**64739 TCP/UDP** mit eigener Konfiguration, Datenbank und eigenen Logs. Er kann
parallel zum bisherigen Standard-Mumble-Server auf 64738 betrieben werden.

Die folgenden Anleitungen enthalten die gepflegten Befehle. Hier gibt es bewusst
keine zweite Installationsanleitung:

| Aufgabe | Anleitung |
| --- | --- |
| Automatische Einrichtung, Zugangsdaten, Updates und Entfernung | [Setup-Anleitung](docs/funk/setup.md) |
| Voraussetzungen und manueller Installationsweg | [Ubuntu-Installation](docs/funk/installation-ubuntu.md) |
| Aktualisieren, Simplex einschalten, Einstellungen und Rückkehr zur vorherigen Version | [Simplex-Anleitung](docs/funk/simplex.md) |
| Android-/Windows-Test mit optionalem dritten Beobachter | [Praxistestplan](docs/funk/simplex.md#6-praxistest-mit-android-und-windows) |
| Paketzeitpunkte und Ende-Markierungen untersuchen | [PTT-Diagnose](docs/funk/diagnose.md) |

Ein Update erhält die bestehende Einstellung des Funkmodus. Eine neue Installation
über `setup.sh` aktiviert ihn automatisch. Die Clients verbinden sich wie gewohnt
mit dem zusätzlichen Server und brauchen keine Anpassung.

## Roadmap

| Stand | Inhalt |
| --- | --- |
| **Implementiert** | Simplex-Engine v0.2, optionale Diagnosesysteme und automatische parallele Ubuntu-Installation. |
| **Aktuelle Entwicklungsziele** | Umfangreichere Praxistests, schnelle PTT-Wechsel innerhalb der Protokollgrenzen verbessern, Netzwerkstörungen untersuchen sowie Stabilität und Zuverlässigkeit durch längere Tests absichern. |
| **Experimenteller Audiobranch** | Neue Installationen aktivieren Busy und Squelch bei 50 % / +8 dB; bestehende Einstellungen bleiben erhalten. |
| **Unverbindliche Zukunftsideen** | Roger Beep, analoge Funk-Audioeffekte, Squelch, digitale Funkprofile und erweiterte Konfiguration. |

Die Zukunftsideen sind angedacht, noch nicht implementiert und ohne zugesagten
Termin. Ihr späterer Einbau ist nicht garantiert.

## Mitwirken, Herkunft und Lizenz

Fehler und Vorschläge zum Fork gehören in die
[Mumble-Funk-Issues](https://github.com/Shawna7han/mumble-funk/issues).
Für nachvollziehbare Berichte helfen Server-Commit, Clientversionen, Einstellungen
und genaue Testschritte. Passwörter und persönliche Angaben vor dem Teilen von
Diagnoseauszügen entfernen.

Mumble-Funk basiert auf dem ursprünglichen
[Mumble-Projekt](https://github.com/mumble-voip/mumble), ist aber unabhängig.
Es ist keine offizielle Mumble-Veröffentlichung. Eine Unterstützung oder Empfehlung
durch die ursprünglichen Mumble-Entwickler wird nicht behauptet.
Die ursprüngliche Projektwebsite ist [mumble.info](https://www.mumble.info/).

> Copyright (C) 2005-2022 The Mumble Developers
>
> All rights reserved.

Es gelten die Bedingungen der [LICENSE](LICENSE) (BSD 3-Clause).
Ursprüngliche Copyright- und Lizenzhinweise bleiben erhalten. Mitgelieferte
Komponenten behalten ihre eigenen Bedingungen; siehe
[3rdPartyLicenses](3rdPartyLicenses/) sowie die Hinweise im Quellcode und in Submodulen.
