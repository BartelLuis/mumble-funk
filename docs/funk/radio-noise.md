# Experimentelles kontinuierliches Funkrauschen — B2b

Diese Vorschau ergänzt den privaten AudioFeedback-Weg. Der Simplex-Controller und
Sprachpakete bleiben unverändert. Eine Freigabe setzt Hörtests mit Windows-Mumble
und Android-Mumla voraus; automatisierte Paket-/Opus-Tests ersetzen diese nicht.

**Betreiber-Hörtest erfolgreich:** Der Stand
`6f976a22f6511f372ff85ca2403592e5faa8e896` wurde auf der Ubuntu-Test-VM praktisch
geprüft. Kontinuierliches Rauschen, der gemeinsame Open-/Noise-/Close-Stream und
Close beim Senden vom Windows-Client funktionieren; die zuvor gehörten kurzen
Audioaussetzer sind behoben. Dies bestätigt den bisherigen Testumfang und ist
**keine allgemeine Stable-Freigabe** oder abgeschlossener Langzeit-/Lasttest.

Die [gemeinsame Befehlsübersicht](ubuntu-commands.md) erklärt Serverinformationen
mit/ohne Passwort, Audio-Status, Dienststatus, installierten Commit und Serverlog.

## Konfiguration

**Neuinstallationen dieses Feature-Branches** erhalten in Ubuntu und Docker:

```ini
radionoise=true
radionoisefile=/usr/local/share/mumble-funk/audio/radio-noise.wav
radionoisevolume=20
```

Nur bei aktiviertem Simplex und `audiofeedback=true` kann eine ausdrücklich
aktivierte Rauschfunktion arbeiten. Die Standardaufnahme wird automatisch installiert.
Sie stammt vom Projektbetreiber und ist ausdrücklich unter CC0 1.0 Universal
freigegeben; siehe [Herkunft, Lizenz und SHA-256](../../assets/audio/LICENSE.md).
Die Originaldatei wird bytegenau übernommen, ohne Umkodierung oder neue Stille.
Für eine eigene lokale Testkonfiguration kann ein anderer absoluter WAV-Pfad gewählt werden.
Der Server muss die Datei lesen können. Das wird beim Start geladen; Änderungen
benötigen einen Neustart der eigenen Testinstanz.

Akzeptiert wird RIFF PCM-WAV, Mono, 48 kHz, signed 16 Bit Little Endian, mit
20 ms bis 60 Sekunden Audiodaten und höchstens 8 MiB Dateigröße. Die Aufnahme wird
nicht verändert. Leerer Pfad, Volume 0 oder eine ungültige Datei deaktivieren nur
das optionale Rauschen. Fehler werden im Serverlog gemeldet.

Die Lautstärke ist unabhängig, erlaubt 0..100 und verwendet keinen globalen
`feedbackboostdb`. Volume 0 erzeugt keine Rauschpakete oder Rausch-Worker.
Bestehende INI-Dateien und Audio-Standardeinstellungen werden nicht migriert.
Fehlende Rauschschlüssel in alten INIs bedeuten weiterhin `false`, leere Dateiauswahl
und Lautstärke `20`. Ein Update schaltet Rauschen somit nicht unbemerkt ein.
Eine eigene Datei am neuen Standardpfad wird nicht überschrieben: Der native
Installer bricht bei abweichendem Inhalt oder einem symbolischen Link vor der
Installation ab. Eigene Dateien zuerst sichern und bewusst über die Verwaltung
importieren, bevor der Paketpfad freigegeben wird.

## Zustellung und Grenzen

Pro angenommener Übertragung wird ein gemeinsamer ursprünglicher Zufallsstart auf
einem 20-ms-Raster gewählt. Separate Encoder erlauben unabhängige Unterbrechungen
je Empfänger. Am WAV-Ende folgt samplegenau Sample 0, ohne Crossfade, zusätzliche
Nullen oder Encoderreset. Nach Busy wird die bisherige PCM-Position fortgesetzt.

Mit aktiviertem Rauschen bilden **Squelch Open → Rauschen → Squelch Close einen
zusammenhängenden privaten Effektstream pro Empfänger**. Ein einziger Opus-Encoder
bleibt über alle drei Phasen aktiv; Sequenzen steigen durchgehend um 2 pro
20-ms-Paket. Erst das letzte Close-Paket trägt eine Ende-Markierung. Zwischen den
Phasen gibt es keine künstliche Stille oder 60-ms-Pause. Open und Noise können
innerhalb eines Frames unmittelbar aufeinander folgen; es wird nichts gemischt.
Alle WAV-Dateien und die getrennte normale Sprachweiterleitung bleiben unverändert.

