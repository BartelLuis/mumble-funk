# Entwicklung und Branches: Vorbereitung von dev und stable

Stand der Bestandsaufnahme: **18. Oktober 2026**. Diese Dokumentation beschreibt
vorhandene Technik und einen noch freizugebenden Entwicklungsablauf. **Es gibt
noch keinen `stable`-Branch und keine neue Stable-Veröffentlichung.** `dev` ist
zunächst die bisherige Simplex-v0.2-Basis mit dieser Planungsdokumentation.

Weiterlesen: [Release- und Installationsplan](release-plan.md) ·
[GitHub-Schutz und Zusammenarbeit](github-collaboration.md).

## 1. Vorhandener Stand

Die GitHub-Referenzen wurden unmittelbar vor der Vorbereitung abgefragt. Die
bekannten Referenzpunkte haben sich nicht verändert:

| Branch | Vollständige Commit-ID | Aufgabe |
| --- | --- | --- |
| `master` | `9ef78aac077e3e1555b0aecf640c941ef1f093f5` | Ursprüngliche Mumble-Basis; weiterhin Standardbranch |
| `funk-v0.1` | `95ac344838c1535f943dccc7d134a280aac6a340` | Simplex v0.2 und native Ubuntu-Schnellinstallation |
| `feature/audio-feedback` | `8b295e0cd889933951f7c2e941ad914a5a7c5269` | Experimentelle Audioentwicklung v0.3 |
| `feature/docker-install` | `4e30c9aa189a95171a51a1a15d9931a9fab37049` | Docker-Verpackung von Simplex v0.2; externer Praxistest läuft |
| `weblate-translation-update` | `a6f6cc98e8bd5972433af5ccbc09764a1888502e` | Separat erhaltene Übersetzungsänderungen |

Der neue Branch `dev` wird bei
`95ac344838c1535f943dccc7d134a280aac6a340` abgezweigt. Die folgenden
Dokumentationscommits ändern weder Servercode noch Installer oder Workflows.
Die Basis stammt vom offiziellen Mumble-Tag `v1.5.939` ab und entspricht dem
annotierten Projekt-Tag `funk-v0.2.0`. Dieses Tag markiert einen experimentellen
v0.2-Stand und wird durch den neuen Branch nicht zu einer Stable-Freigabe.

Beide Feature-Branches haben denselben gemeinsamen Ausgangscommit wie `dev`.
Der Vergleich von `funk-v0.1` und dem Docker-Branch zeigt **keinen Unterschied**
unter `src/`, `scripts/funk/`, `.gitmodules` und `LICENSE`. Es gibt keine eigene
Docker-Simplex-Engine. Der Audio-Branch enthält dagegen tatsächliche Server- und
Buildänderungen und wird jetzt ausdrücklich nicht übernommen.

Die 32 Tags unter `archive/mumble/…` und alle bisherigen Versionstags bleiben
erhalten. Es werden keine bisherigen Entwicklungsbranches umbenannt oder ersetzt.

## 2. Zielstruktur und Bedeutung

| Bereich | Bedeutung | Stand dieser Phase |
| --- | --- | --- |
| `stable` | Geprüfter, vom Inhaber freigegebener gemeinsamer Stand für Ubuntu und Docker | Nur geplant; noch nicht angelegt |
| `dev` | Gemeinsame Grundlage für kommende Entwicklung; keine Installationsfreigabe | Neu aus der v0.2-Basis angelegt; zunächst nur Dokumentation |
| `feature/*` | Abgegrenzte Änderungen, die über einen Pull Request vorgeschlagen werden | Bestehende Docker-/Audioarbeit bleibt getrennt |
| `funk-v0.1` | Erhaltene v0.2-Referenz und derzeitiger nativer Installationsweg | Unverändert |
| `master` | Historische Mumble-Basis | Unverändert; Standardbranch bis zu separater Freigabe |

