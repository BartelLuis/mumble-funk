# Sprachpaketdiagnose für Mumble-Funk

Die Diagnose untersucht eingehende Sprachpakete auf unserem Zweig `funk-v0.1`,
ausgehend von Mumble Server 1.5.939. Sie ist standardmäßig ausgeschaltet.
Sie vergibt kein Senderecht und verändert die Sprachweiterleitung nicht.
Eine normale Mumble-Installation aus dem Ubuntu-Paketarchiv enthält diese
Erweiterung nicht: Zum Testen wird der hier gebaute Server benötigt.

Für einen dauerhaften Parallelbetrieb unter Ubuntu 24.04 oder 26.04 mit systemd auf Port
**64739** verwende die [Installationsanleitung](installation-ubuntu.md).
Das spätere Beispiel hier auf Port 64749 ist ein separater, manuell gestarteter
Kurztest und wird dafür nicht zusätzlich benötigt.

## Was aufgezeichnet wird

Jede Zeile der Ergebnisdatei ist ein eigenständiges JSON-Objekt (JSONL).

| Ereignis (`event`) | Bedeutung |
| --- | --- |
| `diagnostics_started` | Ausgabe gestartet; enthält UTC-Referenzzeit und Timeout-Einstellung. |
| `stream_start` | Erstes Paket mit Audionutzdaten eines neu erkannten Stroms. Kein Beweis eines physischen Tastendrucks. |
| `packet` | Gültig dekodiertes Sprachpaket einer authentifizierten Verbindung. |
| `end_marker` | Das Paket enthält tatsächlich eine Ende-Markierung. |
| `stream_end_marker` | Die Ende-Markierung wurde dem beobachteten aktiven Strom zugeordnet (`marker_confirmed`). |
| `stream_end_timeout` | Die eingestellte Zeit ohne Sprachpaket ist verstrichen; Ende nur vermutet (`suspected_only`). |
| `channel_changed` / `disconnected` | Kanalwechsel oder Verbindungsende. |
| `stream_interrupted` | Beobachtung durch Kanal-/Zielwechsel, Verbindungsende, Serverstopp oder Diagnoseverlust unterbrochen. Kein bestätigtes PTT-Ende. |
| `diagnostic_loss` | Diagnosedatensätze konnten nicht aufgenommen werden; Anzahl steht in `dropped_records`. |
| `diagnostics_stopped` | Ausgabe bei normalem Serverstopp abgeschlossen; Gesamtzahl ausgelassener Datensätze. |

`packet` enthält insbesondere:

- `run`: Kennung des Diagnoselaufs; `server`: Nummer des virtuellen Servers;
- `connection`: Verbindungskennung innerhalb dieses Laufs; `session`: Mumble-Sitzungsnummer;
- `user`: Benutzername bei der Anmeldung, auf 256 Zeichen begrenzt;
- `channel`: aktuelle Mumble-Kanalnummer; `channel_name_at_join`: Name beim
  letzten Kanalbeitritt, auf 256 Zeichen begrenzt;
- `target`: Mumble-Sprachziel (0 = normale Kanalsprache, 31 = Server-Loopback,
  andere Werte = Flüster-/Rufziele);
- `transport`: `udp` oder `tcp`; `codec`: numerischer Mumble-Codec;
- `time_us`: monotone Beobachtungszeit in Mikrosekunden seit Start der Diagnose;
- `gap_us`: Abstand zum vorherigen beobachteten Sprachpaket dieser Verbindung;
- `frame`: Position im Audioframestrom; `payload_bytes`: Länge der Audionutzdaten;
- `is_last_frame`: Ende-Markierung im Paket; `older_frame`: kleinere Frameposition
  als die bislang höchste Position im aktiven beobachteten Strom;
- `stream`: Kennung des erkannten Stroms innerhalb dieses Diagnoselaufs.

Die 64-Bit-Werte werden als Dezimalstrings gespeichert, damit JSON-Auswerter
keine Genauigkeit verlieren. `gap_us` ist `null`, wenn kein Vorgänger bekannt
ist, etwa nach Anmeldung, Kontextwechsel oder Diagnoseverlust. Nach mindestens
60 Sekunden bzw. zweimal der Timeout-Frist wird inaktive Historie entfernt;
bei der nächsten Übertragung kann der Abstand deshalb ebenfalls unbekannt sein.
Kanalnummern und Verbindungskennungen sind für die Zuordnung maßgeblich:
nachträgliche Umbenennungen ändern die Namens-Schnappschüsse nicht.

