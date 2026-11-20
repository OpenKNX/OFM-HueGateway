### Lichtmanager nach manueller Verstellung sperren

Versetzt den Lichtmanager für diesen Hue-Kanal automatisch in den Standby, sobald der Kanal von Hand verstellt wird.
Ohne diese Option muss die Sperre jedes Mal von Hand über das KO `Sperre Lichtmanager` gesetzt werden.

Sichtbarkeit:
- Ab Lampentyp `Dimmbar`.
- Nur wenn beim Kanal ein Lichtmanager `1..N` zugeordnet ist.

Optionen:
- **Nein**: Der Lichtmanager führt den Kanal nach einer manuellen Verstellung unverändert weiter nach.
- **Bei Helligkeit und Dimmen**: Ein Helligkeitswert (1 Byte) oder ein relativer Dimmbefehl (4 Bit) löst den Standby aus.
- **Bei Helligkeit, Dimmen, Farbtemperatur und Farbe**: Zusätzlich lösen Farbtemperatur- und Farbbefehle den Standby aus. Dimmbare Lampen haben keine Farbbefehle; dort wirkt diese Stufe wie **Bei Helligkeit und Dimmen**.

Ein **Schaltbefehl** löst bewusst keinen Standby aus: Ausschalten hebt die Sperre auf, und beim Einschalten soll der Kanal den aktuellen Lichtmanager-Wert übernehmen.

Rückkehr aus dem Standby: Es gelten dieselben Einstellungen wie für die manuelle Sperre, also **Rückfallstrategie nach Sperre** und die zugehörige Rückfallzeit bzw. -uhrzeit. Jede weitere manuelle Verstellung zieht die Rückfallzeit neu auf. Zusätzlich heben ein Aus-Befehl und das globale KO `Entsperren Trigger` die Sperre sofort auf.
Der Zustand wird über das KO `Status Sperre Lichtmanager` gemeldet.

**Wichtig:** Die KNX-Statusobjekte des Lichtmanagers (`LM x: Status-Soll-Helligkeit` bzw. `Status-Soll-Farbtemperatur`) dürfen nicht auf die Eingangsobjekte desselben Hue-Kanals gelegt werden. Bei aktivem Auto-Standby käme sonst jeder Zyklus des Lichtmanagers als vermeintlich manueller Befehl an und würde den Kanal dauerhaft selbst sperren. Bei interner Zuordnung über `Zuordnung Lichtmanager` besteht diese Gefahr nicht.

Praxisbeispiel:
- Im Wohnzimmer folgt das Licht tagsüber der Lichtmanager-Kurve.
- Am Abend wird per Taster gedimmt; der Kanal geht dadurch selbsttätig in den Standby und behält den eingestellten Wert.
- Nach Ablauf der konfigurierten Rückfallzeit — oder nach dem Ausschalten — folgt der Kanal wieder automatisch dem Lichtmanager.

