# Entwicklung: gemeinsamer Ubuntu-/Docker-Stand

Stand: 18. Oktober 2026. Simplex-Engine v0.2 auf Mumble Server 1.5.939.
Ubuntu und Docker werden aus demselben Commit gebaut. Der gemeinsame `stable`
ist zunächst ein **Stable-Kandidat**, keine behauptete Produktionsfreigabe.
Die abschließende Abnahme auf dem Docker-Host des Kollegen steht noch aus.

| Branch | Rolle |
| --- | --- |
| `stable` | Gemeinsamer geprüfter Installationskandidat; endgültige Freigabe gesondert |
| `dev` | Gemeinsame Weiterentwicklung, Änderungen nachvollziehbar per PR |
| `integration/stable-v0.2` | Integration aus `dev` plus getesteter Docker-Verpackung |
| `funk-v0.1` | Unveränderte bisherige Ubuntu-/Simplex-v0.2-Referenz |
| `feature/docker-install` | Unveränderte bisherige Docker-Testbasis |
| `feature/audio-feedback` | Getrennte experimentelle Audioentwicklung; nicht integriert |
| `master` | Historische Mumble-Basis, weiterhin GitHub-Standardbranch |
| `weblate-translation-update` | Unverändert erhaltene Übersetzungsarbeit |

Die vollständigen ursprünglichen Commit-IDs und Integrationsergebnisse stehen im
[Integrationsbericht](STABLE-INTEGRATION-STATUS.md). Alle bestehenden Archiv- und
Versionstags bleiben erhalten, insbesondere `funk-v0.2.0` auf
`95ac344838c1535f943dccc7d134a280aac6a340` (noch ohne Docker).

## Änderungen und Tests

Neue Funktionen auf `feature/<thema>` aus aktuellem `dev` entwickeln, mit Tests und
Dokumentation ergänzen und per Pull Request nach `dev` prüfen lassen. Eine
Integration erfordert erneute Prüfungen beider Installationswege. Ein grüner Build
ersetzt weder Geräteprüfung noch die Freigabe eines konkreten Commits.

Die Simplex-Vergabelogik dieser Integration ist unverändert. Der separat geprüfte
UDP-Fix setzt nach erfolgreicher Authentifizierung die bekannte Clientversion
wieder ein. Neue Regressionstests prüfen ältere/neue Formate, ersten/bekannten
UDP-Peer und aktivierten/deaktivierten Simplex-Modus. Die bisherigen Docker-
Nonce-Adapter werden entfernt; dieselben echten Testquellen laufen in beiden Builds.
Audio-Engine, WAV-Verwaltung, Audio-Codecs oder Audio-Feedback werden nicht übernommen.

Die Installer akzeptieren `stable`, weiterhin `funk-v0.1` und künftig veröffentlichte
annotierte Projektversionstags. `dev` und Integrationsbranches sind bewusst keine
Installationsquellen. Entwickler bauen **ohne Installation** direkt mit CMake:

```bash
git submodule update --init --recursive
cmake -S . -B build/dev -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_NUMBER=939 \
  -Dclient=OFF -Dserver=ON -Dtests=ON -Dplugins=OFF -Doverlay=OFF \
  -Dlto=OFF -DCMAKE_UNITY_BUILD=OFF \
  -DCMAKE_PROJECT_mumble_INCLUDE="$PWD/scripts/docker/tests/register.cmake"
cmake --build build/dev --parallel 2
ctest --test-dir build/dev --output-on-failure
python3 tests/funk-install/test_source.py
python3 tests/funk-install/test_setup.py
python3 tests/funk-install/test_install.py build/dev/mumble-server
```

Die zusätzliche Probe `build/dev/FunkDockerProbe` ist nur ein Testwerkzeug, kein
Bestandteil des Laufzeit-Images. Docker-Integrationstests erzeugen ausschließlich
eigene Wegwerf-Projekte; diese dürfen sie anschließend mit ihren Testvolumes
aufräumen. Für eine echte Serverumstellung gilt dagegen **niemals `down -v`**.

[Docker-Anleitung](docker.md) · [Migration](docker-stable-migration.md) ·
[Ubuntu-Setup](setup.md) · [Releaseplan](release-plan.md) ·
[Empfohlenes GitHub-Schutzmodell](github-collaboration.md)

## Audio-Integration, Phase 1

`feature/audio-engine` baut auf dem gemeinsamen `dev`-/Stable-Stand auf und
integriert ausschließlich die vorhandene Busy-Tone-Engine. Keine automatische
Übernahme nach `dev` oder `stable`, keine neue Freigabe. Die historische Audio-
Preview bleibt erhalten. [Bedienung und Architektur](audio-engine.md),
[Integrationsnachweis](AUDIO-ENGINE-STATUS.md).