**Es werden keine Sprachinhalte gespeichert.** Das Diagnosemodul bekommt weder
Audiobytes noch Schlüssel oder Passwörter. Namen und technische Kennungen sind
Metadaten. Die Dateien werden mit Lese-/Schreibrechten nur für den Serverbenutzer
angelegt. Zum Teilen reicht ein kurzer Ausschnitt mit den relevanten Ereignissen.

## Aktivieren und Ergebnisse finden

In der vom eigenen Server verwendeten INI-Datei ergänzen:

```ini
voicediagnostics=true
voicediagnosticsfile=/pfad/zu/einem/beschreibbaren/verzeichnis/voice
voicediagnosticstimeout=500
voicediagnosticsmaxsize=10485760
```

Danach den Server neu starten. Auch Änderungen an Pfad, Frist und Dateigröße
werden erst beim Neustart übernommen. Die Frist ist in Millisekunden, die Größe
in Bytes angegeben. Fristen werden auf 50 bis 60000 ms begrenzt; kleinere
Dateigrenzen als 4096 Bytes werden auf 4096 angehoben.

`voicediagnosticsfile` ist ein **Dateipräfix**, kein vollständiger Dateiname.
Für den virtuellen Server 1 entsteht im Beispiel `voice.1.jsonl`. Eine relative
Angabe wie `voice-diagnostics` wird relativ zur INI-Datei aufgelöst. Ohne eigene
Pfadangabe entsteht dort `voice-diagnostics.1.jsonl`. Das Verzeichnis muss bereits
existieren und für den Serverbenutzer beschreibbar sein.

Standardmäßig darf eine Datei etwa 10 MiB groß werden. Danach wird sie nach
`voice.1.jsonl.1` verschoben; eine ältere Sicherung wird ersetzt. Es gibt eine
aktuelle Datei und eine Sicherung, zusammen standardmäßig höchstens etwa 20 MiB
pro virtuellem Server. Auch ein Neustart hängt an die vorhandene aktuelle Datei
an; `run` unterscheidet die Läufe. Rotationen können eine Messreihe teilen.

Bei fehlenden Schreibrechten oder einem Ausgabefehler wird die Diagnoseausgabe
gestoppt und eine Meldung mit `Voice diagnostics:` im normalen Serverprotokoll
ausgegeben. Der Funk-/Sprachbetrieb wird dadurch nicht gesperrt. Nach Behebung
des Fehlers den Server neu starten.

Zum Abschalten `voicediagnostics=false` setzen und den Server neu starten.
Bereits geschriebene Dateien bleiben zur Auswertung erhalten.

## Einfacher Test auf Ubuntu

Die folgenden Schritte starten einen separaten Testserver auf Port **64749**
mit eigener Datenbank. Ein vorhandener Server auf Port 64738 wird nicht ersetzt.
Die Abhängigkeiten entsprechen der Installationslösung für Ubuntu 24.04/26.04.
Der ursprüngliche automatische Diagnosetest wurde in der Codex-Umgebung auf
Debian 13 mit Qt 5.15.15 ausgeführt; aktuelle Installationsprüfungen sind in der
[Ubuntu-Anleitung](installation-ubuntu.md) beschrieben.

### 1. Werkzeuge und Quellcode bereitstellen

```bash
sudo apt update
sudo apt install git build-essential cmake ninja-build pkg-config \
  qtbase5-dev qtbase5-dev-tools libqt5sql5-sqlite libboost-dev \
  libssl-dev libprotobuf-dev protobuf-compiler libprotoc-dev libcap-dev \
  libavahi-compat-libdnssd-dev libzeroc-ice-dev

git clone --branch funk-v0.1 --recurse-submodules \
  https://github.com/Shawna7han/mumble-funk.git mumble-funk
cd mumble-funk
```

Wenn das Verzeichnis bereits vorhanden ist, nicht erneut darüber kopieren.
Zuerst den bestehenden Arbeitsstand prüfen. Für einen frischen Test kann ein
anderer Verzeichnisname verwendet werden.

