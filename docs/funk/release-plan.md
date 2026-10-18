# Releaseplan nach der Ubuntu-/Docker-Integration

Die Installation wird gemeinsam auf `stable` vorbereitet. Dieser Branch ist
zunächst ein **Kandidat**: Die endgültige Freigabe folgt erst nach dem Test des
Kollegen auf seinem Docker-Host. Es werden jetzt kein Release, kein neuer Tag und
kein öffentliches Registry-Image erstellt. GitHubs Standardbranch bleibt `master`.

## Versionsidentität

Vorschlag für die gemeinsame nächste Veröffentlichung: **`funk-v0.2.1`**.
Mumble-Basis: **1.5.939**, Simplex-Engine: weiterhin **v0.2**.
Das vorhandene annotierte `funk-v0.2.0` bleibt unverändert auf
`95ac344838c1535f943dccc7d134a280aac6a340` und enthält noch keine Docker-Verpackung.
Die experimentelle Audio-Entwicklung gehört nicht zu diesem Kandidaten.

Ubuntu und Docker müssen aus demselben vollständigen Git-Commit gebaut werden:

```bash
git rev-parse HEAD
```

Diese ID gehört in Testberichte und Fehlermeldungen. `Mumble 1.5.939` und der
bewegliche Branchname allein reichen nicht. Unterschiedliche Bibliotheken oder
Compiler dürfen unterschiedliche Binärdateien erzeugen. Noch werden keine
unveränderlichen vorgefertigten Registry-Images verteilt; Docker baut lokal.

## Vor der endgültigen Freigabe

- [ ] Den im Integrationsbericht benannten Kandidaten auf dem echten Docker-Host
  des Kollegen mit vorhandenem Projekt, Port 64740 und gespeicherten Daten testen.
- [ ] Mumla/Windows-Mumble: hörbare Sprache, konkurrierende PTT-Versuche, Ende und
  Timeout, Kanalwechsel, Trennung/Wiederverbindung und schnellen Wechsel prüfen.
- [ ] UDP und TCP-Tunnel durch die tatsächliche Firewall/NAT prüfen.
- [ ] Reales Host-/Docker-Neustartverhalten und native systemd-VM-Starts prüfen;
  Container-/Dienstsimulationen nicht als VM-Neustartnachweis zählen.
- [ ] Backup, Wiederherstellung und Rückkehr zum vorherigen Image auf dem Zielhost
  nachvollziehen. Automatisierte Cloud-Tests ersetzen diesen Abnahmeschritt nicht.
- [ ] Umfang und Ergebnis eines längeren Betriebsversuchs dokumentieren. Kein
  abgeschlossener Langzeit-/Lasttest und keine ARM-Freigabe werden behauptet.
- [ ] Inhaber gibt die konkrete vollständige Commit-ID zur Veröffentlichung frei.

Nach dieser Freigabe können ein neues annotiertes Versionstag, Release-Unterlagen
und später ein Registry-Veröffentlichungsweg gesondert beauftragt werden. Erst nach
separater Zustimmung wird GitHubs Standardbranch `stable`. `master` und bisherige
Entwicklungsbranches bleiben bis dahin erhalten.

## GitHub-Prüfungen und spätere GHCR-Veröffentlichung

Die Integrationsversion enthält serverbezogene Build-, Installer- und CodeQL-
Workflows. Die früheren Veröffentlichungsaktionen für `mumble-voip/mumble-docker`
und Mumble-WinGet-Pakete sowie Client-Builds sind dort entfernt. Es gibt keinen
Registry-Login oder Publish-Job, keine automatischen Backports und keine
Veröffentlichung beim Anlegen eines Branches. Bestehende historische Branches
enthalten weiterhin ihre unveränderten Dateien.

Die eigenen Prüfungen bauen Server und Docker, prüfen die vollständige CTest-
Suite, native Installationshilfen sowie echte Compose-Integration. CodeQL schreibt
nur Sicherheitsanalyseergebnisse. Die definierten Workflows sind kein Beleg, dass
GitHub Actions im Fork aktiviert ist oder ein Lauf bereits bestanden wurde; dafür
sind die tatsächlichen Laufresultate maßgeblich. Es werden keine neuen
Schutzregeln oder Prüfausnahmen automatisch eingerichtet.

Für späteres GHCR: nur nach Freigabe, mit konkreter Source-ID, OCI-Revision,
Version und Digest; eigene nur für Veröffentlichung berechtigte Umgebung,
Inhaberfreigabe und unveränderte frühere Versionen für Rückkehr. Kein automatisches
Deployment auf fremde oder laufende Server. `docker compose pull` ist für das
heutige lokale `:local`-Image **kein** Updateweg.

[Integrationsnachweis](STABLE-INTEGRATION-STATUS.md) ·
[Entwicklung](development.md) · [GitHub-Schutzvorschlag](github-collaboration.md)
