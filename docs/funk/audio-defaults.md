# Standard-Audio für neue Installationen — experimentell

Arbeitsbranch **`feature/audio-defaults`**, aus `dev`
`35f83e9315945c50cbc3c0dc60c0b6f89ca83bb1`. Squelch, Audio Boost und der neue
CC0-Busy-Tone sind bereits enthalten. Keine Stable-Freigabe, kein neuer Release.

Neue Ubuntu- und Docker-Konfigurationen starten mit:

```ini
simplex=true
audiofeedback=true
busytone=true
busytonefile=/usr/local/share/mumble-funk/audio/busy-tone.wav
busytonevolume=50
squelch=true
squelchopen=true
squelchclose=true
squelchopenvolume=50
squelchclosevolume=50
feedbackboostdb=8
squelchclosetimeoutfallback=true
```

Beide Squelch-Dateien werden weiterhin aus
`/usr/local/share/mumble-funk/audio/` geladen. Die Lautstärken betragen 50 %;
der zusätzliche gemeinsame Boost beträgt +8 dB. Der Boost gilt für alle drei
Effekte. Sprachlautstärke und weitergeleitete Sprachpakete werden nicht verändert.
Simplex-Regeln, Empfängerzuordnung, Busy-Priorität und Squelch-Architektur bleiben
unverändert. Ein Timeout beweist kein Loslassen von PTT; der experimentelle
Mumla-Timeout-Close ist für neue Konfigurationen eingeschaltet. Ohne bestätigte
Ende-Markierung kann nach dem bestehenden Timeout einmal Close folgen. Ein
bestätigtes Ende erzeugt weiterhin sofort Close und keinen zusätzlichen Timeout-Close.

## Bestehende Installation: keine automatische Umstellung

Die neuen Werte gelten ausschließlich beim Erzeugen einer **fehlenden** INI.
Eine vorhandene INI wird nicht ergänzt oder migriert. Das gilt auch für fehlende
Schlüssel, ausgeschaltetes Simplex/Audio/Squelch, Boost 0, Volume 0, eigene WAVs
und einen leeren Busy-Pfad für den synthetischen Ton. Die bisherigen C++-Defaults
und die Ersatzwerte der Audioverwaltung bleiben deshalb unverändert: fehlendes
`audiofeedback`/`squelch` bleibt aus, fehlender Boost bleibt 0 und ein fehlendes
`squelchclosetimeoutfallback` bleibt false. Auch ein ausdrücklich gespeichertes
false bleibt bei Updates erhalten.

Vorhandene Datenbanken, Konten, Kanäle, Passwörter, Zertifikate und eigene Dateien
bleiben erhalten. Docker behält `.env`, Projektnamen und Volumes. Kein automatisches
Portwechseln: Ubuntu **64739 TCP/UDP**, Docker Host **64740 TCP/UDP → 64739 TCP/UDP**
im Container; bewusst konfigurierte andere Host-Ports gelten weiterhin.

