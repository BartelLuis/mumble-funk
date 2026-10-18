# Prüfstand: automatisches Setup

Stand: 18. Oktober 2026. Zweig: `funk-v0.1`, Mumble-Funk Simplex v0.2.
Diese Änderung betrifft Einrichtung, Prüfung und Dokumentation. Simplex-Kernlogik,
Audiocodecs, Protokoll und Clients wurden nicht verändert.

## Implementierter Ablauf

- `scripts/funk/setup.sh`: normaler sudo-Benutzer, Ubuntu-Allowlist 24.04/26.04,
  RAM-/Swap-/Platzprüfung, fehlende Pakete über APT, Submodule, bestehender Build
  einschließlich aller Tests, anschließend bestehender Installer.
- Kein Build als root, kein automatisches Git-Pull, keine Firewalländerung.
- `install.sh --setup`: ausschließlich bei neuer Konfiguration Simplex mit 500/500 ms
  und eigener Diagnose. Der direkte manuelle Installationsweg bleibt opt-in.
- `setup_support.py`: sichere Vorprüfung, vorhandene Konfiguration und lokale
  SQLite-Overrides lesen, Port-/Dienstzuordnung, atomare neue Konfiguration,
  Startprüfung und installierter lokaler Befehl `sudo mumble-funk-info`.
- Vorhandene Konfiguration wird nicht neu geschrieben. Abweichende Portwünsche,
  fremde Datenbankpfade, mehrdeutige Einstellungen und unbekannte Portbesitzer
  werden abgewiesen. Auch gebundene, noch nicht lauschende Sockets werden geprüft.
- Passwort: 16 zufällige Bytes / 32 Hexadezimalzeichen; kein Standardpasswort,
  keine Ausgabe im Setup-Protokoll. Abruf nur lokal mit sudo.
- Installation prüft aktive und aktivierte eigene Unit, MainPID, beide TCP-/UDP-
  Listener und **neue** Startmeldungen. Ein alter Logeintrag reicht nicht.
- Sicherung vor Updates umfasst den gesamten eigenen Datenbankordner und die
  Konfiguration, auch bei benutzerdefiniertem SQLite-Dateinamen.
- Die Programmordner erhalten explizit 0755, damit der Dienstbenutzer auch bei
  strenger `umask 077` zum Programm gelangen kann. Konfiguration bleibt 0640,
  Konfigurations-/Daten-/Logverzeichnisse 0750 mit passenden Besitzern/Gruppen.

## Automatisierte Sicherheits- und Fehlerfalltests

`tests/funk-install/test_setup.py` enthält **16** Testmethoden. Davon prüfen die
Orchestrierungstests eine temporäre Skriptkopie mit nachgebildeten Befehlen für
APT, Git, CMake, CTest und sudo. Sie installieren nichts auf dem Host.
Dateierstellung, Parser, Dateirechte und SQLite-Lesezugriffe werden tatsächlich
ausgeführt; ein Socket-Test verwendet echte lokale Sockets.

Geprüft werden unter anderem:

- Neue Konfiguration, zufälliges Passwort, 500/500 ms und getrennte Diagnose.
- Wiederholung, benutzerdefinierte Konfiguration, Passwort- und Porterhaltung.
- SQLite-Overrides ohne Schreibzugriff; Abweisung fremder Datenbankpfade.
- Fehlende Konfiguration bei vorhandenen Daten: kein neues Passwort erzeugen.
- Belegte TCP-/UDP-Ports, gebundene Sockets, fremde Dienste und fehlende PID-Zuordnung.
- Ungültige Ports, nicht unterstütztes OS, unzureichender Speicher.
- APT-Update-/Installationsfehler, fehlendes Netzwerk für Submodule, Build- und Testfehler:
  keine Bereitstellung, keine Erfolgsmeldung.
- Neue statt historischer Startmeldungen; beide Transport-Listener erforderlich.
- Lokaler Passwortabruf; Passwort in der Setup-Zusammenfassung verborgen.
- Programmpfade trotz restriktiver Dateivorgaben zugänglich.

Die bestehenden **fünf** Installationstests prüfen zusätzlich Wiederholung,
vorherige Programmversion, falsche Versionen, Schutz nachgebildeter Standarddateien,
OS-Allowlist und zwei reale parallel laufende Serverprozesse.

## Tatsächlich getestete Ubuntu-Umgebungen

Die Prüfung begann in frischen Docker-Basisumgebungen für **Ubuntu 24.04 und
Ubuntu 26.04 (amd64)**. Das automatische Setup installierte die fehlenden
Build-Abhängigkeiten über die echten Ubuntu-Paketquellen und kompilierte den
Server sowie alle Tests als Benutzer `setup-tester`, nicht als root.

Beide vollständigen Neubauten bestanden alle **18/18 Servertestprogramme**.
Die anschließende Prozesszuordnung benötigte im isolierten Docker-PID-Namensraum
zusätzlich `SYS_PTRACE`, weil Docker sonst Socket-Besitzer anderer Benutzer verbirgt.
Der Installer wurde dafür nicht abgeschwächt. Für weitere Prüfungen wurden die
bereits gebauten Umgebungen mit dieser Leseberechtigung fortgesetzt; die vorhandenen
Build-Ergebnisse wurden weiterverwendet.

Abschließender Stand auf beiden Ubuntu-Versionen:

