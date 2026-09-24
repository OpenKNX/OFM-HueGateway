### Inbetriebnahme

Auf dieser Seite werden die Hue-Geräte direkt aus der ETS von der Bridge geladen und den Kanälen bzw. Szenen-Slots zugeordnet. Das WebUI wird dafür nicht benötigt.

Voraussetzungen:
- Das Gerät ist mit dieser Applikation programmiert.
- Die Hue-Bridge ist gepairt (LED grün dauerhaft).
- Die ETS hat eine Busverbindung zum Gerät.

Ablauf:
1. **Hue-Geräte laden**: Das Gerät fragt die Bridge ab und überträgt Leuchten, Schalter/Sensoren/Steckdosen und Szenen in die Tabellen. Die Abfrage dauert einige Sekunden. Bereits in den Kanälen eingetragene Ziel-IDs werden erkannt und in den Spalten *Kanal*, *Steuern als* und *Szenen-Slot* vorbelegt.
2. **Zuordnen**: Pro Gerät den Hue-Kanal wählen. Bei Leuchten zusätzlich wählen, ob die Leuchte selbst, ihr Raum oder ihre Zone gesteuert wird. Szenen werden einem der 8 Szenen-Slots zugeordnet.
3. **Zuordnungen übernehmen**: Überträgt in jeden gewählten Kanal den Gerätetyp, bei Licht den Zieltyp und die Ziel-ID. Leere Kanalbeschreibungen erhalten den Hue-Namen. Szenen-IDs werden in die Szenen-Slots geschrieben, die Hue Szenen werden dabei aktiviert.

Das **Übernahme-Resultat** listet jeden geänderten Kanal und Slot auf. Ist ein Kanal oder Slot doppelt belegt, fehlt einer Leuchte der gewählte Raum bzw. die gewählte Zone oder ist ein Gerätetyp nicht zuordenbar, wird **nichts** übernommen und die Ursache angezeigt.

Hinweise:
- Die Tabellen werden nur im ETS-Projekt gespeichert und nicht in das Gerät geladen. Die übernommenen Werte werden erst mit dem nächsten Download wirksam.
- Eine abgewählte Zuordnung (Kanal auf „–“) löscht keinen Kanal; bestehende Kanäle bleiben unverändert.
- Es werden bis zu 64 Leuchten, 32 Schalter/Sensoren/Steckdosen und 64 Szenen angezeigt. Größere Installationen bitte über das WebUI (`http://<IP-des-OpenKNX-Geräts>/hue/scan`) einrichten.
- Hue-Steckdosen erscheinen nur unter *Schalter, Sensoren und Steckdosen*.
