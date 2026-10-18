# GitHub-Schutz und Zusammenarbeit: konkreter Einrichtungsplan

**Nur Empfehlung, noch nicht aktiviert.** Stand: 18. Oktober 2026.
Es werden keine Einladungen, Berechtigungen, Rulesets, Branch-Protection-Regeln,
CODEOWNERS-Dateien, Environments oder Registry-Rechte eingerichtet.

[Entwicklungsablauf](development.md) · [Releaseplan](release-plan.md)

## 1. Tatsächliches Repository und technische Möglichkeiten

Die aktuelle GitHub-Abfrage bestätigt ein **öffentliches, persönliches Repository**
von `Shawna7han` (`owner.type=User`, `private=false`), Standardbranch `master`.
Die fünf bisherigen Branches melden `protected=false`, die Ruleset-Liste ist leer.
Offene PRs und Actions-Läufe wurden bei der Bestandsaufnahme nicht gefunden.
`dev` kommt als neuer Vorbereitungsbranch hinzu; `stable` existiert noch nicht.

Die Möglichkeiten wurden mit der aktuellen offiziellen GitHub-Dokumentation
abgeglichen, nicht durch unerlaubtes Anlegen einer Schutzregel ausprobiert:

| Mechanismus | Für dieses öffentliche persönliche Repository möglich? | Bedeutung |
| --- | --- | --- |
| Klassischer Branch-Schutz | Ja, grundsätzlich schon mit GitHub Free | PRs, Reviews, Statuschecks, Lösch-/Force-Push-Schutz |
| Branch- und Tag-Rulesets | Ja, grundsätzlich schon mit GitHub Free | Mehrere kombinierbare Regeln, auch für Versionstags |
| CODEOWNERS | Ja | Zuständigkeiten; verbindliche Freigabe nur mit zusätzlicher Schutzregel |
| Environment mit Freigabe-Reviewer | Ja bei öffentlichem Repository | Publish-Job kann auf eine bewusste Freigabe warten |
| Persönliche Collaborator-Rollen | Inhaber und Collaborators | Keine getrennte Docker-Pfad-Schreibrolle |
| Klassische Liste „Restrict who can push“ | Nicht als Organisationsfunktion für dieses persönliche Repository einplanen | Nicht mit PR-Pflicht verwechseln |
| Regeln für bestimmte Reviewer-Teams | Nein, keine Organisationsteams in einem persönlichen Repository | Benutzer/Owner und passende Rulesets verwenden |
| Organisationsweite Sonderrollen / Push-Rulesets für Dateipfade | Nicht Grundlage dieser Lösung | Keine solche Funktion oder kostenpflichtige Organisationsstruktur voraussetzen |

Quellen: [geschützte Branches](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches),
[Rulesets](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets),
[persönliche Repository-Berechtigungen](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/repository-access-and-collaboration/permission-levels-for-a-personal-account-repository)
und [klassische Branch-Regeln](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/managing-a-branch-protection-rule).

## 2. Empfohlener Einstieg für den zweiten Entwickler

**Zunächst eigener Fork und Pull Requests:** Der zweite Entwickler kann das
öffentliche Repository forken, dort eigene `feature/docker-…`-Branches pflegen und
PRs mit Ziel `dev` vorschlagen. Dafür ist keine Einladung mit Schreibrechten auf
dem Hauptrepository nötig. Der Inhaber prüft und übernimmt. Die bereits laufende
Docker-Prüfung wird nicht auf einen anderen Branch umgelenkt.

Ein Collaborator im persönlichen Hauptrepository hätte dagegen allgemeine
Schreibrechte, nicht ausschließlich Rechte für `Dockerfile` oder `scripts/docker/`.
Falls gemeinsames Schreiben im Hauptrepository gewünscht ist: erst Schutzmodell
und Registry-Rechte freigeben und prüfen, dann den tatsächlichen GitHub-Benutzernamen
einladen. Dieser Name ist noch nicht bekannt; es wurde keine Einladung versendet.
Bei späterem Bedarf an differenzierten Teams/Rollen kann eine Organisation bewusst
geprüft werden, ohne sie für diese erste Zusammenarbeit vorauszusetzen.

Review-Aufteilung: Der zweite Entwickler prüft Docker, Compose, Datenhaltung und
Container-Tests; der Inhaber beurteilt Server-/Simplex-Verhalten, Audio und
Releasefreigabe. Änderungen an gemeinsamen Tests, Builddateien, Workflows und
Installationspfaden werden zusammen geprüft. Ein grüner Docker-Test ersetzt keine
Prüfung einer zugleich geänderten Serverfunktion.

