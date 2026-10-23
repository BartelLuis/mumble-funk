# Experimentelles kontinuierliches Funkrauschen — B2b

Diese Vorschau ergänzt den privaten AudioFeedback-Weg. Der Simplex-Controller und
Sprachpakete bleiben unverändert. Eine Freigabe setzt Hörtests mit Windows-Mumble
und Android-Mumla voraus; automatisierte Paket-/Opus-Tests ersetzen diese nicht.

## Konfiguration

Neue Serveroptionen (Standardwerte):

```ini
radionoise=false
radionoisefile=
radionoisevolume=20
```

Nur bei aktiviertem Simplex und `audiofeedback=true` kann eine ausdrücklich
aktivierte Rauschfunktion arbeiten. Für eine eigene lokale Testkonfiguration
`radionoise=true` und einen absoluten Pfad zu einer eigenen WAV-Datei setzen.
Der Server muss die Datei lesen können. Das wird beim Start geladen; Änderungen
benötigen einen Neustart der eigenen Testinstanz.

Akzeptiert wird RIFF PCM-WAV, Mono, 48 kHz, signed 16 Bit Little Endian, mit
20 ms bis 60 Sekunden Audiodaten und höchstens 8 MiB Dateigröße. Die Aufnahme wird
nicht verändert. Leerer Pfad, Volume 0 oder eine ungültige Datei deaktivieren nur
das optionale Rauschen. Fehler werden im Serverlog gemeldet.

Die Lautstärke ist unabhängig, erlaubt 0..100 und verwendet keinen globalen
`feedbackboostdb`. Volume 0 erzeugt keine Rauschpakete oder Rausch-Worker.
Bestehende INI-Dateien und Audio-Standardeinstellungen werden nicht migriert.

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

Optional kann die private Testaufnahme außerhalb des Repositorys verwendet werden:

```bash
MUMBLE_FUNK_NOISE_WAV=/absoluter/pfad/static.wav \
  ctest --test-dir build/funk --output-on-failure

build/funk/tests/TestRadioNoise /absoluter/pfad/static.wav
```

Der zusätzliche Dateitest erwartet die für B2a/B2b bekannte Aufnahme und ihre
Prüfsumme. Ohne diese Datei verwenden die Tests vorhandene öffentliche Kurzsignale
beziehungsweise programmgenerierte Test-WAVs. Die private Aufnahme ist kein
Repository-Asset und wird nicht heruntergeladen oder veröffentlicht.

B2b ergänzt keine Audioverwaltung oder neuen Ports. B2c-1 ergänzt nachfolgend
die ausdrückliche Ubuntu-Testinstallation; vollständige Verwaltung und weitere
Backup-/Restore-Erweiterungen bleiben späteren Phasen vorbehalten.

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
| Private Testaufnahme | `/etc/mumble-funk/sounds/radio-noise.wav` |

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
beachten. **Rauschen bleibt ausgeschaltet**, auch ohne veröffentlichte Rauschdatei.
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

### C. Private Aufnahme vom eigenen Computer übertragen

Auf dem **eigenen Computer**, in dem Ordner mit `static.wav` (PowerShell unter
Windows mit OpenSSH oder ein Linux-/macOS-Terminal): Benutzer und VM-IP ersetzen.
Für SCP muss der SSH-Dienst der VM erreichbar sein.

```bash
scp static.wav BENUTZER@VM-IP:radio-noise.wav
```

Alternativ mit einem SFTP-Programm nach `/home/BENUTZER/radio-noise.wav` kopieren.
Nicht nach GitHub oder ins Git-Verzeichnis übertragen. Das Original auf dem
Computer bleibt erhalten. Danach auf der **Ubuntu-VM**:

```bash
sudo install -d -o root -g mumble-funk -m 0750 /etc/mumble-funk/sounds
sudo install -o root -g mumble-funk -m 0640 \
  "$HOME/radio-noise.wav" /etc/mumble-funk/sounds/radio-noise.wav
```

Vergleich und Leserechte prüfen:

```bash
sudo cmp "$HOME/radio-noise.wav" /etc/mumble-funk/sounds/radio-noise.wav
sudo -u mumble-funk test -r /etc/mumble-funk/sounds/radio-noise.wav
sudo stat -c '%U:%G %a %n' /etc/mumble-funk/sounds /etc/mumble-funk/sounds/radio-noise.wav
```

Erwartet: Verzeichnis `root:mumble-funk 750`, Datei `root:mumble-funk 640`.
`cmp` und `test` bleiben bei Erfolg still. Die Datei wird bytegleich kopiert, nicht
resampelt, normalisiert oder umkodiert. Der Dienst erhält Leserecht, kein Schreibrecht.
Der Pfad liegt außerhalb von `ProtectHome` und ist unter `ProtectSystem=strict`
lesbar. Keine Rauschaufnahme wird durch den Installer geliefert oder ersetzt.

### D. Bewusst einschalten und Konfiguration prüfen

Zunächst nur die INI sichern und öffnen:

```bash
sudo cp -a /etc/mumble-funk/server.ini "/etc/mumble-funk/server.ini.before-noise-$(date -u +%Y%m%dT%H%M%S)"
sudo nano /etc/mumble-funk/server.ini
```

Diese drei Schlüssel **jeweils genau einmal** setzen:

```ini
radionoise=true
radionoisefile=/etc/mumble-funk/sounds/radio-noise.wav
radionoisevolume=20
```

Vorhandene aktive Einträge ersetzen, nicht nochmals anhängen. Fehlende Schlüssel
im allgemeinen Bereich **vor der ersten `[Abschnitt]`-Zeile** einfügen, oder in
einem bereits vorhandenen `[General]`-Abschnitt. Keinen `[server]`-Abschnitt und
keinen zweiten `[General]`-Abschnitt erfinden. Andere Optionen, insbesondere
Passwort, Datenbank und bestehende Audioeinstellungen, nicht ändern. Mit Strg+O,
Enter speichern und mit Strg+X schließen.

Dieser nur lesende Check prüft die drei Werte und Doppelungen im allgemeinen
INI-Bereich, ohne Passwörter auszugeben:

```bash
sudo python3 - <<'PY'
from pathlib import Path
wanted = {'radionoise': 'true',
          'radionoisefile': '/etc/mumble-funk/sounds/radio-noise.wav',
          'radionoisevolume': '20'}
found = {key: [] for key in wanted}
section = 'General'
for line in Path('/etc/mumble-funk/server.ini').read_text(encoding='utf-8-sig').splitlines():
    line = line.strip()
    if not line or line.startswith(('#', ';')):
        continue
    if line.startswith('['):
        section = line
        if line == '[General]':
            section = 'General'
        continue
    if section == 'General' and '=' in line:
        key, value = (part.strip() for part in line.split('=', 1))
        if key in found:
            found[key].append(value)
for key, expected in wanted.items():
    if found[key] != [expected]:
        raise SystemExit(f'{key}: fehlt, doppelt oder falscher Wert; INI prüfen, noch nicht neu starten.')
    print(f'{key}={expected}')
print('INI-Prüfung erfolgreich.')
PY
sudo mumble-funk-audio status
```

`status` zeigt die bisherigen Audioeinstellungen; die neuen Rauschwerte werden
oben separat geprüft. Falls in einer älteren Installation Effekte deaktiviert
sind, bleiben sie so; für den gewünschten Hörtest die vorhandenen Verwaltungs-
befehle aus [squelch-ubuntu.md](squelch-ubuntu.md) und [audio-volume.md](audio-volume.md)
verwenden. Keine pauschale Migration. Falls die Datenbank verwaltete Einstellungen
überlagert, bei abweichendem Verhalten erst deren Herkunft klären, nicht die
Datenbank direkt verändern.

Nach erfolgreicher Prüfung nur diesen Dienst neu starten:

```bash
sudo systemctl restart mumble-funk
sudo systemctl --no-pager --full status mumble-funk
sudo tail -n 60 /var/log/mumble-funk/server.log
```

Erwartet wird im **neuen Startabschnitt** `Experimental radio noise enabled:`.
`Radio noise disabled:` weist auf ungültige Lautstärke/Datei oder einen Ladefehler
hin. Pfad, Format und Leserechte prüfen; andere Audioeffekte sollen weiterlaufen.
Bei Volume 0 wird Rauschen bewusst deaktiviert. Ein Neustart ist nach Änderungen
an Datei, Pfad oder Lautstärke erforderlich.

### E. Wieder deaktivieren

```bash
sudo nano /etc/mumble-funk/server.ini
```

Den vorhandenen allgemeinen Eintrag auf `radionoise=false` ändern, nur einmal.
Pfad, Lautstärke, WAV und andere Audiooptionen stehen lassen. Danach:

```bash
sudo systemctl restart mumble-funk
sudo systemctl --no-pager --full status mumble-funk
```

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

Diese Anleitung dokumentiert **noch keine erfolgreichen Geräte-Hörtests** oder
vollständige systemd-/Proxmox-VM-Praxistests. Solche Ergebnisse liefert erst der
Betreiber. Installation, Verwaltung und Veröffentlichung des offiziellen
Rausch-Assets sowie Docker-Erweiterungen bleiben späteren Phasen vorbehalten.

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

**Erneuter Geräte-Hörtest erforderlich:** Auf der separaten Snapshot-Test-VM mit
der ursprünglichen Close-WAV testen, nicht mit einer um Stille verlängerten Kopie.
Windows → Mumla für 2, 10 und mindestens 62 Sekunden, Noise an/aus; anschließend
Mumla → Windows mit Timeout-Fallback, sehr kurzes PTT während Open, schnelle
Wechsel und Busy prüfen. Erste/letzte Sprachsilben vollständig, Close genau einmal,
keine hängenbleibenden Effekte. Automatisierte Server-, Opus- und Puffermodelltests
bestätigen keine tatsächliche Wiedergabe auf dem Android-Gerät. Bis zum erneuten
Hörtest bleibt die Version experimentell.