Die künftige Richtung ist `feature/*` → `dev` → freigegebener Kandidat → `stable`.
Ein erfolgreicher Build allein macht aus `dev` keine Stable-Version. Auch ein
Merge in `dev` installiert oder aktualisiert keinen Server.

## 3. Arbeit an neuen Funktionen

1. Den aktuellen `dev`-Stand prüfen und einen eigenen `feature/<thema>`-Branch
   davon erstellen. Neue Quellverzeichnisse dürfen bestehende Installationen nicht
   ersetzen. Die bestehenden Audio-/Docker-Branches bleiben vorerst die vereinbarten
   Ausgangspunkte für deren laufende Tests.
2. Eine überschaubare Änderung entwickeln und dabei passenden Testnachweis und
   Bedienungsdokumentation ergänzen. Verwendete Basis- und Test-Commit-ID festhalten.
3. Einen Pull Request mit Ziel **`dev`** öffnen. Problem, Änderung, ausgeführte
   Prüfungen und offene Grenzen beschreiben. Einen Reviewer ausdrücklich anfordern.
4. Prüfer bewerten Code und Tests; Fehler werden im Feature-Branch korrigiert.
   Ein PR ersetzt keine praktische Freigabe des Projektinhabers.
5. Nach bestandenen Prüfungen kontrolliert in `dev` übernehmen. Anschließend den
   gemeinsamen Integrationsstand prüfen; Erfolge auf Einzelbranches reichen nicht.

Dies ist die künftige Arbeitsvereinbarung. Automatische Prüfpflichten und
Schutzregeln sind noch **nicht** eingerichtet. Ein PR darf deshalb derzeit nicht
allein aufgrund einer vermeintlich vorhandenen Sperre als sicher gelten.

## 4. Bestehende Prüfungen und praktische Tests