## 3. Vorgeschlagenes Schutzmodell für stable

Eine gewöhnliche PR-Pflicht allein garantiert **nicht**, dass nur der Inhaber eine
Änderung übernehmen kann: Ein Collaborator kann je nach Regeln einen freigegebenen
PR selbst mergen. Vorschlag sind deshalb zwei getrennte Branch-Rulesets für exakt
`stable`, sobald dieser nach Freigabe existiert:

| Ruleset | Geplante Regeln | Bypass |
| --- | --- | --- |
| `stable-quality` | PR verpflichtend, mindestens ein zustimmendes Review durch eine andere Person, überholte Reviews nach neuen Änderungen verwerfen, Gespräche auflösen, tatsächlich eingerichtete Statuschecks bestehen; Force-Push und Löschen sperren | Keine normale Ausnahme |
| `stable-owner-merge` | `Restrict updates`: nur der Inhaber darf den Stable-Stand übernehmen | Ausschließlich Repository-Admin-Rolle, **For pull requests only**; keine Write-/Maintain-Rolle und keine fremde App |

Im persönlichen Repository ist der Inhaber der administrative Verantwortliche.
Die Ausnahme gehört ausschließlich in das zweite Ruleset: Sonst könnte sie auch
Tests oder Reviews umgehen. Sie erlaubt dem Inhaber die Übernahme per PR, während
die Qualitätsregeln des ersten Rulesets weiterhin gelten. Kein allgemeiner
„Always allow“-Bypass für Stable-Änderungen. Mit Owner-PRs prüft die zweite Person;
mit PRs der zweiten Person prüft der Owner. Die abschließende Übernahme bleibt
beim Owner. Falls eine genaue Einstellung im aktuellen GitHub-Dialog anders
angeboten wird, nicht durch ein schwächeres Modell ersetzen, sondern zuerst prüfen.

Das ist eine aus den dokumentierten Regeln abgeleitete, noch zu erprobende
Konfiguration. Sie wurde in dieser Phase nicht aktiv geschaltet. Der Inhaber kann
als Administrator Einstellungen später ändern; Schutzregeln ersetzen keine
Verantwortung des Kontoinhabers.

Die offiziellen Beschreibungen erklären [Restrict updates und Reviewregeln](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets)
sowie [Bypass nur für Pull Requests](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/creating-rulesets-for-a-repository).
Die dortigen speziellen Team-Reviewer-Regeln stehen persönlichen Repositorys nicht
zur Verfügung. Normale Review-/Statusanforderungen sind davon zu unterscheiden.

Für `dev` später zunächst PRs, einen passenden Review, vollständige CI und
Lösch-/Force-Push-Schutz vereinbaren. Feature-Branches bleiben Arbeitsbereiche.
Die bisherigen fünf Branches werden jetzt nicht verändert oder gelöscht; ihre
weitere Behandlung benötigt eigene Freigabe.

## 4. CODEOWNERS sinnvoll verwenden

Eine spätere `.github/CODEOWNERS` kann den Inhaber für Standard- und kritische
Änderungen zuordnen, zum Beispiel:

```text
* @Shawna7han
/.github/ @Shawna7han
```

Dies ist nur ein Dokumentationsbeispiel, keine angelegte Datei. Der zweite
Entwickler kann für Docker zusätzlich als Reviewer angefordert werden. Mehrere
Codeowner auf einer Zeile bedeuten nicht, dass alle zustimmen müssen; einer kann
genügen. Daraus nicht versehentlich eine Umgehung der Inhaberfreigabe machen.

CODEOWNERS wirkt aus dem PR-Zielbranch und benötigt gültige Benutzer mit passenden
Rechten. Ohne verpflichtende Codeowner-Regel ist es eine Zuständigkeits-/Reviewhilfe.
Der oben vorgeschlagene Owner-Merge-Schutz bleibt deshalb maßgeblich. Eine zusätzlich
verpflichtende Owner-only-Codeowner-Freigabe muss auch den Fall eines vom Owner
selbst verfassten PRs berücksichtigen; Autoren können ihr eigenes Review nicht als
unabhängige Freigabe abgeben. Selbstblockaden nicht durch beliebige Bypässe lösen.

Siehe [GitHub: Code Owners](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners).

## 5. Versionstags und Veröffentlichung absichern

Geplante Tag-Rulesets müssen bestehende `funk-v*`-Tags sowie alle Archiv-Tags
einschließlich verschachtelter Namen erfassen. `archive/mumble/*` allein genügt
nicht für alle Unterpfade. Die Zielauswahl vor Aktivierung ausdrücklich anhand
von `archive/mumble/1.4.x` und `archive/mumble/backport/1.6.x/pr-7315` prüfen.

