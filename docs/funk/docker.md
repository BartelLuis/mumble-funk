# Mumble-Funk mit Docker — experimentell

Die Docker- und Ubuntu-Installation sind im gemeinsamen **`stable`-Kandidaten**
zusammengeführt: Simplex-Engine v0.2 auf Basis Mumble Server 1.5.939, aus demselben
Git-Commit. `git rev-parse HEAD` zeigt die vollständige Quellrevision. Audio-Feedback
v0.3 gehört nicht dazu. Der bisherige Docker-Feature-Branch bleibt unverändert.

**Bereits installiert?** Nicht neu klonen oder Volumes neu anlegen. Verwende die
[Umstellung auf Stable mit Sicherung und Rollback](docker-stable-migration.md).
Die folgenden Schritte sind für eine **neue** Installation.

## Voraussetzungen

Führe die Befehle im Terminal deines **Docker-Hosts** aus, zum Beispiel auf einer
Ubuntu-VM. Es wird nichts automatisch auf einer anderen VM oder Proxmox installiert.

- Docker Engine mit BuildKit und **Docker Compose v2** müssen bereits installiert
  sein. Docker Engine ab 24 und Compose ab 2.24 werden empfohlen; geprüft wurden
  Engine **28.4.0** und Compose **2.40.3** auf einem Linux-amd64-Cloud-Host.
- `git`, Bash und die üblichen Linux-Werkzeuge müssen vorhanden sein. Die
  Docker-Befehle müssen für deinen Benutzer erlaubt sein. Falls du normalerweise
  `sudo docker` benötigst, verwende dies konsequent; die Sicherungsskripte dann
  entsprechend mit `sudo bash` ausführen.
- Empfohlen: **2 CPU-Kerne, 4 GiB RAM**, mindestens **12 GiB freier Docker-Speicher**,
  besser 20 GiB für Build-Cache und spätere Updates. Der Build verwendet standardmäßig
  zwei Prozesse; LTO und Unity-Build sind aus. Eine harte 4-GiB-Grenze wurde hier
  nicht separat nachgestellt. Das Laufzeit-Image benötigt ungefähr **199 MB**
  unkomprimiert; Build-Werkzeuge, Quellen, Zwischenstände und Cache mehrere GiB.
- Internetzugriff auf Docker Hub, Ubuntu-Paketquellen und GitHub. Der erste Build
  lädt Pakete und die vorhandenen, auf konkrete Commits festgelegten Submodule.
- Ein freier **Host-Port für TCP und UDP**, standardmäßig 64739.
- Für die Bedienungs- und Sicherungsskripte wird ein Linux-Host vorausgesetzt.
  Windows-/Android-Clients benötigen selbst kein Docker.

Prüfen:

```bash
docker version
docker compose version
docker info
```

