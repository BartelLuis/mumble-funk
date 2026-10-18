# Releaseplan: gemeinsamer Stand für Ubuntu und Docker

**Vorbereitung, keine fertige Veröffentlichungsfunktion.** Stand: 18. Oktober 2026.
Es werden in dieser Phase weder `stable` noch Releases, Images, neue Versionstags,
Installeränderungen, automatische Updates oder Serverinstallationen angelegt.

[Entwicklungsablauf](development.md) · [Schutz und Zusammenarbeit](github-collaboration.md)

## 1. Was zusammengeführt werden soll

Die erste geplante Stable-Basis enthält Mumble Server 1.5.939, Simplex-Engine v0.2,
native Ubuntu-Schnellinstallation, Docker-Installation und passende Dokumentation.
Audio-Feedback v0.3 bleibt ausdrücklich außerhalb dieser ersten Integration.

| Bereich | Bereits vorhanden | Noch vorzubereiten |
| --- | --- | --- |
| Servercode | Auf `funk-v0.1` und Docker-Branch identisch | Ein freigegebener gemeinsamer Commit |
| Ubuntu | Setup, getrennte systemd-Instanz, Sicherung und Datenbestandsschutz | Stable-/Versionsauswahl statt fester Branchbindung |
| Docker | Lokaler Multi-Stage-Build, sichere Erstkonfiguration, Volumes und Offline-Backup | Abgenommener Praxistest, gemeinsame Integration und Registry-Betriebsweg |
| Versionen | Annotiertes `funk-v0.2.0` auf `95ac344838c1535f943dccc7d134a280aac6a340` | Neue Versionsentscheidung für den erweiterten Integrationsstand |
| CI | Übernommene Mumble-Workflows, bisher keine Actions-Läufe bei der Abfrage | Eigene Server-, Installations- und Veröffentlichungsjobs |

### Verbindliche Identität einer Version

Ein Versionsmanifest soll später Projektversion, vollständige Git-Commit-ID,
Mumble-Basis, Submodulstände, freigegebene Plattformen, Testergebnisse, native
Artefaktprüfsummen und Container-Digest zusammenführen. Ubuntu und Docker müssen
aus **derselben freigegebenen Quellrevision** gebaut werden. Verschiedene Compiler
und Betriebssystembibliotheken bedeuten nicht zwingend bytegleiche Binärdateien.

Die Anzeige `Mumble 1.5.939` genügt nicht, um eine bestimmte Funk-Version zu
identifizieren: Der bisherige Installer prüft diese Basisversion und verwendet
einen Binärdatei-Hash als Installationsverzeichnis, aber noch kein gemeinsames
Funk-Release-Manifest. Das ist spätere Integrationsarbeit.

**Bestehendes Tag nicht umdeuten:** `funk-v0.2.0` enthält noch keine Docker-Dateien.
Ein späterer gemeinsamer Commit darf dieses Tag nicht ersetzen. Vorschlag für die
erste gemeinsame Verpackung: eine neue Version wie `funk-v0.2.1`, nach ausdrücklicher
Entscheidung des Inhabers. Die Engine kann dabei weiterhin v0.2 heißen. Ein Image
`:0.2.0` aus einem anderen Commit als dem bisherigen Tag wäre irreführend.

## 2. Integration in getrennten Schritten

1. **Laufenden Docker-Praxistest abnehmen.** Tester nennt exakten Commit, Host,
   Clientversionen, verwendete Ports, Ergebnisse und verbleibende Einschränkungen.
   Gegenwärtiger Ausgangscommit ist `4e30c9aa189a95171a51a1a15d9931a9fab37049`.
