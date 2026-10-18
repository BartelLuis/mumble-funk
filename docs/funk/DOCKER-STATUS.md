> **Historischer Prüfbericht des unveränderten Docker-Feature-Branches.**
> Für die neue Ubuntu-/Docker-Integration gilt zusätzlich der
> [aktuelle Integrationsbericht](STABLE-INTEGRATION-STATUS.md).
> Die hier beschriebenen generierten Nonce-Testadapter sind im gemeinsamen Stand
> entfernt; dort wird der tatsächliche UDP-Fehler mit Regressionstests korrigiert.

# Prüfstand: experimentelle Docker-Installation

Branch: `feature/docker-install`, abgezweigt von `funk-v0.1` bei
`95ac344838c1535f943dccc7d134a280aac6a340`. Basis ist Simplex v0.2 / Mumble 1.5.939.
Servercode, bestehende CMake-Dateien, Simplex-Engine und `scripts/funk/` bleiben
unverändert. Die Docker-spezifische CMake-Ergänzung registriert ausschließlich eine
zusätzliche Protokollprobe sowie einen expliziten Adapter für die Verbindungs-
Testhilfe. Die ursprünglichen Server-/Testquellen werden nicht bearbeitet.

[Installations- und Bedienungsanleitung](docker.md)

## Architektur

- Multi-Stage-Build mit Ubuntu 24.04, Server-only, Release, zwei Build-Prozesse als
  Standard; Anzahl einstellbar. Vorhandene Submodule auf konkrete Commits fixiert.
- Laufzeit-Image ungefähr 199 MB unkomprimiert, ohne Compiler, Git oder Quellen.
  Tatsächlich verknüpfte Bibliotheken mit `ldd` geprüft; SQLite-Plugin vorhanden,
  kein unnötiger Opus-Encoder für v0.2.
- UID/GID 10001, keine Capabilities, kein Root, schreibgeschütztes Root-Dateisystem,
  eigene benannte Volumes; kein Docker-Socket und kein Host-Netzwerk.
- TCP und UDP explizit vom wählbaren Host-Port auf internen Port 64739 abgebildet.
- Erstkonfiguration mit Simplex und Diagnose; zufälliges Passwort nur im privaten
  Config-Volume. SuperUser-Anmeldung zunächst deaktiviert, keine automatischen
  Administratorpasswörter in Logs. Optional interaktive Einrichtung ohne Geheimnisse
  in Prozessargumenten.
- Offline-Sicherungen stoppen ausschließlich die eigene Instanz, prüfen SQLite und
  sichern alle drei Volumes. Restore nur in leere Ziele, mit Archivpfadprüfung.
- `audio/` liegt vorsorglich im Datenvolume, enthält aber keine implementierte
  Audio-Feedback-Funktion.

## Automatisierte Tests

Der abschließende Image-Build und die echte Docker-Integration am 18.10.2026
waren erfolgreich:

| Prüfung | Ergebnis |
| --- | --- |
| Dockerfile Multi-Stage-Build mit echten Ubuntu-Paketen | Erfolgreich |
| Alle vorhandenen CTest-Serverprogramme | 18/18 erfolgreich |
| `docker compose config` | Erfolgreich |
| Docker-spezifische Hilfsprogrammtests | 11 erfolgreich |
| Rootloser Start, Dateirechte, schreibgeschütztes Root, Capabilities | Erfolgreich |
| Explizite TCP-/UDP-Veröffentlichung und abweichender Host-Port | Erfolgreich |
| Anmeldung/TCP und authentifizierte verschlüsselte UDP-Pakete | Erfolgreich |
| Exklusiver Sender, keine automatische Übernahme, gültiges Ende/neuer Versuch | Erfolgreich |
| Simplex bei deaktivierter Diagnose | Erfolgreich |
| Neustart und Neuerstellung erhalten Konfiguration/Zertifikat/Daten | Erfolgreich |
| Wechsel von einem früheren auf ein anderes, neu gebautes Image | Erfolgreich |
| SuperUser-Passwort interaktiv über Standardeingabe setzen | Erfolgreich |
| Fremde TCP- und UDP-Portbelegung wird nicht übernommen | Erfolgreich |
| Parallel laufender nativer Mumble-Prozess bleibt unverändert erreichbar | Erfolgreich |
| Sicherung, Verweigerung bei laufender Instanz, Restore in unabhängige Volumes | Erfolgreich |
| Verweigerung beim Restore über bestehende Daten | Erfolgreich |
| SIGTERM/sauberer Shutdown | Erfolgreich |

