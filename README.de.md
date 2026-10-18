# Mumble-Funk

**Serverseitige Simplex-Funkkommunikation für Mumble**

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
[Installation unter Ubuntu](docs/funk/installation-ubuntu.md) ·
[Simplex aktivieren und testen](docs/funk/simplex.md)

## Was bereits funktioniert

| Funktion | Verhalten |
| --- | --- |
| Ein Sender pro Kanal | Der erste akzeptierte Sender erhält das exklusive Senderecht. Verschiedene Kanäle funktionieren unabhängig. |
| Gleichzeitige Sendeversuche | UDP und TCP teilen sich dieselbe atomare Vergabe. Es gibt nur einen Besitzer pro Kanal. |
| Abgewiesene Aussendung | Bei fortlaufenden Paketen bleibt sie stumm, auch wenn der Kanal wieder frei wird. Ein neuer Versuch ist erforderlich. |
| Ende und Timeout | Eine gültige Ende-Markierung gibt den Kanal frei. Ohne Marker gilt standardmäßig ein Timeout von 500 ms ohne gültiges neues Paket. |
| Wiederholter Versuch | Ohne Marker wird ein neuer Versuch nach standardmäßig 1.000 ms Paketruhe angenommen. Die Zeiten sind konfigurierbar. |
| Wechsel und Abbruch | Kanalwechsel, erkannter Verbindungsabbruch und Verlust der Sendeberechtigung geben das Senderecht frei. |
| Diagnose | Optionale PTT-/Sprachpaketdiagnose und eigene Simplex-Diagnose für Vergabe, Ablehnung und Freigabe. |
| Bestehende Clients | Keine Änderungen an Clients oder Mumble-Protokoll erforderlich. |

Es werden keine Sprachinhalte aufgezeichnet. Die Diagnosen speichern Metadaten;
Dateizugriffe erfolgen in Hintergrundthreads. Bei Überlast können Diagnoseeinträge
fehlen, ohne die Vergabe des Senderechts zu verändern. Für die Funklogik werden
Sprachdaten nicht dekodiert und neu kodiert.

**Simplex ist standardmäßig ausgeschaltet.** Ohne Aktivierung bleibt die normale
Mumble-Sprachweiterleitung verfügbar.

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

Die automatisierten Ergebnisse sind **keine abgeschlossene Hörprobe der
Simplex-Engine mit echten Endgeräten**. Testdaten, Umgebung und Grenzen stehen im
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
- Keine integrierten Funk-Audioeffekte, kein Busy Tone, Roger Beep oder Squelch.
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
| Voraussetzungen und Erstinstallation | [Ubuntu-Installation](docs/funk/installation-ubuntu.md) |
| Aktualisieren, Simplex einschalten, Einstellungen und Rückkehr zur vorherigen Version | [Simplex-Anleitung](docs/funk/simplex.md) |
| Android-/Windows-Test mit optionalem dritten Beobachter | [Praxistestplan](docs/funk/simplex.md#6-praxistest-mit-android-und-windows) |
| Paketzeitpunkte und Ende-Markierungen untersuchen | [PTT-Diagnose](docs/funk/diagnose.md) |

Ein Update allein aktiviert den Funkmodus nicht. Die Aktivierung erfolgt in der
Serverkonfiguration mit anschließendem Neustart des Funkdienstes. Die Clients
verbinden sich wie gewohnt mit dem zusätzlichen Server und brauchen keine Anpassung.

## Roadmap

| Stand | Inhalt |
| --- | --- |
| **Implementiert** | Simplex-Engine v0.2, optionale Diagnosesysteme und parallele Ubuntu-Installation. |
| **Aktuelle Entwicklungsziele** | Umfangreichere Praxistests, schnelle PTT-Wechsel innerhalb der Protokollgrenzen verbessern, Netzwerkstörungen untersuchen sowie Stabilität und Zuverlässigkeit durch längere Tests absichern. |
| **Unverbindliche Zukunftsideen** | Busy Tone, Roger Beep, analoge Funk-Audioeffekte, Squelch, digitale Funkprofile und erweiterte Konfiguration. |

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