- Bestehende Versionen und Archive: Updates und Löschungen sperren, keine normale
  Bypass-Ausnahme. Das gilt auch für `funk-v0.2.0`; es bleibt auf seiner bisherigen
  vollständigen Commit-ID.
- Neue Release-Tags: in einem getrennten Ruleset ihre Erstellung auf den Inhaber
  beschränken. Eine dafür notwendige Ausnahme darf nicht zugleich die Sperre für
  Änderungen/Löschungen bestehender Tags aushebeln.
- Der spätere Publish-Workflow prüft zusätzlich Commit-Zugehörigkeit und
  Versionsmanifest. Ein passender Tag-Name allein ist keine Releasefreigabe.

Für den Publish-Job eine Umgebung `stable-release` mit dem Inhaber als einzigem
Freigabe-Reviewer vorsehen. „Prevent self-review“ verhindert auch die Genehmigung
eines selbst gestarteten Laufs. Soll der Inhaber eigene Veröffentlichungen starten
und freigeben, muss diese Einstellung bewusst dazu passen; alternativ braucht es
einen zweiten freigegebenen Reviewer. Nicht unbemerkt einen unbedienbaren Ablauf
konfigurieren. [GitHub: Environments und Reviewer](https://docs.github.com/en/actions/reference/workflows-and-actions/deployments-and-environments).

**Registry-Rechte gesondert beurteilen:** Ein Freigabe-Environment schützt nur Jobs,
die es tatsächlich verwenden. Bei später erteilten allgemeinen Schreibrechten
darf ein anderer Feature-Workflow nicht einfach ohne Environment mit
`packages: write` dieselbe Release-Package überschreiben können. Auch direkte
Paket-Schreibrechte eines Collaborators werden nicht durch PR-Regeln aufgehoben.
Deshalb ist zunächst Fork/PR ohne Schreibrechte im Hauptrepository empfohlen.
Vor einer Einladung muss die gewünschte GHCR-Berechtigungsgrenze nachgewiesen
werden; nötigenfalls eine getrennt kontrollierte Release-Infrastruktur planen.
Eine Umgebungsfreigabe allein nicht als vollständigen Registry-Schutz verkaufen.
Grundlage: [Paketrechte und Sichtbarkeit](https://docs.github.com/en/packages/learn-github-packages/configuring-a-packages-access-control-and-visibility).

## 6. Sichere Reihenfolge einer späteren Einrichtung

1. Inhaber bestätigt Arbeitsmodell (zunächst Fork/PR oder Collaborator), GitHub-Name
   des zweiten Entwicklers und gewünschte Review-/Release-Verantwortung.
2. Eigene Funk-CI in einem gesonderten PR umsetzen. Aktionen möglichst auf geprüfte
   Commit-IDs festlegen. PR-Jobs erhalten nur nötige Leserechte, keine Publish-Secrets;
   keinen fremden PR-Code mit privilegiertem `pull_request_target` ausführen.
3. Nach echten erfolgreichen Läufen stabile Check-Namen und GitHub Actions als
   erwartete Quelle festlegen. Nicht erfundene Checks zur Pflicht machen und keine
   Schutzwirkung auf bislang nicht gelaufene Workflows stützen.
4. Rulesets zunächst als Konfiguration prüfen, dann nach ausdrücklicher Freigabe
   aktivieren. Es wird keine kostenpflichtige „Evaluate“-Funktion vorausgesetzt.
5. Kontrolliert nachweisen: direkter Stable-Push abgelehnt; Collaborator kann
   Stable nicht mergen; Owner kann einen freigegebenen PR mergen; fehlgeschlagene
   Checks, neue Änderungen nach Review und Tag-Manipulation werden abgewiesen.
   Solche Schreibversuche nur mit vorheriger Testfreigabe und geeigneten Testzielen,
   niemals als unangekündigten Versuch am laufenden Stable-/Archivbestand.
6. Registry-/Workflow-Berechtigungen mitprüfen: fremder PR und ungeprüfter
   Feature-Workflow können keine offizielle Version veröffentlichen. Erst dann
   gegebenenfalls Collaborator einladen und den abgenommenen Releaseweg aktivieren.

Zur späteren Bedienung: **Settings → Rules → Rulesets** für die neuen Regeln,
**Settings → Collaborators / Manage access** für eine ausdrücklich freigegebene
Einladung und **Settings → Environments** für die Releasefreigabe. Bezeichnungen
können sich ändern. In dieser Phase wurden ausschließlich lesende GitHub-Abfragen
ausgeführt; keine dieser Einstellungen wurde gespeichert.
