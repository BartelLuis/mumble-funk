# Prüf- und Übergabestand: Simplex v0.2

Stand: 18. Oktober 2026. Arbeitszweig: `funk-v0.1`, Basis Mumble 1.5.939
(offizieller Tag `v1.5.939`, Commit `dfbac8d3d9c967003e74b4ffc5610f7d45e8f666`).
Die Abstammung wurde mit `git merge-base --is-ancestor` geprüft.

Implementierung und Tests sind in **`761208c9f`** gesichert. Der nachfolgende
Dokumentationscommit enthält diesen Code unverändert. Den vollständigen aktuellen
Stand zeigt `git log -1 --oneline`; die beiden Schritte sind getrennt nachvollziehbar.

## Ergebnis und relevante Dateien

- `src/murmur/SimplexController.*`: atomare Senderechte pro Kanal, blockierte
  Aussendungen, Marker/Timeout, konservative Wiederfreigabe und Lebenszyklus.
- Die zurückgegebene Sperre bleibt bis zum Ende von `Server::processMsg` bestehen;
  UDP und TCP können die Entscheidung und Weiterleitung nicht überholen.
- Alle Zustände enthalten Werte, keine User-/Channel-Zeiger; Verbindungskennungen
  sind innerhalb eines Serverlaufs eindeutig. Disconnect löscht den Zustand.
- `SimplexLogger.*`: eigene begrenzte asynchrone JSONL-Ausgabe, kein Sprachinhalt.
  Diagnoseverlust beeinflusst die Senderechte nicht.
- Integration in `Server`, `Meta`-Konfiguration, `Messages`, `RPC` und ACL-Änderungen.
- Im Funkmodus nur aktuelle Kanalmitglieder, kein Whisper/Loopback/Link/Listener.
- Standardwerte: `simplex=false`, `simplextimeout=500`,
  `simplexretrytimeout=1000`; Diagnose unabhängig von der PTT-Diagnose.
- Keine Änderungen an Audio-Codecs, Clientcode oder Protokolldefinitionen.
  Der vorhandene PTT-Diagnosecode und seine Einstellungen wurden erhalten.
- Installationsskripte und systemd-Datei bleiben unverändert. Die neue Funktion
  benötigt keine weiteren Pakete. Nur INI-Vorlagen enthalten zusätzliche Kommentare.
- [Bedienungsanleitung mit Update, Rückkehr und Praxistest](simplex.md).
- [Technische Synchronisierung](../dev/MurmurLocking.md).

## Tatsächlich ausgeführte Tests

Die aktuelle Prüfung erfolgte in der **Debian-13-Codex-Umgebung (amd64)** mit
Qt 5.15.15, GCC 14.2, C++14, Debug-Build und deaktiviertem Unity-Build.
Es wurde inkrementell gebaut; Warnungen werden als Fehler behandelt.

1. Server und sämtliche Testprogramme erfolgreich kompiliert.
2. **18/18 CTest-Programme erfolgreich**, einschließlich aller bisherigen 16.
   Der abschließende Gesamtlauf dauerte rund 30 Sekunden; kein Test übersprungen.
3. **5/5 Installationstests erfolgreich**, kein Test übersprungen. Dabei zwei
   lokale Testserver auf 64738/64739 mit getrennten Datenbanken: Simplex nur auf
   64739; Startmeldung und eigene Diagnosedatei geprüft. Wiederholte Installation
   erhält eine bereits angepasste Konfiguration einschließlich `simplex=true`.
4. Shell-Syntaxprüfung der bestehenden Build-/Installations-/Plattformskripte und
   `git diff --check` erfolgreich.

### Absicherung nach Wiederaufnahme am 18. Oktober 2026

Die bestehende Cloud-Umgebung, der kompilierte Server und alle Testprotokolle sind
erhalten. `git diff` bestätigt, dass der Implementierungs- und Testcode seit
`761208c9f` unverändert ist; ungesichert war ausschließlich die Dokumentation.
Die Protokolle des Gesamtlaufs vom 17. Oktober bestätigen weiterhin 18/18
Servertests und 5/5 Installationstests.

Zur Absicherung der wieder gestarteten Umgebung wurden gezielt
`TestVoiceDiagnostics`, `TestSimplex` und `TestSimplexIntegration` erneut ausgeführt:
**3/3 erfolgreich**, kein Test übersprungen, rund 26 Sekunden. Ein Neubau und die
Wiederholung der übrigen unveränderten Tests waren nicht erforderlich.
Der Dokumentationsabschluss verändert weder Programm noch Tests.

`TestSimplex` enthält elf Testfunktionen (13 Qt-Testeinträge einschließlich
Initialisierung/Aufräumen). `TestSimplexIntegration` führt zehn datengestützte
Testfälle aus (12 Qt-Testeinträge einschließlich Initialisierung/Aufräumen).
Die CTest-Zahl 18 zählt Programme, nicht einzelne Assertions oder Qt-Testfälle.

