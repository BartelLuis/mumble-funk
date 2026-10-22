# Experimentelles kontinuierliches Funkrauschen — B2b

Diese Vorschau ergänzt den privaten AudioFeedback-Weg. Der Simplex-Controller und
Sprachpakete bleiben unverändert. Eine Freigabe setzt Hörtests mit Windows-Mumble
und Android-Mumla voraus; automatisierte Paket-/Opus-Tests ersetzen diese nicht.

## Konfiguration

Neue Serveroptionen (Standardwerte):

```ini
radionoise=false
radionoisefile=
radionoisevolume=20
```

Nur bei aktiviertem Simplex und `audiofeedback=true` kann eine ausdrücklich
aktivierte Rauschfunktion arbeiten. Für eine eigene lokale Testkonfiguration
`radionoise=true` und einen absoluten Pfad zu einer eigenen WAV-Datei setzen.
Der Server muss die Datei lesen können. Das wird beim Start geladen; Änderungen
benötigen einen Neustart der eigenen Testinstanz.

Akzeptiert wird RIFF PCM-WAV, Mono, 48 kHz, signed 16 Bit Little Endian, mit
20 ms bis 60 Sekunden Audiodaten und höchstens 8 MiB Dateigröße. Die Aufnahme wird
nicht verändert. Leerer Pfad, Volume 0 oder eine ungültige Datei deaktivieren nur
das optionale Rauschen. Fehler werden im Serverlog gemeldet.

Die Lautstärke ist unabhängig, erlaubt 0..100 und verwendet keinen globalen
`feedbackboostdb`. Volume 0 erzeugt keine Rauschpakete oder Rausch-Worker.
Bestehende INI-Dateien und Audio-Standardeinstellungen werden nicht migriert.

## Zustellung und Grenzen

Pro angenommener Übertragung wird ein gemeinsamer ursprünglicher Zufallsstart auf
einem 20-ms-Raster gewählt. Separate Encoder erlauben unabhängige Unterbrechungen
je Empfänger. Am WAV-Ende folgt samplegenau Sample 0, ohne Crossfade, zusätzliche
Nullen oder Encoderreset. Nach Busy wird die bisherige PCM-Position fortgesetzt.

Busy hat höchste Priorität. Open geht vor Rauschen; Close beendet es. Bestätigtes
Ende und Simplex-Timeout stoppen Rauschen; Timeout-Close folgt nur bei aktiviertem
bestehendem Fallback. Ein Timeout beweist kein physisches Loslassen von PTT.
Rauschen funktioniert auch bei bewusst ausgeschaltetem Squelch.

Maximal 16 vorbereitete Produzenten pro Prozess, je drei gepufferte Pakete,
verwenden einen gemeinsamen PCM-Puffer. Erzeugung und Thread-Joins liegen beim
Start beziehungsweise Herunterfahren, nach dem Stoppen des Sprachthreads.
Während der Zustellung werden nur nicht wartende Produzentenzugriffe verwendet.
Bei Überlast oder TCP-Stau kann optionales Rauschen entfallen. Der Scheduler
vergibt die ausgehenden Sequenzen, Ende-Markierungen und Abstände zentral.

Empfänger werden am Übertragungsbeginn erfasst. Neue Empfänger erhalten Effekte
ab der nächsten Übertragung. Identität, Kanal und Übertragungsgeneration werden
vor der Zustellung geprüft; Kanalwechsel, Disconnect und Berechtigungsverlust
invalidieren die zugehörigen Effekte. Bereits übertragene/clientseitig gepufferte
Pakete können nicht zurückgerufen werden. Die bestehende 60-ms-Auslaufpause ist
keine Garantie für einen Decoderreset in jedem Client. Gemeinsame TCP-Verbindungen
und Netzwerkengpässe bleiben insbesondere praktisch zu prüfen.

## Lokale Prüfungen

Der normale Server-Build registriert `TestRadioNoise`, `TestNoiseScheduler` und
`TestNoiseIntegration` zusätzlich zu den bisherigen Regressionstests.
`TestRadioNoise` prüft kontinuierliche Kodierung gegen eine unabhängige Referenz.
`TestNoiseIntegration` verwendet temporäre Server mit Loopback-Adressen und
prüft TCP, verschlüsseltes UDP, unveränderte Sprache und den Übertragungslebenszyklus.

Optional kann die private Testaufnahme außerhalb des Repositorys verwendet werden:

```bash
MUMBLE_FUNK_NOISE_WAV=/absoluter/pfad/static.wav \
  ctest --test-dir build/funk --output-on-failure

build/funk/tests/TestRadioNoise /absoluter/pfad/static.wav
```

Der zusätzliche Dateitest erwartet die für B2a/B2b bekannte Aufnahme und ihre
Prüfsumme. Ohne diese Datei verwenden die Tests vorhandene öffentliche Kurzsignale
beziehungsweise programmgenerierte Test-WAVs. Die private Aufnahme ist kein
Repository-Asset und wird nicht heruntergeladen oder veröffentlicht.

B2b ergänzt keine Installerfreigabe für `feature/radio-noise`, keine Ubuntu-/Docker-
Audioverwaltung und keine neuen Ports. B2c soll nach gesonderter Freigabe die
Verwaltung eigener Dateien, Installation und Backup/Restore ergänzen.
