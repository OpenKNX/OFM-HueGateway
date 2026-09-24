### Inbetriebnahme

Auf dieser Seite wird die Hue-Bridge gekoppelt, die Hue-Geräte werden direkt aus der ETS von der Bridge geladen und den Kanälen bzw. Szenen-Slots zugeordnet. Das WebUI wird dafür nicht benötigt.

Voraussetzungen:
- Das Gerät ist mit dieser Applikation programmiert.
- Die ETS hat eine Busverbindung zum Gerät.

Ablauf:
1. **Pairing starten**: Startet die Kopplung mit der Bridge. Innerhalb des Pairing-Zeitfensters den Link-Button an der Hue-Bridge drücken; die ETS zeigt die verbleibende Zeit und meldet den Erfolg im Pairing-Status. Ist die Bridge bereits gekoppelt, entfällt dieser Schritt (der Button startet dann kein neues Pairing).
2. **Hue-Geräte laden**: Das Gerät fragt die Bridge ab und überträgt Lichter, Schalter/Sensoren/Steckdosen und Szenen in die Tabellen. Die Abfrage dauert einige Sekunden. Bereits in den Kanälen eingetragene Ziel-IDs werden erkannt und in den Spalten *Kanal*, *Steuern als* und *Szenen-Slot* vorbelegt.
3. **Zuordnen**: Pro Gerät den Hue-Kanal wählen. Bei Lichtern zusätzlich wählen, ob das Licht selbst, sein Raum oder seine Zone gesteuert wird. Szenen werden einem der 8 Szenen-Slots zugeordnet.
4. **Zuordnungen übernehmen**: Überträgt in jeden gewählten Kanal den Gerätetyp, bei Licht den Zieltyp und die Ziel-ID. Leere Kanalbeschreibungen erhalten den Hue-Namen. Szenen-IDs werden in die Szenen-Slots geschrieben, die Hue Szenen werden dabei aktiviert.

Das **Übernahme-Resultat** listet jeden geänderten Kanal und Slot auf. Ist ein Kanal oder Slot doppelt belegt, fehlt einem Licht der gewählte Raum bzw. die gewählte Zone oder ist ein Gerätetyp nicht zuordenbar, wird **nichts** übernommen und die Ursache angezeigt.

Hinweise:
- Die Tabellen werden nur im ETS-Projekt gespeichert und nicht in das Gerät geladen. Die übernommenen Werte werden erst mit dem nächsten Download wirksam.
- Eine abgewählte Zuordnung (Kanal auf „–“) löscht keinen Kanal; bestehende Kanäle bleiben unverändert.
- Es werden bis zu 64 Lichter, 32 Schalter/Sensoren/Steckdosen und 64 Szenen angezeigt. Größere Installationen bitte über das WebUI (`http://<IP-des-OpenKNX-Geräts>/hue/scan`) einrichten.
- Hue-Steckdosen erscheinen nur unter *Schalter, Sensoren und Steckdosen*.
