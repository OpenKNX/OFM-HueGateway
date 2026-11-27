### Polling-Intervall

Status-Abfrageintervall in Sekunden für den Fall, dass der Eventstream der Bridge nicht verfügbar ist.
Empfehlung: `5..30 s` je nach Kanalzahl und Netzlast.

So kommt der Status vom Hue-System auf KNX:
- **Normalbetrieb (Eventstream)**: Das Gateway hält eine Verbindung zum Eventstream der Bridge offen. Die Bridge meldet darüber jede Änderung von sich aus (Push), auch Änderungen aus der Hue-App oder von Hue-Schaltern. Es wird nicht zyklisch abgefragt.
- **Rückfallebene (Polling)**: Nur solange der Eventstream nicht verbunden ist, liest das Gateway die Kanäle zyklisch im hier eingestellten Intervall.
- **Nach KNX-Kommandos**: Das Gateway liest den betroffenen Kanal zusätzlich kurz nach (Fast-Track), weil der Eventstream z. B. nach einem einfachen Einschalten keine Helligkeit mitliefert.

Wichtig:
- `0` = zyklisches Polling für diesen Kanal deaktiviert. Fällt der Eventstream aus, wird dieser Kanal dann nicht mehr aktualisiert.
- Bei `>0` wird der Kanal in der Rückfallebene gemäß Intervall aus Hue gelesen (abhängig von Sync-Richtung).

Praxisempfehlungen:
- Einzelne Lampen: meist `5..15 s`
- Räume oder Zonen: meist `15..30 s`
- Viele aktive Kanäle: größere Werte wählen, um Bridge und Netzwerk zu entlasten

Zu kleine Werte bringen in großen Projekten oft keinen echten Mehrwert, erzeugen in der Rückfallebene aber mehr HTTP-Last auf der Hue-Bridge.