| Anforderung | Ausgeführter Nachweis |
| --- | --- |
| Erster Sender, zweiter abgewiesen | Kernlogik und tatsächlich empfangene Pakete |
| Nahezu gleichzeitiger Beginn | 100 Zwei-Thread-Vergaben; zusätzlich acht UDP/TCP-Runden je Integrationsvariante |
| Keine Überholung der letzten Weiterleitung | Zweiter Thread wartet auf die gehaltene Transaktionssperre |
| Stille bei weiter eintreffenden Paketen | 30 Sekunden synthetischer Zeitverlauf mit 20-ms-Paketen; keine Freigabe |
| Ende-Markierung | Kernlogik, Legacy- und modernes Protobuf-Paketformat |
| Fehlende Markierung und Timeout | Künstliche Uhr plus echter 500-ms-Timeout im Serverprozess |
| Abgewiesene laufende Aussendung bleibt stumm | Kernlogik und ausbleibende Weiterleitung trotz freiem Kanal |
| Kanalwechsel | Alter Kanal frei, laufender Versuch im neuen Kanal gesperrt |
| Verbindungsabbruch | Kernlogik und tatsächliches Schließen der TLS-Verbindung |
| Unabhängige Kanäle | Mehrere Sender/Empfänger in zwei echten Serverkanälen |
| Gleichzeitige UDP/TCP-Verarbeitung | Verschlüsselte UDP-Pakete und TLS-Tunnel, abwechselnde Ankunftsreihenfolge |
| Verspätete Marker, Duplikate, alte Timeouts | Kernlogik und verspätete Marker im Integrationstest |
| Whisper, Links, Listener, Loopback | Tatsächlicher Empfang im Standardmodus; Ausbleiben im Funkmodus |
| Simplex aus | Fehlender Parameter, explizites `false`, Legacy- und modernes Format; beide Sender werden weitergeleitet |
| Bestehende PTT-Diagnose | UDP-/TCP-Pakete einschließlich abgewiesener Versuche weiterhin erfasst |
| Neustart während Belegung | Echter Serverneustart mit derselben Datenbank; neuer Sender zugelassen |
| Schnelle Wiederholung ohne Marker | Unterhalb der Ruhezeit gesperrt; Sequenzreset erst nach Paketruhe zugelassen |
| Rechteverlust | Admin-Mute, Selbststummschaltung, ACL-Entzug; Freigabe, Wiedererteilung ohne automatische Übernahme |
| Diagnosefehler und Filter | Dateifehler beeinflusst Eigentümer nicht; Filter, Rotation und Größenbegrenzung |
| Diagnose ausgeschaltet | Simplex arbeitet weiterhin, keine eigene Logdatei |
| Audio unverändert | Empfänger bekommt identische undurchsichtig behandelte Nutzdaten; kein Codec-Aufruf |

Die Integrationsclients nutzen TLS und Mumbles bestehende UDP-Verschlüsselung.
Sie führen den üblichen UDP-Ping vor der Sprachübertragung aus. Ein leerer
Legacy-Endmarker wird geprüft; moderne Protobuf-Endmarker enthalten Nutzdaten,
weil der unveränderte Mumble-Decoder leere moderne Audiopakete ablehnt.
Empfänger im Integrationstest prüfen die über TCP zugestellten Pakete.

Wiederholung auf Ubuntu nach dem Build:

```bash
ctest --test-dir build/funk --output-on-failure
python3 tests/funk-install/test_install.py build/funk/mumble-server
```

Die getrennten Installationstests stoppen keine bestehenden Dienste. Sind
64738/64739 belegt, wird ausschließlich ihr Parallelserver-Test übersprungen.
Die 18 CTest-Programme verwenden freie lokale Ports.

## Was nicht behauptet wird

- Keine Installation oder Tests auf der realen Proxmox-VM durchgeführt.
- Kein erneuter Vollbau in beiden Ubuntu-Containern für diese Änderung. Die zuvor
  erfolgreich geprüfte Ubuntu-24.04/26.04-Installation wird weiterverwendet;
  Paketliste, Skripte und Dienstdatei benötigen keine Änderung.
- Keine reale neue Hörprobe mit Android/Windows durchgeführt. Die Protokolltests
  ersetzen nicht den [Gerätetest](simplex.md#6-praxistest-mit-android-und-windows).
- Kein Langzeit-/Lasttest, kein Sanitizer-Nachweis und kein formaler Beweis völliger
  Race-/Speicherfehlerfreiheit. Sperrreihenfolge, Zustandslebensdauer und begrenzte
  Diagnosewarteschlange sind dokumentiert und gezielt getestet.
- Der RPC-Hook wurde kompiliert und geprüft, aber nicht mit einem externen
  Ice-/DBus-Administrationsclient im Integrationstest aufgerufen.

## Bekannte Grenzen und weitere Arbeiten

Es gibt nach den ausgeführten Tests keinen bekannten offenen Implementierungsfehler.
Der gewünschte v0.2-Umfang ist umgesetzt. Vor dem dauerhaften Einsatz bleiben
Update und Praxistest durch den Betreiber nötig. Ein Last-/Mobilfunk-Dauertest
ist eine sinnvolle nächste Aufgabe.

Ohne PTT-Nachricht kann eine ausreichend lange Netzunterbrechung wie Loslassen und
erneutes Drücken aussehen. Fortlaufende Pakete eines blockierten Senders bleiben
gesperrt; eine Paketpause ab `simplexretrytimeout` erlaubt einen neuen Versuch.
Kurze PTT-Pausen ohne Marker können deshalb ebenfalls gesperrt bleiben. Zurückgesetzte
Sequenznummern werden erst nach dieser Paketpause neu akzeptiert. Ein echter
physischer Tastendruck und beliebig verspätete Pakete nach einer solchen Pause
sind ohne Protokolländerung nicht zweifelsfrei zu unterscheiden.

Es gibt keine native Besetztanzeige im Client. Funkmodus bedeutet bewusst nur
Sprache im aktuellen Kanal. Die Diagnose kann bei Überlast Zeilen verlieren;
die Durchsetzung der Senderechte verwendet ausschließlich den Controllerzustand.
