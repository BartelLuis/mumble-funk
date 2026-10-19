# Sicherheitsprüfung: Audio-Preview, PR #2

Stand: 19. Oktober 2026, ausschließlich `feature/audio-engine`.
Die drei gemeldeten CodeQL-Probleme werden einzeln bewertet. Keine globale
CodeQL-Regel, kein Test und keine Sicherheitsprüfung wird deaktiviert. Es gibt
keinen automatischen Merge, keine Installation und keine Stable-Freigabe.

## 1. Passwort in Standardausgabe

**Ursache:** `Installation.info()` gab das Serverpasswort ohne ausdrückliche
Nachfrage mit `print()` aus. Das Programm verlangt im normalen Betrieb zwar root,
aber eine Umleitung, `tee` oder ein automatischer Aufruf konnte es in Logs ablegen.
Die bisherige Option `--hide-password` war nur eine freiwillige Schutzmaßnahme.
Das ist ein tatsächliches Risiko unbeabsichtigter Offenlegung.

**Korrektur:** Der Informationsbericht verbirgt das Passwort immer, unabhängig
von Terminal, Umleitung und bisherigen Funktionsargumenten. Auch normale
interaktive Aufrufe und Setup-Ausgaben bleiben geheimnisfrei. Der bekannte Befehl
`sudo mumble-funk-audio-engine-info` funktioniert weiterhin.

Eine absichtliche Anzeige erfordert nun zusätzlich **`--show-password`**, ein
interaktives stdin und stdout sowie ein verfügbares steuerndes Terminal.
Das Passwort wird ausschließlich nach `/dev/tty` geschrieben, nie nach stdout
oder stderr. Umleitungen und Pipes führen für diese Option zum Abbruch. Die
root-Prüfung für die echte Installation bleibt bestehen. `--hide-password` bleibt
kompatibel; die beiden Optionen schließen sich gegenseitig aus.

Bewusstes Anzeigen auf einem Bildschirm ist keine Logausgabe. Gegen eine
absichtliche Terminalaufzeichnung oder das Mitlesen am Bildschirm kann die CLI
nicht schützen; deshalb erfolgt die Anzeige ausschließlich nach ausdrücklicher
Anforderung. Konfiguration, Passwort und Datenbank werden dabei nicht verändert.

Regressionstests prüfen echte Pseudoterminals und umgeleitete Ausgaben: Standard
und alte Aufrufvarianten ohne Geheimnis, ausdrückliche Terminalanzeige, keine
Offenlegung bei stdout-Umleitung oder fehlendem steuerndem Terminal.

## 2. TLS-Mindestversion im Integrationstest

**Ursache:** Der Test erzeugte `SSLContext(PROTOCOL_TLS_CLIENT)`, ohne eine
Mindestversion ausdrücklich zu setzen. Moderne Python/OpenSSL-Versionen nutzen
bereits sichere Defaults; die Sicherheitsanforderung war jedoch nicht explizit
und damit abhängig von der Testumgebung.

**Korrektur:** Der Test verlangt ausdrücklich **TLS 1.2 oder neuer** über
`context.minimum_version = ssl.TLSVersion.TLSv1_2`. Alle vier isolierten
Serververbindungen müssen damit weiterhin funktionieren. Der produktive
Mumble-TLS-Code und dessen Konfiguration werden nicht verändert.

Die abgeschaltete Zertifikatsprüfung dieses Tests betrifft nur seine neu
erzeugten, selbstsignierten lokalen Testzertifikate. Sie ist keine Einstellung
für echte Serververbindungen. Keine produktiven Zugangsdaten werden übertragen.

## 3. Kurzzeitige Portprüfung an 0.0.0.0 und ::

**Bewertung: kontextbezogener Fehlalarm, kein offener Anwendungsdienst.**

Die Prüfung wertet zuerst `ss` aus und lehnt fremde beziehungsweise nicht
eindeutig zugeordnete Prozesse ab. Ein weiterer Bind-Test fängt insbesondere
bereits gebundene TCP-Sockets ab, die noch nicht lauschen und daher bei einer
reinen Listener-Abfrage fehlen können. Eine fehlgeschlagene `ss`-Abfrage führt
zum Abbruch, nicht zur Annahme eines freien Ports.

Die Wildcard-Adressen sind erforderlich: Eine Bind-Probe auf `127.0.0.1` könnte
erfolgreich sein, während derselbe Port auf einer anderen lokalen Adresse
belegt ist. Eine solche Änderung würde gerade die verlangte Schutzfunktion
verschlechtern. Die vorhandene IPv4-/IPv6-Trennung mit `IPV6_V6ONLY` bleibt.