Falls Docker fehlt, verwende die offiziellen Anleitungen für
[Docker Engine auf Ubuntu](https://docs.docker.com/engine/install/ubuntu/) und das
[Compose-Plugin](https://docs.docker.com/compose/install/linux/). Diese Lösung
installiert oder verändert Docker selbst nicht.

## Ubuntu-Installation oder Docker?

| Eigenschaft | Ubuntu-Schnellinstallation | Docker-Installation |
| --- | --- | --- |
| Start | `scripts/funk/setup.sh` auf `stable` | `docker compose up -d --build` auf demselben `stable`-Commit |
| Betrieb | Eigener systemd-Dienst | Eigener Container mit Neustartrichtlinie |
| Dateien | Eigene Verzeichnisse direkt auf Ubuntu | Eigene benannte Docker-Volumes |
| Port | Standardmäßig 64739 | Host-Port wählbar, intern immer 64739 |
| Änderungen am anderen Weg | Keine | Keine |

Docker ersetzt die vorhandene Installation nicht. Wenn dein nativer Funkserver
schon 64739 verwendet, wähle für Docker zum Beispiel **64740**. Wenn auch dieser
Port bereits durch v0.3 belegt ist, verwende beispielsweise 64741.

## 1. Herunterladen

Benutze ein neues Quellverzeichnis, damit vorhandene v0.2-/v0.3-Quellen erhalten
bleiben:

```bash
git clone --branch stable \
  https://github.com/Shawna7han/mumble-funk.git mumble-funk-docker
cd mumble-funk-docker
```

`--recurse-submodules` ist nicht erforderlich: Der Docker-Build beschafft die
vorhandenen Abhängigkeiten anhand von `scripts/docker/submodules.lock.json`.
Es werden weder der Desktop-Client noch die Android-App gebaut.

## 2. Optional den Host-Port festlegen

Wenn 64739 frei ist, überspringe diesen Abschnitt. Für Parallelbetrieb mit dem
nativen v0.2-Server:

```bash
cp -n .env.example .env
sed -i 's/^FUNK_HOST_PORT=.*/FUNK_HOST_PORT=64740/' .env
```

`.env` enthält nur Docker-Einstellungen, **kein Serverpasswort**. `cp -n` ersetzt
keine vorhandene Datei. Der Port steht damit dauerhaft für spätere Updates fest.
Der Server im Container bleibt auf **64739 TCP/UDP**; Docker bildet beide auf den
gewählten Host-Port ab.

Weitere Einstellungen in `.env`:

```ini
FUNK_HOST_PORT=64740
FUNK_BUILD_JOBS=2
COMPOSE_PROJECT_NAME=mumble-funk-docker
```

Bei wenig Arbeitsspeicher kann `FUNK_BUILD_JOBS=1` helfen. Zulässig sind 1 bis 16.
Für eine zweite unabhängige Docker-Instanz brauchst du zusätzlich einen **anderen
Projektnamen**, einen freien Host-Port und am besten ein eigenes Quellverzeichnis.
Ein neuer Projektname erzeugt neue Volumes; ändere ihn bei gewöhnlichen Updates
nicht versehentlich.

## 3. Bauen und starten

```bash
docker compose config --quiet
docker compose up -d --build
docker compose ps
```

Der erste Build dauert einige Minuten, auf kleinen VMs länger. Dabei werden alle
18 bestehenden Server-Testprogramme ausgeführt. Scheitert ein Test oder Build,
wird kein erfolgreiches Laufzeit-Image daraus bereitgestellt.

Beim ersten Start erzeugt die Instanz:

- ein zufälliges Zugangspasswort mit 48 Hex-Zeichen;
- ihre eigene Konfiguration mit `simplex=true`, `simplextimeout=500`,
  `simplexretrytimeout=500`, `simplexdiagnostics=true`;
- eine eigene SQLite-Datenbank und ein eigenes Mumble-Zertifikat.

Vorhandene Konfigurationen und Datenbanken werden bei normalen Neustarts oder
Updates **nicht neu erzeugt oder überschrieben**. Fehlt nur eines von beiden
unerwartet, wird zum Schutz vorhandener Daten angehalten. Eine unterbrochene
Erstinitialisierung hat einen eigenen Wiederanlaufmarker.

Der Server läuft als UID/GID **10001**, ohne Root, ohne Linux-Capabilities und mit
schreibgeschütztem Root-Dateisystem. Schreibbar sind seine eigenen Volumes und ein
begrenztes temporäres Verzeichnis. Er erhält weder Docker-Socket noch Host-Netzwerk
oder Zugriff auf native Datenbanken.

## 4. Zugangsdaten anzeigen und verbinden

```bash
docker compose exec mumble-funk funk-container info
docker compose port --protocol tcp mumble-funk 64739
docker compose port --protocol udp mumble-funk 64739
```

Das Passwort wird nur bei diesem ausdrücklich aufgerufenen Info-Befehl lokal
angezeigt, nicht bei jedem Start in die Logs geschrieben. Die Konfiguration hat
standardmäßig Dateirechte **0600**. Ein Benutzer mit Docker-Administratorrechten
kann grundsätzlich auf die Containerdaten zugreifen.

In **Mumla und Windows-Mumble** einen neuen Servereintrag erstellen:

| Feld | Eingabe |
| --- | --- |
| Name | Zum Beispiel Mumble-Funk Docker |
| Adresse | Die erreichbare IP-Adresse deines Docker-Hosts |
| Port | Gewählter **Host-Port**, zum Beispiel 64740 |
| Benutzername | Eigener Name, z. B. Android oder Windows |
| Passwort | Ausgabe von `funk-container info` |

`0.0.0.0` aus `docker compose port` ist keine Adresse, die du am Telefon eintragen
sollst: Verwende die IP-Adresse der VM/des Hosts. Verbinde die Geräte mit demselben
Kanal. Beide Clients bleiben unverändert.

Die Instanz erzeugt ein eigenes Zertifikat. Prüfe die Zertifikatsanzeige beim ersten
Verbinden nach deinem üblichen Mumble-Verfahren. Das Zertifikat bleibt bei Updates
in der eigenen Datenbank erhalten.

Firewalls und Router werden nicht verändert. Bei Fernzugriff müssen der gewählte
Host-Port **für TCP und UDP** erreichbar sein, gegebenenfalls auch durch Proxmox und
den Router. Beachte, dass Docker veröffentlichte Ports eigene Firewall-Regeln
verwenden; eine UFW-Regel allein beschreibt nicht zwingend den tatsächlichen Zugang.

## 5. Logs und Zustandsprüfung

```bash
docker compose logs --tail=100 mumble-funk
docker compose logs -f mumble-funk
docker compose exec mumble-funk funk-container health
```

Mit `Strg+C` beendest du die laufende Loganzeige, nicht den Server.

| Prüfung | Was sie nachweist |
| --- | --- |
| Laufender Container | Prozess läuft, noch kein Sprachtest |
| Healthcheck/`healthy` | Lokaler TLS-Handshake mit Mumble-Version-Header und passender lokaler UDP-Ping |
| Verbindung eines Clients | Tatsächliche Anmeldung über den veröffentlichten TCP-Port |
| Zwei/drei Clients mit PTT-Test | Sprache über das tatsächliche Netzwerk und Simplex-Verhalten |

Der Healthcheck beweist **keine verschlüsselte Sprachübertragung aus dem Internet**
und keine hörbare Android-/Windows-Ausgabe. Dafür ist der Praxistest unten nötig.

Docker begrenzt seine normalen Containerlogs auf drei Dateien zu je 10 MB. Die
Simplex-Diagnose schreibt separat in das Log-Volume und rotiert bei ungefähr
10 MiB je Datei. Es werden serverseitig keine Sprachinhalte aufgezeichnet.

## 6. Simplex-Diagnose ohne INI-Editor ändern

Die ersten beiden Befehle benötigen einen gestoppten Server, damit die Einstellung
nicht gleichzeitig verändert wird:

```bash
docker compose stop mumble-funk
docker compose run --rm --no-deps mumble-funk set simplexdiagnostics false
docker compose up -d
```

Mit `true` schaltest du sie wieder ein. Simplex funktioniert weiterhin, wenn die
Diagnose ausgeschaltet ist. Entsprechend sind `voicediagnostics`, `simplex`,
`simplextimeout` und `simplexretrytimeout` einstellbar. Die Zeitangaben sind
Millisekunden zwischen 50 und 60000; die bestehenden Regeln begrenzen die wirksame
Wiederholungszeit auf mindestens die Kanalzeit.

Diagnose ansehen:

```bash
docker compose exec mumble-funk tail -n 30 /var/log/mumble-funk/simplex.1.jsonl
```

Die Serverausgaben stehen in Docker-Logs; mit `logdays=-1` wird keine zusätzliche,
unbegrenzt wachsende Serverlog-Kopie in der SQLite-Datenbank angelegt. Benutzer- und
Kanaldaten bleiben davon unberührt.

## 7. Optional: Administratorkonto einrichten

Das Zugangspasswort verleiht keine Administratorrechte. `SuperUser` ist zunächst
für Passwortanmeldungen deaktiviert, damit der unveränderte Server beim Erststart
kein zusätzliches zufälliges Administratorpasswort in die Logs schreibt.

Wenn du Kanäle und Berechtigungen verwalten möchtest:

```bash
docker compose stop mumble-funk
docker compose run --rm --no-deps mumble-funk admin-password
docker compose up -d
```

Gib das neue Passwort zweimal verdeckt ein. Verwende 12 bis 200 druckbare
ASCII-Zeichen, beispielsweise ein langes vom Passwortmanager erzeugtes Passwort.
Es wird über die Standardeingabe an Mumble übergeben, nicht als Prozessargument
oder INI-Klartext gespeichert. Verwende es danach mit dem Benutzernamen `SuperUser`.
Zum normalen Sprechen wieder als gewöhnlicher Benutzer verbinden: Mumble erlaubt
SuperUser grundsätzlich kein Sprechen.

## 8. Stoppen, starten, neu starten oder neu erstellen

```bash
# Stoppen:
docker compose stop

# Vorhandenen Container starten:
docker compose start

# Neu starten:
docker compose restart

# Container neu erstellen, vorhandene Volumes weiterverwenden:
docker compose up -d --force-recreate

# Container und Projektnetzwerk entfernen, Daten behalten:
docker compose down

# Danach wieder erstellen:
docker compose up -d
```

`restart: unless-stopped` startet eine laufende Instanz nach einem Docker-/Host-
Neustart wieder, sofern Docker selbst automatisch startet. Eine ausdrücklich
gestoppte Instanz bleibt gestoppt. Ein echter Host-Neustart bleibt ein Praxistest.

Docker verwendet einen Init-Prozess zur Signalweitergabe. Das Startprogramm wird
per `exec` durch Mumble ersetzt; SIGTERM wird weitergegeben und bis zu 30 Sekunden
für einen ordentlichen Abschluss eingeräumt. Eine zusätzliche Dateisperre verhindert
zwei Serverprozesse oder Offline-Verwaltung auf denselben Daten gleichzeitig.

## 9. Aktualisieren

Bleibe im selben Verzeichnis mit demselben Projektnamen und denselben Volumes:

```bash
mkdir -p backups
bash scripts/docker/manage.sh backup "backups/vor-update-$(date +%Y%m%d-%H%M%S).tar.gz"
git pull --ff-only origin feature/docker-install
docker compose up -d --build
docker compose ps
```

Bei Git-Konflikten nicht mit Zurücksetzen/Erzwingen weiterarbeiten. Den Fehler erst
prüfen lassen. Ein fehlgeschlagener Image-Build ersetzt den laufenden Container
nicht. Das neue Image wird lokal gebaut, nicht aus einer öffentlichen
Mumble-Funk-Image-Registry heruntergeladen.

Für eine bewusste Erneuerung auch der zwischengespeicherten Ubuntu-Pakete:

```bash
docker compose build --pull --no-cache
docker compose up -d
```

Dabei bleiben die Datenvolumes ebenfalls erhalten. Cache-/Paketstände sind nicht
auf ewig eingefroren: Der Ablauf ist reproduzierbar, nicht bytegenau identisch über
alle späteren Ubuntu-Paketupdates hinweg.

**Zum Aktualisieren niemals `docker compose down -v` benutzen.** Dieser Zusatz
löscht die persistenten Volumes und gehört nicht zum Update-Verfahren.

## 10. Wo die Daten liegen

| Containerpfad | Benanntes Volume bei Standard-Projektname | Inhalt |
| --- | --- | --- |
| `/etc/mumble-funk` | `mumble-funk-docker_funk_config` | Konfiguration und Zugangspasswort |
| `/var/lib/mumble-funk` | `mumble-funk-docker_funk_state` | SQLite, Zertifikat, Konten, Kanäle, Rechte; künftiges `audio/` |
| `/var/log/mumble-funk` | `mumble-funk-docker_funk_logs` | Paket-/Simplex-Diagnose |

Anzeigen:

```bash
docker volume ls --filter label=com.docker.compose.project=mumble-funk-docker
```

Bei anderem Projektnamen den Filter anpassen. Docker verwaltet diese Volumes; nicht
mit `/var/lib/mumble-funk` der nativen Installation verwechseln. **Nie beide
Installationen auf dieselbe SQLite-Datei zeigen lassen.**

## 11. Konsistent sichern

Eine laufende SQLite-Datei nicht einfach mit `cp` kopieren. Verwende:

```bash
mkdir -p backups
bash scripts/docker/manage.sh backup "backups/mumble-funk-$(date +%Y%m%d-%H%M%S).tar.gz"
```

Das Skript stoppt **nur diesen Compose-Dienst**, hält während der Sicherung eine
exklusive Datensperre, prüft SQLite und archiviert Konfiguration, vollständiges
Datenverzeichnis einschließlich Zertifikat sowie Logs. War der Dienst vorher aktiv,
wird er anschließend wieder gestartet, auch bei einem Sicherungsfehler. Es gibt
also eine kurze Unterbrechung; Clients müssen gegebenenfalls neu verbinden.

Das Archiv wird erst nach erfolgreicher Sicherung mit Rechten **0600** veröffentlicht;
ein vorhandenes Zielarchiv wird nicht ersetzt. Es enthält Zugangsdaten und den
privaten Zertifikatsschlüssel und sollte deshalb vertraulich außerhalb des Hosts
aufbewahrt werden. Das Quellverzeichnis `backups/` ist von Git und Build-Kontext
ausgeschlossen. Die Images selbst müssen nicht gesichert werden.

## 12. Wiederherstellen, ohne vorhandene Daten zu überschreiben

Die Wiederherstellung akzeptiert absichtlich **nur leere Ziel-Volumes**. So kann ein
falsches Archiv keine laufende Installation ersetzen. Verwende ein neues
Verzeichnis, einen neuen Projektnamen und einen freien Port. Beispiel:

```bash
cd ~
git clone --branch stable \
  https://github.com/Shawna7han/mumble-funk.git mumble-funk-wiederherstellung
cd mumble-funk-wiederherstellung
cp .env.example .env
sed -i 's/^COMPOSE_PROJECT_NAME=.*/COMPOSE_PROJECT_NAME=mumble-funk-restored/' .env
sed -i 's/^FUNK_HOST_PORT=.*/FUNK_HOST_PORT=64741/' .env
docker compose build
bash scripts/docker/manage.sh restore /ABSOLUTER/PFAD/ZUR/SICHERUNG.tar.gz
docker compose up -d
docker compose exec mumble-funk funk-container info
```

**Nicht vor dem Wiederherstellen `up` ausführen**, sonst werden die neuen Volumes
bereits initialisiert. Falls dies versehentlich geschieht, verwende einen weiteren
neuen Projektnamen. Die ursprüngliche Instanz bleibt erhalten.

Vor dem Übertragen werden Archivpfade und SQLite geprüft. Symbolische Links,
Sonderdateien und Pfade außerhalb der drei vorgesehenen Bereiche werden abgewiesen.
Grenzen: 2 GiB komprimiert, 8 GiB entpackt und höchstens etwa 100000 Einträge. Für das
Zwischenlagern und Wiederherstellen muss zusätzlich ausreichend Platz vorhanden
sein. Die Konfiguration wird zuletzt bereitgestellt. Bei einem Speicher-/I/O-Fehler
bleiben ursprüngliche Instanzen unangetastet; ein teilweise gefülltes neues Ziel
muss geprüft bzw. für einen weiteren Versuch ein neues Ziel gewählt werden.

Teste die Anmeldung und PTT auf dem Wiederherstellungsport, bevor du später selbst
entscheidest, den alten Dienst abzuschalten. Es gibt kein automatisches Löschen
alter Volumes, keine automatische Migration und keine Freigabe eines belegten Ports.

## 13. Einfacher Praxistest mit echten Geräten

1. Mumla und Windows-Mumble am **Host-Port** anmelden, im gleichen Kanal.
2. Windows hält PTT und spricht. Android versucht gleichzeitig zu sprechen.
   Ein dritter Zuhörer soll ausschließlich Windows hören.
3. Android hält PTT weiter. Windows hört auf. Android darf nicht automatisch
   übernehmen. Android lässt PTT los, wartet mindestens eine Sekunde und startet neu.
4. Rollen tauschen und den Ablauf wiederholen.
5. Container neu starten und neu erstellen. Passwort, Zertifikat und eigene Kanäle
   müssen weiterhin passen.
6. Falls du Netzwerkprobleme untersuchen möchtest, TCP-erzwungenen Clientbetrieb
   und normalen UDP-Betrieb getrennt testen; Client-Version und Netzweg notieren.

Ein Busy Tone wird hier **nicht** erwartet: Diese Docker-Version enthält nur v0.2.
Die bisherigen realen Mumla-/Windows-Erfahrungen mit v0.2 ersetzen keinen neuen
Hörtest über deinen konkreten Docker-Host, Router und die Proxmox-Firewall.

## 14. Fehlerbehebung

- **Port belegt:** Keinen fremden Dienst stoppen. Einen freien `FUNK_HOST_PORT` in
  `.env` wählen und erneut `docker compose up -d` ausführen.
- **Anmeldung geht, Sprache nicht:** Host-Port für **UDP und TCP** und den gesamten
  Firewall-/Routerweg prüfen. `healthy` ist ein lokaler, kein Internet-Sprachtest.
- **Passwort vergessen:** Lokal `docker compose exec mumble-funk funk-container info`.
- **Unhealthy:** `docker compose logs --tail=100 mumble-funk` und den manuellen
  Healthcheck ausführen; anschließend Rechte/Platz und unveränderte internen
  Port-/Datenbankpfade prüfen. Der interne Port muss 64739 bleiben.
- **Fehlende Konfiguration oder Datenbank:** Keine neue Datei über vorhandene Daten
  kopieren. Richtigen Projektnamen und die zugehörigen Volumes prüfen, gegebenenfalls
  aus einer vollständigen Sicherung in eine neue Instanz wiederherstellen.
- **Build bricht wegen Speicher ab:** `FUNK_BUILD_JOBS=1` in `.env` setzen und freien
  Docker-Speicher prüfen. Ein Testfehler beendet den Build; er wird nicht ignoriert.
- **Mehrere Instanzen auf denselben Volumes:** Die Datensperre blockiert den zweiten
  Prozess. Jedem Server einen eigenen Projektnamen geben.
- **Wiederherstellung verweigert:** Ziel muss leer und Archiv vollständig sein;
  vorhandene Volumes werden bewusst nicht überschrieben.

## 15. Automatisierte Prüfung und bekannte Grenzen

Die ausgeführten Prüfungen und Commits stehen im [Docker-Prüfbericht](DOCKER-STATUS.md).
Die Multi-Stage-Builds führen alle **18** bisherigen CTest-Serverprogramme aus.
Zusätzlich gibt es Docker-spezifische Tests für Initialisierung, sichere Rechte,
Sperren, Konfiguration und Archivprüfung sowie echte Compose-Integrationstests.

Die Protokollprobe benutzt authentifizierte Testclients und verschlüsselte UDP-
Pakete mit überprüften Testnutzdaten durch den veröffentlichten Host-Port. Sie
prüft die Weiterleitungs-/Simplex-Entscheidungen, keine hörbare Opus-Wiedergabe.
Der Server benötigt dafür keinen Opus-Encoder. Der echte Gerätetest bleibt offen.

Es wurden keine Simplex-Regeln oder Serverfunktionen für Docker geändert. Die im
Audio-Zweig separat behandelte seltene v0.2-UDP-Nonce/Versions-Erkennung wird hier
nicht automatisch übernommen. Die Docker-Probe und ausschließlich für den
Docker-Build erzeugte Kopien der bestehenden Simplex-/Diagnose-Testhilfen benutzen
kontrollierte, gültige Test-Nonce-Bereiche. Die
ursprünglichen Testquellen bleiben unverändert, ebenso sämtliche Sprach-/Routing-
Assertions. Die UDP-Testhilfe verwirft unpassende ungültige Datagramme und verlangt
weiterhin den passenden authentifizierten Ping. Das macht die Verpackungsprüfung
reproduzierbarer; es ist keine Behebung oder vollständige Prüfung der bekannten
v0.2-Paketverlust-/Nonce-Grenzfälle.

Keine Behauptung von Produktionsreife, abgeschlossenem Langzeittest oder getesteter
ARM-/Windows-/macOS-Hostunterstützung. Ein echter Host-Neustart und der externe
Netzweg bleiben Praxistests. Es wird **kein öffentliches Container-Image** veröffentlicht.

## 16. Lizenz und spätere Audio-Integration

Die bestehende BSD-Projektlizenz und Copyright-Hinweise bleiben erhalten. Das
Laufzeit-Image enthält die Projektlizenz, die vorhandenen Drittanbieter-Hinweise und
die Ubuntu-Paket-Lizenzdateien. Es werden ausschließlich die bisherigen
Serverabhängigkeiten und Python für die Containerverwaltung verwendet. Kein neuer
Codec-/Audioinhalt und kein unbekannter Drittanbieter-Code wird hinzugefügt.

`ldd` bestätigt Qt5, OpenSSL, Protobuf, Ice, Avahi/DBus und Systembibliotheken;
Qt-SQLite wird als Laufzeit-Plugin ausdrücklich installiert. **Opus ist für diesen
v0.2-Server keine Laufzeitabhängigkeit**; Sprachpakete werden unverändert weitergeleitet.
Compiler, Git, CMake, Testprogramme und Quellbäume sind nicht im Laufzeit-Image.

Für eine spätere, bewusst durchgeführte Übernahme in `feature/audio-feedback`:

1. Docker-Dateien in einem separaten Integrationszweig gezielt übernehmen und
   Konflikte prüfen. Kein automatisches Merge und keine Änderung von `funk-v0.1`.
2. Im Build `libopus-dev`, im Laufzeit-Image die passende Opus-Bibliothek und ihre
   Lizenzhinweise ergänzen. Versionsanzeigen und Testumfang auf v0.3 aktualisieren.
3. Den bereits persistenten Bereich `/var/lib/mumble-funk/audio/` für eigene WAV-
   Dateien nutzen und den v0.3-Verwaltungsbefehl an diesen Containerpfad anpassen.
4. Den Docker-Testhilfe-Abgleich mit den dortigen Simplex-/Diagnosetests prüfen. Bei Übernahme
   der UDP-Korrektur kann der v0.2-spezifische Testadapter entfallen; der Build hält
   bei einer unerwarteten Testhilfenversion ausdrücklich an.
5. Feedback zunächst bewusst konfigurieren, vorhandene Dateien erhalten und alle
   Audio-, Routing-, Sicherungs- und Neustarttests erneut ausführen.

Hier wird nur der Speicherort vorbereitet, keine Audiofunktion eingebaut und keine
Änderung am aktuellen Audio-Feedback-Zweig vorgenommen.