| Bereich | Vorhandener Nachweis | Einschränkung |
| --- | --- | --- |
| Server/Simplex v0.2 | 18 CTest-Programme laut [Simplex-Prüfbericht](SIMPLEX-STATUS.md) | Pakettests sind kein vollständiger Hör-/Langzeittest |
| Natives Setup | 16 Setup- und 5 Installationstests; Ubuntu 24.04/26.04 laut [Setup-Prüfbericht](SETUP-STATUS.md) | Containerprüfung verwendet teilweise nachgebildetes systemd |
| Docker | Image-Build, 18 CTest-Programme, 11 Hilfsprogrammtests und echte Compose-Integration laut [Docker-Prüfbericht](https://github.com/Shawna7han/mumble-funk/blob/4e30c9aa189a95171a51a1a15d9931a9fab37049/docs/funk/DOCKER-STATUS.md) | Docker nutzt angepasste Testhilfen; externer Geräte-/Hosttest noch nicht abgenommen |
| Audio | Zusätzliche Audio-, WAV- und Integrationstests im separaten [Audio-Prüfbericht](https://github.com/Shawna7han/mumble-funk/blob/8b295e0cd889933951f7c2e941ad914a5a7c5269/docs/funk/AUDIO-FEEDBACK-STATUS.md) | Experimentell; nicht Bestandteil der ersten geplanten Stable-Basis |

Die Zahlen sind dokumentierte Ergebnisse der genannten Stände, **keine in dieser
Dokumentationsphase neu ausgeführten Tests**. Nach Integrationen muss die gesamte
dann vorhandene Suite laufen; die Zahl 18 ist keine Obergrenze zukünftiger Tests.

### Lokaler Server-Build auf dev

Die aktuellen Skripte `scripts/funk/build.sh` und `scripts/funk/setup.sh` verlangen
ausdrücklich den Branch `funk-v0.1`. Sie sind nicht auf `dev`, `stable` oder einen
ausgecheckten Versionstag umgestellt. **Das produktive Setup jetzt nicht auf dev
ausführen und seine Schutzprüfung nicht umgehen.** Die laufende v0.2-Installation
verwendet weiterhin die [bestehende Anleitung](setup.md).

Entwickler können mit eingerichteten Build-Abhängigkeiten in einer eigenen
Entwicklungsumgebung direkt CMake verwenden, ohne etwas zu installieren:

```bash
git submodule update --init --recursive
cmake -S . -B build/dev -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_NUMBER=939 \
  -Dclient=OFF -Dserver=ON -Dtests=ON -Dplugins=OFF -Doverlay=OFF \
  -Dlto=OFF -DCMAKE_UNITY_BUILD=OFF
cmake --build build/dev --parallel 2
ctest --test-dir build/dev --output-on-failure
python3 tests/funk-install/test_setup.py
python3 tests/funk-install/test_install.py build/dev/mumble-server
```

Die Installationstests arbeiten mit temporären Testzielen und Testservern. Einen
belegten Paralleltest können sie auslassen; für eine Release-Freigabe sind solche
Auslassungen ausdrücklich zu prüfen. Es wird hier kein `install.sh` aufgerufen.
Docker-Tests sind auf diesem anfänglichen `dev` noch nicht enthalten; ihre
unveränderte Anleitung bleibt im Docker-Feature-Branch.

## 5. Bewusste Integration der vorhandenen Feature-Branches

**Docker zuerst, nach Abschluss des laufenden Praxistests:** Im eigenen
Integrationsbranch aus aktuellem `dev` den freigegebenen Docker-Commit prüfen und
einen PR nach `dev` vorbereiten. README-Konflikte bewusst lösen. Der derzeitige
gemeinsame Vorfahr ist `95ac3448…`; Docker bringt drei weitere Commits mit.
Servercode und native Skripte müssen bei der reinen Verpackungsübernahme identisch
bleiben. Das ist erneut per Git-Diff und anschließend durch beide Installationswege
zu prüfen. Der [Releaseplan](release-plan.md) benennt zusätzliche notwendige Arbeiten.

**Audio später und getrennt:** Erst nach eigener Freigabe einen Integrationsbranch
von aktuellem `dev` erstellen und den dann aktuellen Audio-Branch per PR prüfen.
Er enthält vier zusätzliche Commits, unter anderem Opus-Abhängigkeiten,
Server-/Simplex-Erweiterungen, WAV-Verwaltung, eine eigene Preview-Installation und
eine UDP-Korrektur. Deshalb ist es keine reine Docker-Verpackungsänderung.
Testhilfe, Lizenzhinweise, CMake, Konfiguration und Preview-Pfade gemeinsam
abgleichen. Insbesondere darf die bisherige Preview-Installation nicht versehentlich
die normale Instanz ersetzen. Es erfolgt kein automatischer Merge und kein Rebase
oder Force-Push des laufenden Feature-Branches.

Wenn der UDP-Fix unabhängig von Audio in eine v0.2-Nachfolgeversion soll, braucht er
einen separaten, geprüften Vorschlag mit Regressionstests. Ihn nicht stillschweigend
aus dem Audio-Branch übernehmen. Andernfalls müssen die bekannten Grenzen vor der
Stable-Freigabe bewertet und dokumentiert werden.

## 6. Wann eine Version Stable werden darf

Der Inhaber muss einen **konkreten Commit** freigeben, der beide Installationswege,
gemeinsamen Servercode, passende Dokumentation und vollständige Testnachweise
enthält. Dazu gehören echte Geräte-/Proxmox-Tests und geprüfte Erhaltung vorhandener
Daten bei Update und Rückkehr. Offene Fehler und unterstützte Plattformen müssen
zum Freigabestand passen. Umfang und Ergebnis eines Langzeittests werden benannt;
es wird keine ungeprüfte Produktionsreife behauptet.

Die noch offenen Voraussetzungen, Versionsentscheidung und spätere Umstellung
der GitHub-Startseite stehen im [Releaseplan](release-plan.md). Alle bisherigen
Installationen bleiben bis zu einer bewussten Aktualisierung unverändert.
