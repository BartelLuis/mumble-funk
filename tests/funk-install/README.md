# Tests der Ubuntu-Installation

Die Installationstests laufen getrennt von den 18 CTest-Programmen des Servers:

```bash
python3 tests/funk-install/test_install.py build/funk/mumble-server
bash -n scripts/funk/build.sh scripts/funk/install.sh scripts/funk/platform.sh
shellcheck scripts/funk/build.sh scripts/funk/install.sh scripts/funk/platform.sh
```

`shellcheck` ist ein optionales Entwicklungswerkzeug (`sudo apt install shellcheck`).
Die Tests benötigen keine root-Rechte und installieren keinen dauerhaften Dienst.
Sie prüfen fünf Fälle:

1. Wiederholung erhält eigene Einstellungen und Datenbank sowie Dateien einer
   nachgebildeten Standardinstallation. Programmdatei und Dateirechte stimmen.
2. Ein Versionswechsel behält das vorige Programm für die Rückkehr vor; eine
   Wiederholung derselben Installation verändert diesen Verweis nicht.
3. Eine falsche Programmversion sowie `/` als Testziel werden abgewiesen.
4. Zwei echte Serverprozesse starten parallel auf TCP/UDP 64738 und 64739 mit
   getrennten SQLite-Datenbanken und Logdateien. Nur beim zweiten ist die Diagnose
   aktiviert. Zusätzlich ist nur auf 64739 Simplex mit eigener Diagnose aktiv;
   die JSONL-Datei und Startmeldungen werden geprüft. Beide Prozesse werden
   anschließend beendet und Testdaten entfernt.
5. Die Betriebssystemprüfung erlaubt ausschließlich Ubuntu 24.04 und 26.04;
   ältere, neuere, andere und fehlende Kennungen werden abgewiesen (zehn Kombinationen).

Wenn einer dieser Ports schon belegt ist, wird der vierte Fall übersprungen.
Kein bestehender Prozess wird beendet. Netzwerkzugriff auf localhost muss in der
Testumgebung gestattet sein.

`install.sh --stage VERZEICHNIS SERVERBINARY` ist der intern verwendete Testmodus.
Er erzeugt ausschließlich eine Verzeichnisstruktur unter dem angegebenen Ziel,
legt keine Systembenutzer an und führt kein systemctl aus. Die erzeugten
Konfigurationspfade sind die späteren Produktivpfade, keine direkt ausführbare
Konfiguration für dieses Testverzeichnis.

Dieser Testmodus allein prüft weder systemd noch den Start beim Hochfahren.
Für den Produktivbetrieb gilt die [Ubuntu-Anleitung](../../docs/funk/installation-ubuntu.md).