Busy behält höchste Priorität, seinen bisherigen Clip und Cooldown. Ein Reserve-
paket beendet den unterbrochenen Effekt; Busy bleibt ein unabhängiger Stream.
Danach kann Noise bei weiter laufender Übertragung mit frischem Encodersegment
an der nächsten PCM-Position fortsetzen, ohne Open zu wiederholen. Kurze PTT-
Betätigungen können Open bereits am nächsten Frame in Close überführen.

Bestätigtes Ende und Simplex-Timeout stoppen Rauschen; Timeout-Close folgt nur
bei aktiviertem bestehendem Fallback. Ein Timeout beweist kein physisches
Loslassen von PTT. Deaktiviertes Open/Close wird ausgelassen; ohne Close wird der
Stream regulär beendet. Ohne Rauschen bleibt die bisherige Squelch-Zustellung
bestehen. Rauschen funktioniert auch bei bewusst ausgeschaltetem Squelch.

Maximal 16 vorbereitete Produzenten, je drei gepufferte Pakete plus ein bereits
codiertes ungesendetes Reservepaket pro aktivem Empfänger, verwenden gemeinsame
unveränderliche PCM-Puffer. Erzeugung und Thread-Joins liegen beim Start/Stop.
Zugriffe auf Produzenten während der Zustellung warten nicht auf deren Mutex.
Die bisherigen 1024-Byte-/64-KiB-TCP-Grenzen und die 300-ms-Close-Frist bleiben
bestehen. Kurzzeitiger TCP-Stau invalidiert nicht mehr die Übertragung; bei
anhaltendem Stau oder Produzentenfehler darf optionaler Effekt entfallen.
Eine Ende-Markierung kann bei dauerhaft unbeschreibbarem Transport nicht
zugesichert werden. Es wird niemals eine unbegrenzte Warteschlange aufgebaut.

Empfänger werden am Übertragungsbeginn erfasst. Neue Empfänger erhalten Effekte
ab der nächsten Übertragung. Identität, Kanal und Generation werden erneut
geprüft; Kanalwechsel, Disconnect und Berechtigungsverlust verwerfen alte Close-
Anforderungen. Session-Reuse übernimmt keine alten Audiodaten. Bereits beim
Client gepufferte Pakete können nicht zurückgerufen werden. Der 60-ms-Abstand
zwischen unabhängigen Streams, etwa bei Busy, ist weiterhin keine Garantie für
einen Decoderreset auf jedem Client. Details und Grenzen:
[Squelch-Close-Analyse](SQUELCH-CLOSE-ANALYSIS.md).

## Zeitplanung während laufender Übertragung

Der Effektstream verwendet ein fortlaufendes 20-ms-Medienraster. Der Timer wird
an der nächsten Fälligkeit ausgerichtet; kurze Lock-/Producer-Konflikte werden
nicht blockierend nach 5 ms erneut versucht. Pro Empfänger wird höchstens ein
Frame je Callback zugestellt; Puffer-, Stall- und Close-Fristen bleiben bestehen.
Damit führen leicht schwankende Timeraufrufe nicht mehr zu regelmäßigen 40-ms-
Lücken. Ohne Rauschen bleibt die bisherige Timerplanung erhalten.

Ursache, vorher/nachher-Messung, UDP-Lasttests und verbleibender Hörtest:
[Analyse der Audioaussetzer](AUDIO-TIMING-ANALYSIS.md).

## Lokale Prüfungen

Der normale Server-Build registriert `TestRadioNoise`, `TestNoiseScheduler` und
`TestNoiseIntegration` zusätzlich zu den bisherigen Regressionstests.
`TestRadioNoise` prüft kontinuierliche Kodierung gegen eine unabhängige Referenz.
`TestNoiseIntegration` verwendet temporäre Server mit Loopback-Adressen und
prüft TCP, verschlüsseltes UDP, unveränderte Sprache und den Übertragungslebenszyklus.

Die mitgelieferte CC0-Aufnahme kann zusätzlich mit dem echten Parser geprüft werden:

```bash
build/funk/mumble-server --validate-radio-noise-wav assets/audio/radio-noise.wav
MUMBLE_FUNK_NOISE_WAV="$PWD/assets/audio/radio-noise.wav" \
  ctest --test-dir build/funk --output-on-failure
```

Die Installationstests prüfen die vollständige SHA-256, Dateirechte, frische
Standardkonfiguration und den Bestandsschutz. Die Docker-Integration prüft sowohl
die mitgelieferte Aufnahme als auch eine synthetische eigene WAV einschließlich
Backup/Restore und Update/Rollback. Native Verwaltung und
[Docker-Rauschverwaltung](docker.md#17-eigenes-funkrauschen-einrichten) verwenden
weiterhin dieselbe Audio-Engine.

## B2c-1: Ubuntu-Snapshot-VM für den ersten Hörtest

Diese Anleitung gilt ausdrücklich für **eine separate Ubuntu-24.04-LTS-Test-VM**
mit systemd, einem normalen Benutzer mit sudo-Rechten, Internetzugang, empfohlenen
4 GiB RAM / 2 CPU-Kernen und mindestens 10 GiB freiem Speicher. Den sauberen
Proxmox-Snapshot erstellt bzw. restauriert der Betreiber selbst. Andere VMs und
Produktivinstallationen bleiben unberührt. Die folgenden Befehle führt der
Betreiber erst beim gewünschten Test aus, jeweils der Reihe nach; bei Fehlern
anhalten. Das Setup installiert bzw. aktualisiert den **normalen** Dienst
`mumble-funk`, keine parallele Vorschau.

| Zweck | Wert |
| --- | --- |
| Experimenteller Branch / Auswahl | `feature/radio-noise` / `--experimental-radio-noise` |
| Ubuntu-Port | **64739 TCP und UDP** |
| Programm | `/opt/mumble-funk/current/mumble-server` |
| INI | `/etc/mumble-funk/server.ini` |
| Datenbank / Zertifikate | `/var/lib/mumble-funk/` |
| Mitgelieferte CC0-Aufnahme | `/usr/local/share/mumble-funk/audio/radio-noise.wav` |
| Eigene WAVs | `/etc/mumble-funk/sounds/radio-noise-<SHA256>.wav` |

Docker bleibt außerhalb dieser Testinstallation: Host **64740**, Container
**64739**, jeweils TCP/UDP, unverändert. Keine weiteren Ports und kein Port 64742.

### A. Frische Ubuntu-VM

Per SSH auf der eigenen Test-VM als normaler Benutzer anmelden:

```bash
sudo apt update
sudo apt install --yes git ca-certificates openssh-client nano
mkdir -p "$HOME/src"
cd "$HOME/src"
git clone --branch feature/radio-noise --single-branch --recurse-submodules \
  https://github.com/Shawna7han/mumble-funk.git mumble-funk
cd "$HOME/src/mumble-funk"
bash scripts/funk/setup.sh --experimental-radio-noise
sudo systemctl --no-pager --full status mumble-funk
cat /opt/mumble-funk/current/BUILD-INFO
```

Nicht in einen bereits vorhandenen Ordner klonen; dann Abschnitt B verwenden.
Das Setup installiert die übrigen Build-Abhängigkeiten, kompiliert, testet und
installiert erst nach Erfolg. `BUILD-INFO` muss `branch=feature/radio-noise` und
`experimental=true` nennen. Keine Stable-Freigabe. Falls Ubuntu-Pakete nicht
gefunden werden, die `universe`-Hinweise in [installation-ubuntu.md](installation-ubuntu.md)
beachten. **Neue INIs aktivieren Rauschen mit der mitgelieferten Aufnahme bei Lautstärke 20.**
Vorhandene INIs werden nicht geändert.
Die übrigen aktuellen Audio-Defaults für Neuinstallationen bleiben erhalten:
Simplex, AudioFeedback, Busy, Open, Close und Timeout-Close aktiv, Effektlautstärken
50 %, Audio-Boost +8 dB. Der Boost gilt nicht für Rauschen.

`setup.sh`, `build.sh` und `install.sh` verlangen ausdrücklich denselben neuen
Schalter. Mumble-1.5.939-Basis, offizielle GitHub-Herkunft, unveränderte versionierte
Dateien sowie exakte Übereinstimmung mit lokalem `origin/feature/radio-noise`
**und dem live veröffentlichten Feature-Commit** werden geprüft. Für diesen Branch
werden auch nicht ignorierte unversionierte Dateien abgewiesen. Ignorierte
Build-Ausgaben bleiben erlaubt. Ein anderer Opt-in-Schalter erlaubt diesen Branch
nicht. `--experimental-dev`, Stable und die älteren Feature-Schalter bleiben gültig
für ihre jeweiligen Quellen. Bei GitHub-/Netzwerkfehlern wird abgebrochen.

### B. Vorhandene Ubuntu-Testinstallation aktualisieren

Nur auf der vorgesehenen Test-VM verwenden. Vorher Snapshot oder externe Sicherung
selbst anlegen. Das Setup erhält vorhandene INI-Einträge einschließlich fehlender
oder deaktivierter Audiooptionen, Datenbanken, Passwörter, Zertifikate und eigene
WAVs. Es aktiviert vorhandene Effekte nicht nachträglich.

Im **vorhandenen** Quellverzeichnis (Pfad bei Bedarf anpassen):

```bash
cd "$HOME/src/mumble-funk"
git status --short --branch
```

Nur ohne eigene Änderungen fortfahren. Eigene Änderungen separat sichern; kein
`reset --hard`, kein `clean`. Die private WAV gehört nicht in dieses Verzeichnis.
Eine zusätzliche lokale INI-Sicherung außerhalb des Repositorys erstellen:

```bash
backup_dir="/var/backups/mumble-funk/manual-$(date -u +%Y%m%dT%H%M%S)-$$"
sudo install -d -m 0700 "$backup_dir"
sudo cp -a /etc/mumble-funk "$backup_dir/config"
```

Den veröffentlichten Feature-Stand explizit abrufen; das funktioniert auch bei
älteren Single-Branch-Klonen:

```bash
git fetch origin refs/heads/feature/radio-noise:refs/remotes/origin/feature/radio-noise
if git show-ref --verify --quiet refs/heads/feature/radio-noise; then
  git switch --no-guess feature/radio-noise
else
  git switch --no-track -c feature/radio-noise refs/remotes/origin/feature/radio-noise
fi
git merge --ff-only origin/feature/radio-noise
bash scripts/funk/setup.sh --experimental-radio-noise
sudo systemctl --no-pager --full status mumble-funk
cat /opt/mumble-funk/current/BUILD-INFO
```

Der erste Wechsel legt den Branch bei Bedarf ohne automatische Upstream-Zuordnung
an; der explizite Fetch funktioniert weiterhin auch ohne diese Zuordnung.
Ein bereits existierender, abweichender Branch darf nicht zurückgesetzt werden.
Während des Builds läuft der bisherige Dienst weiter. Erst bei der Installation
wird ausschließlich `mumble-funk` kurz gestoppt; Konfiguration und vollständiger
Datenbankordner werden dabei konsistent unter `/var/backups/mumble-funk/` gesichert.
Diese lokalen Sicherungen enthalten Geheimnisse; privat aufbewahren. Sie ersetzen
keinen extern aufbewahrten Snapshot bzw. keine externe Sicherung. Rückkehr zum bisherigen Programm:
[Setup-Update/Rollback](setup.md#updates).

### C. Standardaufnahme und Status prüfen

Bei einer frischen Installation ist keine Dateiübertragung und keine manuelle
INI-Bearbeitung nötig. Prüfen:

```bash
sudo mumble-funk-audio status
sudo -u mumble-funk test -r /usr/local/share/mumble-funk/audio/radio-noise.wav
sha256sum /usr/local/share/mumble-funk/audio/radio-noise.wav
sudo tail -n 60 /var/log/mumble-funk/server.log
```

Erwartet: `radionoise=true`, der mitgelieferte Pfad, Lautstärke `20` und SHA-256
`dc0a1c97115eb797f261af52631288b868a648d0d9310fff0a431e8163536312`.
Im aktuellen Startabschnitt steht `Experimental radio noise enabled:`.
Die Standarddatei hat Modus 0644 und ihr Verzeichnis 0755; der Server kann sie lesen.
`status` zeigt ausschließlich gespeicherte Audioeinstellungen, keine Passwörter
und keinen Nachweis einer erfolgreich geladenen Datei oder hörbarer Wiedergabe.
Die Statusabfrage startet keinen Dienst neu. Weitere Befehle:
[Ubuntu-Befehlsübersicht](ubuntu-commands.md).

### D. Eigene Aufnahme und Rückkehr zum Standard

Eine eigene WAV mit geklärten Rechten vom Computer ins eigene Home-Verzeichnis
übertragen, z. B. mit SFTP nach `/home/BENUTZER/radio-noise.wav`. Dann auf der VM:

```bash
sudo mumble-funk-audio set-radio-noise "$HOME/radio-noise.wav"
sudo mumble-funk-audio radio-noise-volume 20
sudo mumble-funk-audio enable-radio-noise
sudo mumble-funk-audio status
```

Die Verwaltung prüft mit dem Servervalidator und kopiert die Datei bytegleich nach
`/etc/mumble-funk/sounds/radio-noise-<SHA256>.wav`, Modus 0640, lesbar für die
Dienstgruppe. Import verändert nur die Dateiauswahl, nicht den Ein/Aus-Schalter.
Alte eigene Dateien bleiben für eine Rückkehr erhalten. Keine Neukompilierung nötig.
Zum Wechsel auf die mitgelieferte Aufnahme:

```bash
sudo mumble-funk-audio use-standard-radio-noise
sudo mumble-funk-audio enable-radio-noise
```

Der erste Befehl ändert ausschließlich den Pfad nach erfolgreicher Validierung,
behält Lautstärke und Aktivierungszustand bei und löscht keine eigenen WAVs.
`enable-radio-noise` setzt bereits aktiviertes Simplex und Audio-Feedback voraus;
es schaltet diese nicht selbst ein. Volume 0 bleibt stumm. Die Rauschlautstärke ist
unabhängig vom Boost für Busy/Open/Close.

### E. Ausschalten und Bestandsschutz

```bash
sudo mumble-funk-audio disable-radio-noise
sudo mumble-funk-audio status
```

Pfad, Lautstärke und eigene Dateien bleiben erhalten. Ändernde Befehle starten nur
`mumble-funk` kurz neu; Verbindungen können dabei getrennt werden. Vorher sichern
(siehe Abschnitt B). Fehlerhafte Importe lassen die INI unverändert; bei einem
fehlgeschlagenen Dienstneustart versucht die Verwaltung, die vorige INI und den
vorigen Betrieb wiederherzustellen. Bei weiterhin auftretenden Fehlern Log prüfen.
Keine parallelen Verwaltungsbefehle ausführen.

Updates ergänzen auch fehlende INI-Einträge nicht. Ausgeschaltetes Rauschen und
individuelle WAVs bleiben damit erhalten. Eine bestehende Installation übernimmt
die neuen Vorgaben nur auf ausdrücklichen Wunsch über die obigen Befehle.

### F. Verbindung und Hörtest

Adresse und Port ohne Passwort bzw. ausdrücklich lokal mit Passwort anzeigen:

```bash
sudo mumble-funk-info --hide-password
sudo mumble-funk-info
```

Passwort nur privat am Terminal ansehen; keine Ausgabe in Testberichte, Screenshots
oder öffentliche Logs kopieren. Clients verbinden sich zur IP der Test-VM auf
**64739** mit eigenem Benutzernamen und gegebenenfalls dem Zugangspasswort. In einer
frischen VM erstmals angebotene Serverzertifikate prüfen. Falls eine Firewall
aktiv ist, TCP **und** UDP 64739 zwischen Clients und VM erlauben; das Setup ändert
weder Ubuntu-/Proxmox-Firewall noch Router. Nicht allein hierfür eine unbekannte
Firewallkonfiguration aktivieren.

Zwei normale, unveränderte Clients in denselben Kanal bringen; Kopfhörer helfen
gegen Rückkopplung. Nacheinander prüfen:

| Test | Erwartung / Beobachtung |
| --- | --- |
| Windows-Mumble sendet | Open, Rauschen und verständliche vollständige Sprache; nach PTT-Ende Rauschen aus, Close. |
| Mumla sendet | Open, Rauschen und Sprache; ohne Ende-Markierung stoppt Rauschen erst beim Simplex-Timeout, dann Close bei aktiviertem Fallback. Ein Timeout beweist kein PTT-Loslassen. |
| 60–90 Sekunden senden | Kontinuierliche Sprachpakete senden (sprechen, nicht nur still PTT halten); keine hörbaren Loop-Klicks oder periodischen Aussetzer. CPU parallel beobachten. |
| Mehrfach PTT | Unterschiedliche Abschnitte durch neue Zufallsstarts; keine alten Effekte nach dem Wechsel. |
| Zweiter Benutzer sendet während Belegung | Sein Senden bleibt blockiert, Busy funktioniert; kein zusätzliches Senderecht. |
| Kanalwechsel / Disconnect | Zugehöriges Rauschen endet; keine Effekte im falschen Kanal oder für eine neue Session. Bereits gepufferte Pakete können kurz nachlaufen. |
| Lautstärke 0, 10, 20, 30, 50 | Nur `radionoisevolume` ändern und Dienst jeweils neu starten; 0 stumm, Sprache stets verständlich, übrige Effekte unverändert. |

CPU und neue Servermeldungen auf der VM beobachten:

```bash
ps -C mumble-server -o pid,pcpu,pmem,etime,args
sudo tail -f /var/log/mumble-funk/server.log
```

Strg+C beendet nur die Loganzeige. Notieren: getestete Git-Revision aus BUILD-INFO,
Clientversionen, Sender/Empfänger-Rollen, Lautstärke, Transport und hörbare Fehler
(z. B. erste/letzte Silbe, verzögerter Close). Kein Passwort und keine privaten
Aufnahmen mitsenden. Bei zwei Geräten ist der blockierte Sender nicht zugleich
Empfänger der angenommenen Übertragung; Busy-Unterbrechung eines bereits laufenden
privaten Empfänger-Effektstreams wird zusätzlich automatisiert geprüft.

Die ursprüngliche B2c-1-Anleitung war allein kein Nachweis erfolgreicher
Geräte-Hörtests. Den inzwischen gemeldeten Ubuntu-Betreiber-Hörtest beschreibt
der Anfang dieses Dokuments; Docker-Hörtests bleiben separat erforderlich.
Die mitgelieferte CC0-Standardaufnahme ist jetzt auf diesem Feature-Branch
enthalten. Persönliche Zusatzaufnahmen werden durch die
[Docker-Rauschverwaltung](docker.md#17-eigenes-funkrauschen-einrichten) nur in
das eigene persistente Volume importiert und nicht veröffentlicht.

## Korrekturen: Close nach kurzer und langer Rauschübertragung

Der in `dc74ff8` behobene Sequenzrücksprung nach 60 Sekunden bleibt verhindert:
Aktive Effektstreams verlieren ihre Sequenzhistorie nicht durch Idle-Bereinigung.
Zusätzlich meldete der Betreiber fehlenden Close auf Mumla bereits bei 2–10
Sekunden langen Windows-Übertragungen. Sein A/B-Hörtest mit 200 ms diagnostisch
vorangestellter Stille stützt ein Problem beim Wechsel der Empfangspuffer.

Die aktuelle Korrektur vermeidet diesen Wechsel innerhalb der Übertragung:
Open, Rauschen und Close haben einen gemeinsamen Encoder- und Sequenzzustand
und genau eine abschließende Ende-Markierung. Keine WAV wurde dafür verändert.
Eine pauschale zusätzliche Pause oder Verlängerung der 300-ms-Frist wird nicht
verwendet. Auch die zuvor nachgewiesenen Abbrüche ohne Noise-Ende bei TCP-Stau,
Produzentenknappheit und verzögertem Scheduler haben gezielte Regressionstests.
Siehe [Analyse und Tests](SQUELCH-CLOSE-ANALYSIS.md).

**Hörtest für künftige Änderungen:** Der oben genannte Betreiber-Test ist
für den bisherigen Umfang erfolgreich. Bei weiteren Änderungen auf der separaten Snapshot-Test-VM mit
der ursprünglichen Close-WAV testen, nicht mit einer um Stille verlängerten Kopie.
Windows → Mumla für 2, 10 und mindestens 62 Sekunden, Noise an/aus; anschließend
Mumla → Windows mit Timeout-Fallback, sehr kurzes PTT während Open, schnelle
Wechsel und Busy prüfen. Erste/letzte Sprachsilben vollständig, Close genau einmal,
keine hängenbleibenden Effekte. Automatisierte Server-, Opus- und Puffermodelltests
bestätigen allein keine tatsächliche Wiedergabe auf dem Android-Gerät. Auch der
erfolgreiche Betreiber-Hörtest ist keine allgemeine Freigabe; die Version bleibt experimentell.