| Prüfung | Ubuntu 24.04 | Ubuntu 26.04 |
| --- | --- | --- |
| Vollständiger Neubau als normaler Benutzer | erfolgreich | erfolgreich |
| Abschließendes Setup und Wiederholung/Update | erfolgreich | erfolgreich |
| Alle Servertestprogramme, jeweils auch beim Update | 18/18 | 18/18 |
| Neue Setup-Sicherheitstests | 16/16 | 16/16 |
| Bestehende Installationstests | 5/5 | 5/5 |
| Tests übersprungen | keine | keine |
| Echte fremde TCP-/UDP-Belegung auf 64739 abgewiesen, Besitzer nicht beendet | erfolgreich | erfolgreich |
| Neue Konfiguration: Simplex, 500/500 ms, zufälliges Passwort | erfolgreich | erfolgreich |
| Echter Serverstart als Dienstbenutzer; Listener und neue Simplex-Meldungen | erfolgreich | erfolgreich |
| Eigene Konfiguration und anderer SQLite-Dateiname beim Update erhalten und gesichert | erfolgreich | erfolgreich |
| Passwort sowie gespeichertes Zertifikat/Schlüsselpaar erhalten | erfolgreich | erfolgreich |
| Zweiter Standardmodus-Server auf 64738 während Update unverändert erreichbar | erfolgreich | erfolgreich |
| ShellCheck und Shell-Syntax | erfolgreich | erfolgreich |
| Echte Unit-Prüfung mit systemd-analyze und Logrotate im Prüfmodus | erfolgreich | erfolgreich |

Die neue Orchestrierung wurde zusätzlich in der bestehenden Debian-Cloud-Umgebung
geprüft (16/16 Tests). Nach der Testkorrektur bestand dort auch der tatsächlich
neu kompilierte Integrationstest erneut. Es wurden keine Server-Tests absichtlich
ausgelassen oder Fehler durch automatische Wiederholung bis zum Erfolg verdeckt:
Nach beobachteten Fehlern wurden deren Ursachen untersucht und die beschriebenen
Korrekturen vorgenommen, danach die betroffenen Abläufe erneut ausgeführt.

Der echte Belegungs- und Erreichbarkeitstest verwendet den Standardport 64739.
Die alternative Portwahl ist zusätzlich in den automatisierten Konfigurations-
und Ablaufprüfungen mit 64740 getestet.

## Korrekturen aus der Prüfung

Die Ubuntu-Prüfung deckte einen Installerfehler auf: Bei strenger Umask konnten
Zwischenverzeichnisse des Programmpfads nur für root zugänglich werden. Explizite
Rechte für Basis-, Release- und Versionsverzeichnis beheben das; ein eigener
Regressionstest prüft es.

Ein Bestands-Integrationstest erwartete unter Last sämtliche Diagnoseereignisse
sofort und vollständig. Das widersprach der ausdrücklich verlustbehafteten,
asynchronen Diagnose. Der Test wartet jetzt auf deren geordneten Abschluss und
berücksichtigt gemeldete Verluste. Alle bisherigen Assertions zur tatsächlich
weitergeleiteten Sprache bleiben erhalten. Zusätzlich synchronisieren UDP-/TCP-Pings
die konkurrierenden Testpakete vor den Ende-Markierungen; positive Empfangsprüfungen
warten bei Scheduler-Verzögerungen bis zu zwei Sekunden statt starr nur 80–100 ms.
Die Prüfung auf unerlaubte zusätzliche Pakete bleibt bestehen. Die Serverfunktion
wurde nicht geändert.

Der Standardmodus-Test wartet auf die tatsächlich gespeicherte Schlüsselpaar- und
Zertifikatskonfiguration, bevor er deren Erhaltung prüft; ein bloß gebundener Socket
war dafür zu früh.

## Grenzen der Nachweise

- APT, Compiler, Testprogramme, Dateianlage, Dienstbenutzer und Serverprozesse in
  Ubuntu wurden tatsächlich ausgeführt.
- `systemctl` wird im Container durch `mock_systemctl.py` ersetzt. Dieser startet
  den echten Server unter dem Dienstbenutzer, ersetzt aber **keinen echten
  systemd-Boot-, Neustart- oder Supervisionstest**. Unit und Logrotate-Regel werden
  zusätzlich mit den echten Prüfwerkzeugen untersucht.
- Der parallel betriebene Standardmodus-Testserver ist derselbe gebaute Fork mit
  `simplex=false`, nicht ein zusätzlich installiertes Ubuntu-Mumble-Paket.
- Paket-/Build-/Netzwerkfehler und weitere Grenzfälle sind gezielt simuliert;
  nicht jede mögliche Störung wurde auf echten Ubuntu-Servern ausgelöst.
- Kein Zugriff und keine Installation auf dem privaten Proxmox-/Ubuntu-Server.
- Kein neuer Endgeräte-, Internet-Erreichbarkeits- oder Langzeittest durch diese
  Installationsprüfung. Der Betreiber berichtet separat erfolgreiche erste
  Simplex-Gerätetests mit Android-Mumla und Windows-Mumble auf Ubuntu 26.04.1.

## Relevante Dateien

| Bereich | Dateien |
| --- | --- |
| Setup und Bereitstellung | `scripts/funk/setup.sh`, `setup_support.py`, `install.sh`, `ubuntu/mumble-funk.service` |
| Tests | `tests/funk-install/test_setup.py`, `test_install.py`, `mock_systemctl.py`, `standard_mode_server.py`, `ubuntu_container_check.sh`, `src/tests/TestSimplexIntegration/{Harness.h,TestSimplexIntegration.cpp}` |
| Bedienung und Darstellung | `README.md`, `README.de.md`, `docs/funk/setup.md`, `installation-ubuntu.md`, `simplex.md`, `tests/funk-install/README.md`, diese Statusdatei |

Der Funktionszwischenstand ist in `6e1c7b205` gesichert, die Korrekturen und
abschließenden Testverbesserungen in `7395f3e9d`. Der nachfolgende
Dokumentationscommit enthält diesen Programm-/Teststand unverändert.
Der aktuelle Gesamtstand ist mit `git log -1 --oneline` zu ermitteln.
