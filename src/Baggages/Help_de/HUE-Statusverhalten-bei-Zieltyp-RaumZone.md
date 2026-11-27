### Statusverhalten bei Zieltyp Raum/Zone

Für Raum/Zone wird intern über `grouped_light` gesteuert.

Praxisverhalten:
- KNX->Hue-Kommandos (Schalten/Dimmen/CT/RGB) werden auf das Gruppen-Ziel gesendet.
- Status-KOs werden nach erfolgreichen Kommandos aktualisiert.
- Externe Änderungen (z. B. Hue App) kommen über den Eventstream der Bridge, ohne Eventstream über das Polling.
- **Status Schalten** ist „Ein", sobald mindestens eine Lampe des Raums bzw. der Zone eingeschaltet ist.
- **Status Helligkeit** ist der Mittelwert der eingeschalteten Lampen des Raums bzw. der Zone; Lampen ohne Dimmfunktion zählen nicht mit. Die Bridge selbst liefert für Räume und Zonen keinen verwertbaren Helligkeitswert, deshalb berechnet das Gateway ihn wie Home Assistant. Ändert sich eine einzelne Lampe, wird der Wert kurz darauf neu berechnet.
- Nach einem Befehl liest das Gateway den Raum bzw. die Zone erst nach dem Ende der Überblendung nach (längere der beiden Zeiten aus Einschalt- und Ausschaltgeschwindigkeit plus 1,5 s). Die Bridge meldet während der Überblendung Zwischenstände, die bis dahin ignoriert werden. **Status Schalten** kommt sofort mit dem Befehl, **Status Helligkeit** nach dem Ende der Überblendung.
- Beim Einschalten über KNX sendet das Gateway die zuletzt bekannte Helligkeit des Raums bzw. der Zone mit. Die Bridge setzt dabei alle Lampen auf diesen Wert; unterschiedliche Helligkeiten einzelner Lampen (z. B. aus einer Hue-Szene) gehen verloren. Ohne diesen Wert würde die Bridge die Lampen nach einem Ausschalten mit Überblendung mit etwa 1 % einschalten.

Hinweis:
- In bestimmten Konstellationen kann kein exakter physischer Gruppen-Istzustand aller Mitglieder abgebildet werden.
- Gehört eine Lampe zu mehreren Zonen, wird sie bei der Helligkeit nur einer dieser Zonen zugerechnet.
- Für streng deterministische Rückmeldung den gewünschten Sync-/Polling-Modus gezielt testen.

