# Übergabestand Simplex v0.2

Arbeitszweig: `funk-v0.1`, Basis Mumble 1.5.939. Keine Arbeit auf der realen VM.

## Implementiert

- `src/murmur/SimplexController.*`: atomare Senderechte pro Kanal, blockierte
  Aussendungen, Marker/Timeout, konservative Wiederfreigabe und Lebenszyklus.
- Die zurückgegebene Sperre bleibt bis zum Ende von `Server::processMsg` bestehen;
  UDP und TCP können die Entscheidung und Weiterleitung nicht überholen.
- Alle Zustände enthalten Werte, keine User-/Channel-Zeiger; Verbindungskennungen
  sind innerhalb eines Serverlaufs eindeutig. Disconnect löscht den Zustand.
- `SimplexLogger.*`: eigene begrenzte asynchrone JSONL-Ausgabe, kein Sprachinhalt.
  Diagnoseverlust beeinflusst die Senderechte nicht.
- Integration in Server, Meta-Konfiguration, UserState, RPC und ACL-Änderungen.
- Im Funkmodus nur aktuelle Kanalmitglieder, kein Whisper/Loopback/Link/Listener.
- Standardwerte: `simplex=false`, `simplextimeout=500`,
  `simplexretrytimeout=1000`; Diagnose unabhängig von der PTT-Diagnose.

## Stand der Prüfung

Der inkrementelle Debug-Build ist erfolgreich. `TestSimplex` besteht alle elf
Testfunktionen (13 Qt-Testeinträge einschließlich Initialisierung/Aufräumen).
Es enthält 100 konkurrierende UDP/TCP-ähnliche Vergaben, einen Test der bis zur
Weiterleitung gehaltenen Sperre, 30 Sekunden kontinuierliche Pakete mit künstlicher
Uhr, verspätete Marker, Wiederholungen, Lebenszyklus und Logrotation/Dateifehler.

Auch die Integration mit einem echten Serverprozess und verschlüsselten Testclients
ist geprüft: 18/18 CTest-Programme erfolgreich (einschließlich aller bisherigen 16),
außerdem 5/5 Installationstests. Die neuen Integrationstests prüfen die tatsächlich
weitergeleiteten Pakete, Legacy- und Protobuf-Format, UDP/TCP, beide OFF-Varianten,
Lebenszyklus/ACL, alternative Sprachwege und Neustart. Die Installationstests
aktivieren Simplex ausschließlich in der isolierten Testinstanz auf 64739.

Noch offen vor Abschluss: Benutzeranleitung/Praxistestplan fertig prüfen,
abschließender Dokumentationscommit und Push. Der letzte gesicherte Stand ist
über `git log -1 --oneline` zu ermitteln.

## Bewusste Grenzen

Ohne PTT-Nachricht kann eine ausreichend lange Netzunterbrechung wie Loslassen und
erneutes Drücken aussehen. Fortlaufende Pakete eines blockierten Senders bleiben
gesperrt; eine Paketpause ab `simplexretrytimeout` erlaubt einen neuen Versuch.
Kurze PTT-Pausen ohne Marker können deshalb ebenfalls gesperrt bleiben. Zurückgesetzte
Sequenznummern werden erst nach dieser Paketpause neu akzeptiert. Ein echter
physischer Tastendruck und beliebig verspätete Pakete nach einer solchen Pause
sind ohne Protokolländerung nicht zweifelsfrei zu unterscheiden.