Ein Update aktiviert ausgeschaltete Effekte nicht. Wer die neuen Werte auf einer
bestehenden Testinstanz bewusst einstellen möchte, kann die vorhandene
[Audioverwaltung](squelch-preview.md#normal-native-audio-management)
verwenden. Beispielsweise `boost 8`, `volume 50`, `open-volume 50`, `close-volume 50`,
`use-standard-busy-tone`, `enable-squelch`, `enable-open` und `enable-close`.
`enable-squelch` setzt bereits aktives Simplex voraus; es schaltet Simplex nicht um.
Ändernde Verwaltungsbefehle starten den eigenen Dienst kurz neu.

## Frische Ubuntu-Test-VM

Voraussetzungen: Ubuntu Server 24.04 oder 26.04 LTS mit systemd, normaler
sudo-Benutzer, Internet, freier Port 64739 und ausreichend RAM/Speicher laut
[Setup-Anleitung](setup.md). Die VM und ihre Proxmox-Snapshots verwaltet der
Betreiber selbst. Keine weitere Preview-Instanz oder zusätzlichen Dienste.

```bash
sudo apt update
sudo apt install --yes git ca-certificates
git clone --branch feature/audio-defaults --single-branch \
  https://github.com/Shawna7han/mumble-funk.git ~/mumble-funk
cd ~/mumble-funk
bash scripts/funk/setup.sh --experimental-audio-defaults
sudo mumble-funk-audio status
sudo mumble-funk-info
```

Die letzte Ausgabe enthält private Verbindungsdaten und das Passwort. Nicht
veröffentlichen. Das Setup baut und testet vor der Einrichtung des normalen
Dienstes `mumble-funk`. Die ausdrückliche experimentelle Option ist notwendig;
Herkunfts-, Versions-, Branch- und Sauberkeitsprüfungen bleiben erhalten.

Bestehende saubere Quellkopie: zuerst `git status --short` prüfen; bei Änderungen
anhalten und sichern. Erstmaliger Wechsel auf den neuen lokalen Branch:

```bash
git fetch origin refs/heads/feature/audio-defaults:refs/remotes/origin/feature/audio-defaults
git switch -c feature/audio-defaults --no-track origin/feature/audio-defaults
bash scripts/funk/setup.sh --experimental-audio-defaults
```

Später auf diesem Branch mit derselben expliziten Fetch-Zuordnung aktualisieren,
danach `git merge --ff-only origin/feature/audio-defaults` und Setup erneut ausführen.
Das funktioniert auch mit älteren Single-Branch-Klonen. Die bestehende Server-INI
bleibt bei allen Updates erhalten.

## Docker und Verwaltung

Die [Docker-Anleitung](docker.md) gilt einschließlich Backup und Wiederherstellung.
Bei Neuinstallation diesen Branch klonen und `docker compose up -d --build`
ausführen. Bei Updates im bestehenden Compose-Projekt arbeiten und vorher sichern;
keine vorhandene `.env` ersetzen und niemals `down -v` für ein Update verwenden.

```bash
bash scripts/docker/audio.sh status
bash scripts/docker/audio.sh boost 8
bash scripts/docker/audio.sh set-busy-tone /absoluter/pfad/eigener-ton.wav
bash scripts/docker/audio.sh use-standard-busy-tone
bash scripts/docker/audio.sh reset-busy-tone
```

`reset-busy-tone` bleibt der synthetische Doppelton. Eine ungültige direkt in der
INI ausgewählte Busy-Datei führt weiterhin zum synthetischen Fallback mit Warnung;
ungültige Importe lehnt die Verwaltung vor einer Auswahl ab. Eigene WAVs brauchen
keine Neukompilierung. Kein Audioasset oder seine Lizenz wurde hier verändert.

## Prüfen und Hörtest

Die lokalen Tests vergleichen die tatsächlich erzeugten Ubuntu-/Docker-INIs,
prüfen Bestandsschutz und Backup/Restore und starten isolierte reale Server mit
diesen Konfigurationen. Die Protokollprobe verlangt Open, Close und den gesamten
Busy bei 50 % und +8 dB, richtige Sessions, Sequenzen, Ende-Markierungen und
Opus-Dekodierbarkeit. Volume 0 wird gesondert geprüft. Vorhandene CTest-/Python-
Regressionen bleiben erhalten. GitHub führt anschließend Docker-Integration und
CodeQL aus; deren Abschluss wird in diesem Arbeitsablauf nicht abgewartet.

Auf der eigenen Test-VM zwei Clients im selben Kanal verbinden. Windows-Mumble
und Android-Mumla abwechselnd senden lassen: vollständige erste/letzte Silben und
Open prüfen; Windows mit bestätigtem Ende sollte Close erzeugen. Bei Mumla ohne
Ende-Markierung sollte nach dem bestehenden Timeout ebenfalls einmal Close
zu hören sein. Dabei PTT tatsächlich loslassen: Paketruhe kann ebenso durch Sprechpausen
oder Netzwerkstörungen entstehen. Bestätigtes Ende darf auch nach Ablauf des
Timeouts keinen zweiten Close-Ton erzeugen.
Während A sendet, versucht B zu senden: nur B hört Busy, B übernimmt den Kanal
nicht automatisch. Volume 0, eigene WAV und synthetischen Fallback bewusst
vergleichen. Hörtests auf echten Geräten und eine Stable-Freigabe bleiben offen.
Optionale Effekte dürfen unter Überlast weiterhin entfallen; Sprache hat Vorrang.