Getestet wurde das endgültige lokale Laufzeit-Image
`sha256:e472b8090d332842a508854319aebb6e56d36cc379513ed873ae53e485c418a1`
mit 198862694 Bytes unkomprimierter Größe. Der vollständige Integrationstest lief
**ohne** `--runtime-file`, also ohne eingeblendetes Entwicklungsskript. Der echte
Image-Wechsel verwendete zusätzlich `--previous-image` und prüfte unterschiedliche
Image-IDs bei unverändertem Zugangspasswort, Konfiguration und Zertifikat.
Die Image-ID dokumentiert diesen lokalen Prüfstand; sie ist kein öffentlich
verfügbares Registry-Image. Spätere Paketstände können eine andere ID ergeben.

Der vollständige Integrationstest ist `scripts/docker/tests/integration.py`. Er
verwendet eindeutig benannte Wegwerf-Projekte und entfernt am Ende ausschließlich
deren Testcontainer/-Volumes. Diese gezielte Testbereinigung ist kein Update- oder
Löschverfahren für Benutzerinstallationen. Die dokumentierten Updates verwenden
keinen Volume-Löschbefehl.

Die Tests legen eigene Konten-, Kanal- und ACL-Datensätze sowie eine zusätzliche
Testtabelle in einer gestoppten Testdatenbank an. Die maßgeblichen Tabellen und das
Zertifikat werden vor/nach dem Neuerstellen und der Wiederherstellung verglichen.
Laufzeit-Logzeilen sind keine unveränderlichen Benutzerdaten und werden beim
Tabellenvergleich getrennt behandelt. Ein zukünftiger Audio-Verzeichniseintrag
wird ebenfalls mitgesichert und wiederhergestellt.

Ein erneuter Build hat den bereits bekannten sporadischen v0.2-UDP-Testfehler in
`TestSimplexIntegration` und `TestVoiceDiagnostics` gezeigt; die Builds brachen
korrekt ab. Für die abschließende Docker-Prüfung wird ausschließlich in generierten
Kopien der jeweiligen Testhilfen ein
gültiger, kontrollierter Nonce-Bereich per normalem Mumble-Resync eingestellt.
Ungültige fremde Datagramme beenden die Ping-Warteschleife nicht vorzeitig; der
passende authentifizierte Ping bleibt zwingend. **Keine bestehende Sprach-/Routing-
Assertion wurde entfernt oder gelockert**, kein CTest-Programm wird übersprungen,
kein Fehler wird automatisch in einen Erfolg umgewandelt. Die bekannte Server-
Fehlerstelle bleibt gemäß Branchtrennung unangetastet.

Die Protokollprobe verwendet Mumble-Audio-Framing mit Testnutzdaten; sie ist kein
hörbarer Audiotest. Die bekannten v0.2-UDP-Grenzfälle werden nicht als Docker-
Änderung repariert oder aus dem Audio-Branch übernommen.

## Noch auf einem echten Host zu prüfen

- Docker-/Host-Neustart und die tatsächliche Neustartrichtlinie des Hosts.
- Verbindungen und hörbare Sprache mit unverändertem Mumla und Windows-Mumble
  durch deine konkrete Proxmox-/Host-Firewall und gegebenenfalls Router/NAT.
- Langzeittest, große Gruppen, reale Netzwerkverluste und Ausfallszenarien.
- Andere CPU-Architekturen, Windows-/macOS-Docker-Hosts, exakt begrenzte 4-GiB-VM.

Die Cloud verwendet einen echten lokalen Docker-Daemon, echtes Compose, echte
Images/Volumes und Serverprozesse; kein systemd-/Docker-Testdouble. Nur eine
zusätzliche Build-CA wird für den Cloud-Proxy als temporäres BuildKit-Secret
bereitgestellt. Diese CA gelangt nicht ins Laufzeit-Image, TLS-Prüfung bleibt an.
Normale Benutzer benötigen diese Cloud-Ergänzung nicht.

## Git und Lizenzen

Nachvollziehbare Commits trennen Docker-Grundgerüst, Verbesserungen für echte
Volumes/Updates sowie die abschließende Testhilfe und Dokumentation.
Die bisherigen Branches und der GitHub-Standardbranch bleiben unverändert.
Es wird kein öffentliches Container-Image und kein Release-Tag erstellt.
Projekt-BSD, Copyright- und bestehende Drittanbieter-Hinweise werden erhalten und
im Image mitgeliefert. Ubuntu und seine Pakete behalten ihre Lizenzdateien.
Die zusätzlichen Verwaltungsskripte verwenden ausschließlich die Python-
Standardbibliothek und normale Docker-/Bash-Werkzeuge; keine neue ungeklärte
Drittanbieter- oder Audioressource.