### 2. Server bauen und testen

```bash
cmake -S . -B build/funk-v0.1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_NUMBER=939 \
  -Dclient=OFF -Dserver=ON -Dtests=ON -Dplugins=OFF -Doverlay=OFF -Dlto=OFF
cmake --build build/funk-v0.1 --parallel 2
ctest --test-dir build/funk-v0.1 --output-on-failure
```

Die Tests umfassen lokale TLS-/UDP-Verbindungen und benötigen die Erlaubnis,
lokale Netzwerksockets zu verwenden. Sie brauchen kein Android-Gerät und keinen
extern erreichbaren Server. In Codex ist dafür die Netzwerkberechtigung des
Executors erforderlich. Auf einer normalen Ubuntu-Maschine ist keine besondere
Freigabe für die lokalen Tests nötig.

### 3. Testkonfiguration anlegen

```bash
mkdir -p "$HOME/mumble-funk-test"
cat > "$HOME/mumble-funk-test/server.ini" <<EOF
database=$HOME/mumble-funk-test/server.sqlite
host=0.0.0.0
port=64749
users=10
bandwidth=72000
serverpassword=FunkTest-2026
ice=
bonjour=false
logfile=
voicediagnostics=true
voicediagnosticsfile=$HOME/mumble-funk-test/voice
voicediagnosticstimeout=500
voicediagnosticsmaxsize=10485760
EOF

./build/funk-v0.1/mumble-server -ini "$HOME/mumble-funk-test/server.ini" -fg
```

Das Terminal bleibt für den laufenden Testserver geöffnet. Beenden mit **Strg+C**.
Für dieses alte Mumble-Release gelten `-ini` und `-fg`; die langen Optionen
`--ini` und `--foreground` werden nicht unterstützt.

### 4. Zwei unveränderte Mumla-Geräte verbinden

- Serveradresse: LAN-IP-Adresse des Ubuntu-Rechners (mit `hostname -I` anzeigen).
- Port: **64749**; Passwort: **FunkTest-2026**.
- Unterschiedliche Benutzernamen wählen und denselben Mumble-Kanal betreten.
- Den Testserver mit seinem selbst erzeugten Zertifikat anhand der in Mumla
  angezeigten Angaben bestätigen.
- Sendemodus „Zum Sprechen drücken“ wählen. Die angeheftete PTT-Funktion
  ausschalten, damit Loslassen tatsächlich den lokalen Sendezustand beendet.
- „Opus Codec vermeiden“ ausschalten. Zunächst „Audio pro Paket“ auf 20 ms setzen.
- Falls eine Firewall aktiv ist, TCP und UDP auf Port 64749 für das Testnetz zulassen.

Version von Mumla und Android sowie Gerätetyp notieren. Dann auf Gerät A:

1. Ohne PTT sprechen: Es sollten keine `packet`-Ereignisse von A erscheinen.
2. PTT halten und fünf Sekunden sprechen: `stream_start` und `packet` beobachten.
3. Bei weiterhin gehaltener PTT 30 Sekunden schweigen, dann wieder sprechen:
   Paketabstände beobachten. Stille ist kein automatischer Endebeweis.
4. Loslassen: Es kann `end_marker` mit `stream_end_marker` erscheinen.
   Fehlt die Markierung, sollte nach der Frist `stream_end_timeout` erscheinen.
5. Dasselbe mit 10 und 40 ms pro Paket sowie kurzen Tastendrücken wiederholen.
6. Einen Kanalwechsel durchführen und die geänderte Kanalnummer prüfen. Falls
   kein zweiter Kanal existiert, zunächst auf dem Testserver einen Benutzer
   registrieren und mit ihm einen temporären Testkanal anlegen.
7. In Mumla „Nur TCP“ einschalten, neu verbinden und den Test wiederholen.
   Das Feld `transport` zeigt den tatsächlich beobachteten Transport.

Danach die Senderrolle tauschen. Für einen abrupten Netzausfall den Zugangspunkt
von außen abschalten, während A die PTT-Taste hält. Das Öffnen der Android-
Einstellungen kann selbst den Sendezustand beenden und ist kein eindeutiger
Test eines plötzlichen Netzverlusts.

### 5. Ausgabe ansehen

In einem zweiten Terminal:

```bash
tail -f "$HOME/mumble-funk-test/voice.1.jsonl"
```

Optional übersichtlich mit `jq` (zuerst `sudo apt install jq`):

```bash
tail -f "$HOME/mumble-funk-test/voice.1.jsonl" |
  jq -r '[.event, .user, .channel, .transport, .gap_us] | @tsv'
```

Ein `gap_us` von `20000` bedeutet 20 Millisekunden zwischen zwei beobachteten
Paketen. Die erste Zeile `diagnostics_started` bestätigt, dass die Ausgabe
geöffnet werden konnte. Für längere Tests auch die rotierte `.1`-Datei aufbewahren.

## Grenzen und technische Umsetzung

Der Messzeitpunkt liegt nach erfolgreicher Audio-Dekodierung und Zuordnung zu
einer authentifizierten Verbindung, vor Codec-/Mute-/Bandbreitenfilterung. Ein
`packet` bedeutet daher **eingegangen**, nicht notwendigerweise weitergeleitet.
Ungültige oder nicht entschlüsselbare Pakete und allgemeine Pings werden nicht
als Sprache aufgezeichnet. TCP-Zeitpunkte beziehen sich auf vollständige
UDPTunnel-Nachrichten, nicht auf einzelne Netzwerksegmente. Paketabstände
enthalten Netz- und Schedulingeffekte und sind keine Mikrofonaufnahmezeiten.

Der Server sieht keine separate physische PTT-Tastenmeldung. Ende-Markierungen
können vom Client ausgelassen werden, verloren gehen oder verspätet eintreffen.
Eine Markierung mit kleinerer Frameposition wird deshalb protokolliert, beendet
aber vorsichtshalber nicht den aktuellen beobachteten Strom. Framezähler können
auch neu starten; `older_frame` ist kein eindeutiger Beweis eines Netzfehlers.
Ohne eindeutige PTT-Kennung ist auch bei anderen Framepositionen die Zuordnung
verspäteter Markierungen nicht zweifelsfrei möglich. Rohereignisse bleiben zur
späteren Auswertung erhalten. Ein Timeout ist immer
nur eine Vermutung und kann bei Netzstörungen auch während gehaltener PTT auftreten.

Bei ausgeschalteter Diagnose wird kein Diagnoseobjekt, keine Warteschlange und
kein Ausgabethread angelegt. Die Sprachpfade prüfen nur den leeren Diagnosezeiger.
Bei eingeschalteter Diagnose kopieren sie ausschließlich begrenzte Metadaten in
eine vorab reservierte Warteschlange mit 4096 Einträgen. Es gibt dort keine
Dateizugriffe und kein Warten auf den Schreiber. Ist die Warteschlange voll oder
kurzzeitig belegt, werden Diagnosedatensätze verworfen und gezählt; Audio wird
weiter wie bisher verarbeitet. `diagnostic_loss` kennzeichnet die Lücke und setzt
die abgeleitete Stromzuordnung zurück. Der Schreiber verwaltet maximal 16384
Verbindungszustände und kann bei Erreichen dieser Grenze die Beobachtung mit
`state_capacity` zurücksetzen. Die Diagnose ist damit eine begrenzte Beobachtung,
keine garantierte lückenlose Paketaufzeichnung bei beliebiger Last.

Nur der Schreiber wertet Ströme aus, prüft Fristen und schreibt/rotiert Dateien.
Die Ausgabe wird normalerweise etwa alle 100 ms geleert; Zeitstempel bezeichnen
die Erfassung, nicht den Zeitpunkt, an dem die Zeile in der Datei sichtbar wird.
Die Warteschlange verwendet eine eigene Sperre; sie wird niemals über Dateizugriffe
gehalten. Sie enthält keine Benutzerzeiger oder geliehenen Audiopuffer. Benutzer-
und Kanalnamen werden im Hauptthread als Schnappschüsse erfasst, um gleichzeitige
Umbenennungen nicht aus dem Sprachthread zu lesen. Verbindungsende und Kanalwechsel
werden innerhalb der bestehenden Lebensdauer-Sperre erfasst. Beim normalen
Serverstopp wird zuerst der Sprachthread beendet und dann die Diagnose geleert.
Ein harter Prozessabbruch kann noch gepufferte Diagnosedaten verlieren.