2. **Docker-Verpackung per PR in dev integrieren.** Einen neuen Integrationsbranch
   verwenden und den laufenden Testbranch unverändert lassen. Die ursprünglichen
   Docker-Dateien, Tests und Dokumente sind im [Docker-Branch](https://github.com/Shawna7han/mumble-funk/tree/feature/docker-install)
   vorhanden, noch nicht im anfänglichen `dev`.
3. **Beide Installationswege versionsfähig machen.** Native Branchprüfungen und
   Docker-Imageauswahl erst in gesonderten, getesteten Änderungen anpassen.
   Aktuell ist Compose auf lokalen Build und `:local` ausgelegt; ein einfaches
   `docker compose pull` ist deshalb noch kein zukünftiger Stable-Updateweg.
4. **Einheitliche Prüfungen und Release-Unterlagen ergänzen.** Gemeinsame Revision,
   Konfigurationsvergleich, Testhilfe und bekannte UDP-Grenzen prüfen. Native und
   Docker-Konfiguration dürfen sich bei Prozess-/Dateipfaden unterscheiden, sollen
   aber dieselben freigegebenen Simplex-Werte für Neuinstallationen verwenden.
   Bestehende benutzerdefinierte Werte niemals beim Update auf Standard zurücksetzen.
5. **Unveränderlichen Kandidaten praktisch testen.** Nach jeder relevanten Korrektur
   den neuen gemeinsamen Commit erneut beurteilen. Keine Freigabe allein anhand
   früherer Einzelbranch-Ergebnisse.
6. **Inhaber gibt konkreten Commit frei.** Erst danach `stable` aus genau diesem
   geprüften Stand anlegen. Einen weitergelaufenen `dev`-HEAD nicht blind übernehmen.

### Bekannter Unterschied bei der Testausführung

Der Docker-Build erzeugt derzeit kontrollierte Kopien zweier Testhilfen über
`scripts/docker/tests/{register.cmake,CMakeLists.txt}`. Damit werden zufällige
Nonce-/Ping-Grenzfälle des unveränderten v0.2-Servers in der Verpackungsprüfung
vermieden; Routing-Assertions bleiben bestehen. Der native Build benutzt diese
Docker-Ergänzung nicht. Das ist kein abweichender Servercode, aber ein Unterschied
in der Testabdeckung.

Vor Stable muss entschieden werden: separat geprüfte UDP-Korrektur übernehmen
oder den bekannten Fehler mit nachvollziehbarer Risikobewertung offen dokumentieren.
Auch künftig die echten Grenzfälle testen; kontrollierte Nonces sind kein Beweis,
dass der Fehler im Server behoben ist. Wird der Fix später übernommen, Testadapter
gezielt entfernen/vereinheitlichen und sämtliche Prüfungen erneut ausführen.

## 3. Noch offene Freigabenachweise

- [ ] Docker-Praxistest des zweiten Entwicklers mit hörbarer Sprache über UDP und
  TCP-Tunnel, unverändertem Mumla und Windows-Mumble abgenommen.
- [ ] PTT-Konkurrenz, Ende-/Timeout-Verhalten, schneller Wechsel, Kanalwechsel,
  Disconnect und Netzwerkstörungen am konkreten Kandidaten beurteilt.
- [ ] Native Ubuntu-24.04-/26.04- und Docker-Neuinstallation sowie Update getestet;
  reale systemd-/Host-Neustarts von bisherigen Container-Simulationen unterschieden.
- [ ] Konfiguration, Passwort, SQLite, Zertifikat, Konten, Kanäle und ACLs bleiben
  erhalten; Sicherung und Wiederherstellung auf getrennten Testzielen nachgewiesen.
- [ ] Beide Installationswege funktionieren parallel zu vorhandenen Servern;
  Ports/Volumes/Dateipfade bleiben getrennt. Docker-/Host-Neustart und externe
  Firewall/NAT sind geprüft. `healthy` allein belegt keine hörbare Funkverbindung.
- [ ] Ressourcen- und Architekturumfang festgelegt. Bisherige amd64-Ergebnisse
  nicht als ARM-Nachweis oder exakten 4-GiB-Limit-Test darstellen.
- [ ] Langzeittest mit Dauer, Last, Störungen und Ergebnis dokumentiert; bekannte
  Fehler und Freigabeentscheidung benannt. Kein Abschluss wird hier behauptet.
- [ ] CI-, Schutz-, Versions-, Lizenz- und Veröffentlichungsprüfung abgeschlossen.
- [ ] Release-Notizen, beide Installationsanleitungen und Wiederherstellungsweg
  beziehen sich auf denselben freigegebenen Commit.

Die bisher dokumentierten automatisierten Erfolge stehen in
[SETUP-STATUS.md](SETUP-STATUS.md) und im
[Docker-Prüfbericht](https://github.com/Shawna7han/mumble-funk/blob/4e30c9aa189a95171a51a1a15d9931a9fab37049/docs/funk/DOCKER-STATUS.md).
Diese Liste ist kein neu ausgeführter Test und keine Bestätigung einer fertigen
Stable-Version.

## 4. Künftige GitHub Actions und GHCR

Die folgenden Workflow-Namen sind **Vorschläge**, keine vorhandenen Dateien:

| Geplanter Workflow | Auslöser | Aufgabe / Rechte |
| --- | --- | --- |
| `funk-ci.yml` | PRs nach `dev`/später `stable`, Push auf Integrationsbranches | Server-only-Build, komplette Tests, native Installationsprüfungen, Docker-Build und Compose-Integration; kein Registry-Schreibrecht |
| `funk-release.yml` | Später freigegebenes neues Versionstag | Commit/Version prüfen, alle Tests für den Release-Commit ausführen, Artefakte bereitstellen; Veröffentlichung erst nach Inhaberfreigabe |

CI-Abdeckung: Ubuntu 24.04 und 26.04, Shell-/Python-Prüfungen, vollständige
CTest-Suite und aktuelle Setup-/Installationstests. Docker prüft `compose config`,
Image-Build, nicht privilegierten Start, TCP/verschlüsseltes UDP, Simplex ohne
Diagnose, Datenbestandsschutz, Backup/Restore und Image-Update. Die Protokollprobe
und der native Referenzserver müssen als Testwerkzeuge im CI-Build verfügbar
sein; sie gehören nicht ins schlanke Laufzeit-Image. Tests verwenden ausschließlich
eigene Wegwerf-Projekte und ändern keine laufenden Proxmox-Instanzen.

### Geplanter Veröffentlichungsablauf

1. Release-Tag auflösen und prüfen, dass seine vollständige Commit-ID dem
   freigegebenen Stable-Stand und Versionsmanifest entspricht. Nicht nur den
   beliebig gewählten Namen eines Tags akzeptieren.
2. Genau diese Revision mit ihren Submodulständen bauen und testen. Das getestete
   Image als OCI-Artefakt/Digest an den Freigabejob übergeben. Nach erfolgreichem
   Test nicht unbemerkt ein anderes Image unter gleicher Version neu bauen.
3. Veröffentlichung über eine später einzurichtende Umgebung `stable-release`
   durch den Inhaber freigeben. Erst der Publish-Job erhält `packages: write`,
   möglichst mit `GITHUB_TOKEN`, und keine PR aus fremdem Code erhält diese Rechte.
4. Image etwa als `ghcr.io/shawna7han/mumble-funk:0.2.1` und zusätzlich mit
   `sha-<commit>` kennzeichnen; OCI-Labels für Source, Version, Revision und Lizenz
   setzen. Feste Versionen/Digests gemäß Veröffentlichungsrichtlinie nicht ersetzen.
   Derselbe Git-Commit allein garantiert bei veränderten Paketquellen keinen
   identischen Image-Digest; Neubauten benötigen eine nachvollziehbare neue Version.
5. Erst bei vollständigem Erfolg den beweglichen Kanal
   `ghcr.io/shawna7han/mumble-funk:stable` auf dieses Image setzen. Vorherige feste
   Versionen für Rollbacks von automatischen Bereinigungen ausnehmen.
6. Paket mit dem Repository verknüpfen, Sichtbarkeit separat prüfen und nach
   bewusster Freigabe anonymen Pull des öffentlichen Images testen. Ein öffentliches
   Git-Repository macht ein neues GHCR-Paket nicht automatisch öffentlich.

Dies veröffentlicht Artefakte, **keine Installation**. Kein Watchtower, kein
automatischer SSH-/Proxmox-Deploy und kein automatischer Neustart eines Benutzerservers.

Die GHCR-Authentifizierung und Paketzuordnung sind in der offiziellen
[Container-Registry-Dokumentation](https://docs.github.com/en/packages/working-with-a-github-packages-registry/working-with-the-container-registry)
beschrieben. Die [GitHub-Beispiele für Image-Veröffentlichung](https://docs.github.com/en/actions/tutorials/publish-packages/publish-docker-images)
sind eine Grundlage für einen späteren eigenen Workflow, nicht bereits installiert.

### Vorher übernommene Workflows bereinigen

`build.yml` baut bislang auch Desktop-Plattformen, fragt eine Upstream-Buildnummer
ab und ist keine fertige Funk-Releasepipeline. Seine Branchfilter `'*'` sind nicht
als vollständige Abdeckung verschachtelter `feature/...`-Branches zu übernehmen.
`pr-checks.yml` und `check_commit_style.py` enthalten weitere Upstream-Annahmen.

Besonders wichtig: `release_dispatch.yml` adressiert
`mumble-voip/mumble-docker`; `winget.yml` verwendet `Mumble.Mumble.Client` und
`Mumble.Mumble.Server`. **Vor dem ersten eigenen GitHub-Release** diese alten
Veröffentlichungswege im künftigen Releasebranch ersetzen/stilllegen, ohne sie
jetzt auszulösen oder `master` zu verändern. Keine Tokens für diese fremden Ziele
einrichten. Die übrigen übernommenen Workflows einschließlich Backport und CodeQL
auf passende Trigger, Berechtigungen und Build-Abhängigkeiten prüfen.

Actions werden erst später bewusst eingerichtet; erforderliche Statuschecks erst
nach einem realen erfolgreichen Lauf als Pflicht konfigurieren. Ein
`workflow_dispatch` benötigt die Workflow-Datei im Standardbranch
([GitHub-Dokumentation](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow)).
Solange `master` unverändert Standard bleibt, einen nur auf `dev` geplanten
manuellen Workflow nicht als schon nutzbaren Releaseknopf beschreiben. Die erste
Freigabe und Standardbranch-Umstellung müssen diesen Ablauf ausdrücklich ordnen.

## 5. Native Stable-Installation und sichere Updates planen

Später soll das Setup standardmäßig den freigegebenen Stable-Stand verwenden und
optional einen bestimmten unveränderlichen Versionstag akzeptieren. Noch gibt es
dafür keinen neuen Bedienbefehl.

Notwendige Änderungen in einem späteren PR:

- Die festen `funk-v0.1`-Prüfungen in `build.sh` und `setup.sh` durch überprüfte
  Release-/Commit-Auswahl ersetzen. Eine Installation vom festen Tag darf nicht
  versehentlich wieder den beweglichen Branch auschecken. Nicht beliebige Refs
  als offiziell freigegeben behandeln.
- Die gewählte Revision vor dem Build einmal auflösen/fixieren und protokollieren;
  keine Mischung aus während des Builds wechselnden Branchständen. Git-Verzeichnisse
  über Git ermitteln: Die feste `.git/funk-setup.lock`-Annahme muss auch für
  Worktrees und getrennte Git-Verzeichnisse überprüft werden.
- Bestehende Ubuntu-Allowlist, normalen Build-Benutzer, Rechte, Portkontrollen,
  Bestandsschutz und eigenen Dienst erhalten. Eine Versionsauswahl ersetzt keine
  Prüfung auf eine bestehende Installation.
- Update nur bewusst starten: neue Version bauen und testen, dann ausschließlich
  den eigenen Dienst kurz stoppen, Daten und Konfiguration konsistent sichern,
  Binärdatei umschalten und Start prüfen. Bei Fehlern die bekannte Binärversion
  wiederverwenden; mögliche Datenbankschemaänderungen getrennt beurteilen.
- Die vorhandenen Release-Verzeichnisse, `previous`-Verknüpfung und Sicherungen
  nutzen und deren Wiederherstellung testen. Ein Binär-Rollback ist keine Garantie,
  dass eine später geänderte Datenbank rückwärts kompatibel ist.

Docker erhält später eine eigene Registry-Compose-Variante mit festlegbarer
Version/Digest; der lokale Build bleibt für Entwickler verfügbar. Vor Updates
konsistent sichern, bewusst neue Version beziehen und nur die eigene Instanz
neu erstellen. Projektname und Volumes beibehalten; niemals Volume-Löschen als
Update-Schritt verwenden. Ein gemeinsamer Servercode bedeutet keine automatische
Migration einer nativen Datenbank in Docker.

## 6. Spätere GitHub-Startseite und erste Freigabe

Nach bestandenem Integrations-/Praxistest und ausdrücklicher Freigabe:

1. Den konkreten Kandidaten inklusive eigener Workflows, Versionsunterlagen und
   beider Installationswege freigeben und `stable` daraus erstellen.
2. Schutzregeln und Veröffentlichungsfreigabe nach dem
   [Zusammenarbeitsplan](github-collaboration.md) einrichten und ihre Wirkung testen.
3. Die README auf `stable` muss Funkfunktionen, unveränderte Clients, Ubuntu und
   Docker, aktuelle freigegebene Version, Versionsgeschichte sowie `dev` und seine
   experimentelle Natur erklären. Mumble als unabhängige Grundlage nennen und
   Lizenz-/Copyright-Hinweise erhalten. Status und Installationslinks dürfen nicht
   mehr auf ungeprüfte Feature-Stände als Stable verweisen.
4. Nach eigener Freigabe in GitHub unter **Settings → Default branch**
   (je nach Oberfläche unter General/Branches) `stable` auswählen. `master` nicht
   löschen oder überschreiben. Anschließend tatsächlichen Standardbranch,
   PR-Ziele, relative Links, Badges und Workflow-Trigger kontrollieren.
5. Erst den vereinbarten neuen Versionstag und die eigene Veröffentlichungsfolge
   auslösen. Vorher müssen die alten Upstream-Releasewege aus dem maßgeblichen
   Release-/Standardbranch entfernt oder deaktiviert sein. GitHub-Release und
   GHCR-Image müssen den tatsächlich gemeinsam getesteten Commit ausweisen.

Offene Entscheidungen: Docker-Testabnahme, Umgang mit dem UDP-Grenzfall, erste
gemeinsame Versionsnummer, Freigabetestumfang, CI-Ressourcen/Architekturen,
Registry-Sichtbarkeit und getrennte Freigaben für Schutzregeln, `stable`,
Standardbranchwechsel und Veröffentlichung. Diese Vorbereitungsphase nimmt keine
dieser späteren Entscheidungen vorweg.