Die Prüfsockets sind durch einen `with`-Block kurzlebig. Sie führen ausschließlich
`bind()` aus und schließen unmittelbar danach. Es gibt kein `listen()`, `accept()`,
`recv*()` oder `send*()`, keine Verarbeitung von Netzwerkinhalten und keine
Weitergabe von Zugangsdaten. Ein UDP-Socket kann während seiner kurzen Bindung
zwar Datagramme im Kernel puffern; die Anwendung liest oder verarbeitet sie nicht.
Es wird kein zusätzlicher dauerhafter Port oder Audio-Dienst eingerichtet.

**Entscheidung:** Den Bind-Test beibehalten und am Quellcode begründen. Keine
Umbenennung von Adressen zur Täuschung der Analyse, keine lokale oder globale
CodeQL-Unterdrückung und kein automatisches Schließen der Meldung.

Regressionstests belegen:

- TCP auf `127.0.0.2`, ohne `listen()`: Eine Probe auf `127.0.0.1` wäre möglich;
  die echte Prüfung verweigert den belegten Port korrekt.
- Gebundene TCP-IPv6- und UDP-IPv4-Sockets werden ebenfalls erkannt; der fremde
  Testsocket bleibt unverändert gebunden.
- Freie Prüfsockets werden geschlossen und rufen weder Listen-/Accept- noch
  Sende-/Empfangsoperationen auf. Anschließend ist der Port wieder nutzbar.
- Fehler bei der Socket-Bestandsabfrage führen sicher zum Abbruch.

Eine Vorprüfung ist keine atomare Reservierung bis zum späteren Dienststart:
Ein anderer Prozess könnte danach denselben Port belegen. Das ist eine bestehende
Race-Grenze; es rechtfertigt weder einen unsicheren Dienst noch eine schwächere
Prüfung. Die anschließende Dienststartprüfung bleibt bestehen.

## Tests und GitHub-Bewertung

Gezielter neuer Test: `python3 tests/funk-install/test_audio_security.py`.
Die elf Fälle bestehen einschließlich IPv6 und echter Terminaltests. Die fünf
bestehenden Preview-Tests bestehen mit explizitem TLS-Minimum ebenfalls.

| Erneut ausgeführte lokale Prüfung | Ergebnis |
| --- | --- |
| Vollständige vorhandene Servertests, einschließlich Simplex/UDP/Busy Tone | 20/20 bestanden |
| Neue Sicherheitstests | 11/11 bestanden, keine ausgelassen |
| Bestehende Preview-Tests mit TLS-Minimum | 5/5 bestanden |
| WAV-/Verwaltungs- und native Installationstests | 8/8 und 5/5 bestanden |
| Setup- und Quellstandprüfungen | 16/16 und 14/14 bestanden |
| Docker-Hilfsprogrammtests | 11/11 bestanden |
| Compose-Konfiguration aus `.env`, 64740 → 64739 TCP/UDP, eigener Host-Port/Fallback | Bestanden, keine Änderung der Dateien |
| ShellCheck und Git-Diff-Prüfung | Bestanden |

Der vorhandene native Server-Build wurde wiederverwendet: Kein C++-, Simplex-,
Audio- oder Docker-Laufzeitcode wird geändert. Eine erneute umfangreiche lokale
Kompilierung würde diese Python-Korrekturen nicht zusätzlich prüfen. GitHub führt
seine Build-/Integrationsworkflows nach dem Push gesondert aus. Der genaue
veröffentlichte Commit und deren Resultate werden im PR-Abschluss protokolliert.

CodeQL-Workflow-Erfolg und CodeQL-Sicherheitsbewertung sind unterschiedliche
Ergebnisse: Eine erfolgreich ausgeführte Analyse kann weiterhin eine begründete
Wildcard-Bind-Warnung anzeigen. Der Abschlussbericht muss diese getrennt nennen.
Die Portmeldung bleibt zur nachvollziehbaren Triage offen, sofern CodeQL sie
weiterhin meldet. Eine eventuelle spätere Einstufung als Fehlalarm gehört zur
konkreten Meldung, nicht zu einer pauschalen Regel-Ausnahme.

Simplex-Regeln, Busy Tone, C++-Server, Docker-Ports, `.env`, Volumes und die
bestehenden Installationen sind nicht Bestandteil dieser Korrekturen.
