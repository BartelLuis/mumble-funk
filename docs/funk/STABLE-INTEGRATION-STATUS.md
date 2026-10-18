# Prüfstand: gemeinsamer Stable-Kandidat

Stand: 18. Oktober 2026. Versionsvorschlag **funk-v0.2.1**, Mumble-Basis **1.5.939**,
Simplex-Engine weiterhin **v0.2**. Noch kein neuer Versionstag, Release, Registry-
Publish oder endgültige Produktionsfreigabe. Kein Zugriff auf laufende Zielserver.

## Quellen und Umfang

| Ausgangsbranch | Vollständige Commit-ID |
| --- | --- |
| dev | ddee0fa5a6afaf026ac1f10fcc0279f6b8bd28b2 |
| funk-v0.1 | 95ac344838c1535f943dccc7d134a280aac6a340 |
| feature/docker-install | 4e30c9aa189a95171a51a1a15d9931a9fab37049 |
| feature/audio-feedback | 8b295e0cd889933951f7c2e941ad914a5a7c5269 |
| master | 9ef78aac077e3e1555b0aecf640c941ef1f093f5 |
| weblate-translation-update | a6f6cc98e8bd5972433af5ccbc09764a1888502e |

Integration: `integration/stable-v0.2`, von aktuellem `dev`, mit echtem Merge des
Docker-Branches. Übernahme nach `dev` per PR; `stable` wird erst nach erfolgreichen
verfügbaren Prüfungen aus dem geprüften Stand erstellt. Die genaue veröffentlichte
Commit-ID steht im zugehörigen PR/Abschlussbericht und ist mit
`git ls-remote origin refs/heads/stable` überprüfbar. Ein Branchname ist beweglich;
für Reproduzierbarkeit die vollständige ID aufbewahren.

Ubuntu und Docker bauen denselben Serverquellcode. Mumble-Version und Binary-
Dateihash allein sind keine Funk-Release-ID. Keine Änderungen an SimplexController,
Clients, Protokoll, Audio-Codecs oder Audio-Feedback; keine neuen Audioressourcen
oder Drittanbieter-Bibliotheken. Lizenzdateien und Copyright-Hinweise bleiben.
Compose-Projekt-/Dienst-/Volume-Namen und Portabbildung bleiben unverändert.
Neuinstallation bei beiden Wegen: Simplex an, Timeout/Retry **500/500 ms**;
vorhandene Einstellungen bleiben erhalten. Der C++-Opt-in-Standard bleibt aus.

## Gezielte UDP-Korrektur

Der neue deterministische Regressionstest erzeugt Nonce-Byte 1 über regulären
CryptSetup-Resync. Vor der Korrektur scheitern **4/8 Kombinationen**: ältere
Paketformate, jeweils bei unbekanntem/bekanntem UDP-Peer, mit/ohne Simplex.
Eine unverschlüsselte Ping-Probe interpretiert das Ciphertext-Byte als neue
Protokollversion. Nach erfolgreicher Entschlüsselung muss der Decoder wieder die
bekannte Version des authentifizierten Benutzers verwenden. Genau dies ergänzt
der Server-Fix (eine Anweisung plus Erklärung), ohne Simplex-Regeln zu ändern.
Danach bestehen alle acht Kombinationen.

Die Docker-CMake-Testadapter mit absichtlich vermiedenem Nonce-Bereich sind
vollständig entfernt. Beide Builds verwenden dieselben Testquellen. Die gemeinsame
Testhilfe verlangt einen passenden authentifizierten Ping und verwirft fremde/
ungültige Datagramme während des Wartens. Ein zusätzlicher Regressionstest speist
ein ungültiges Datagramm ein und verlangt anschließend die echte Ping-Antwort.
Keine Routing-Assertion wird entfernt und kein Test wird in Erfolg umgewandelt.

## Bisher in der Integration erneut ausgeführt

| Prüfung | Ergebnis |
| --- | --- |
| Nativer Server-only-Build (Cloud, GCC 14 / Qt 5.15) | Erfolgreich |
| Vollständige CTest-Suite einschließlich Simplex und Diagnose | 18/18 Testprogramme, keine Auslassung |
| Native Setup-Orchestrierung und Datenerhalt | 16/16 Tests |
| Native Installation, OS-Prüfung, Parallelserver | 5/5 Tests |
| Neue echte Git-Quellstand-/Tag-/Worktree-Prüfungen | 14/14 Tests, nur lokale Wegwerf-Repositories |
| Docker-Runtime-Hilfsprogramme | 11/11 Tests |
| Echter Multi-Stage-Docker-Build mit vollständiger CTest-Suite | Erfolgreich; ohne Nonce-Testadapter |
| Compose auf Host 64740 TCP/UDP, intern 64739 | Erfolgreich |
| Authentifizierte Sprache über TCP und verschlüsseltes UDP, Simplex/Konkurrenz | Erfolgreich mit Protokoll-Testclients |
| Diagnose aus, Neustart/Neuerstellung, belegte Ports, nativer Parallelserver | Erfolgreich |
| Backup/Restore, voller SQLite-Inhalt inklusive Benutzer/Kanäle/ACLs | Erfolgreich |
| Echtes Update vom vorherigen Docker-Image und Rollback mit denselben Volumes | Erfolgreich; Daten, Passwörter, Zertifikat und Konfiguration erhalten |

Die finalen Commit- und Wiederholungsnachweise werden nach Abschluss der Integration
im PR festgehalten. Native Ubuntu-Containerprüfungen und finale Wiederholungen
werden gesondert ergänzt; frühere Einzelbranch-Ergebnisse ersetzen sie nicht.

## Noch keine abgeschlossenen Praxistests dieses Kandidaten

- Ziel-VM: Ubuntu 24.04/26.04 mit echtem systemd-Boot, Dienstüberwachung und
  tatsächlichem VM-/Host-Neustart. Container können das nicht vollständig ersetzen.
- Abschließende Docker-Umstellung des Kollegen, reale Clients mit hörbarer Sprache,
  seine Firewall/NAT und sein Docker-/Host-Neustart.
- Langzeit-/Lastbetrieb, reale Paketverluste und andere Architekturen.

Der Nutzer berichtet bereits einen erfolgreichen praktischen Docker-Test des alten
Feature-Branches. Das wird nicht als Praxistest des neuen Integrationscommits gezählt.
Protokoll-Testnutzdaten sind kein Hörtest. `healthy` belegt keine externe Verbindung.

## Workflows und Schutz

Upstream-Publishing für fremdes Mumble-Docker und WinGet sowie Client-Builds,
automatische Backports und automatische Issue-Schließung sind im Integrationsstand
entfernt. Server-Build-, Installer- und serverbezogene CodeQL-Prüfungen bleiben.
Kein Registry-Schreibrecht, kein Tag-/Release-Publish-Job, kein Zielserver-Deployment.
GitHub-Actions-Laufergebnisse sind getrennt von lokalen Ergebnissen zu betrachten.
Die bestehenden geschützten Quellbranches und alle Tags bleiben unverändert;
`master` bleibt GitHub-Standardbranch. Schutzregeln werden nicht neu eingerichtet.

[Ubuntu-Setup](setup.md) · [Docker](docker.md) ·
[Sichere Docker-Umstellung](docker-stable-migration.md) · [Freigabeplan](release-plan.md)
