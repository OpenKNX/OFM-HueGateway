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



Die Tabellen:

| Tabelle | Spalten |
|---|---|
| Lichter | Name, Licht-ID, Raum, Raum-ID, Zone, Zone-ID, Kanal, Steuern als (Licht/Raum/Zone) |
| Schalter, Sensoren und Steckdosen | Name, Typ, ID, Kanal |
| Szenen | Name, Raum, Szenen-ID, Szenen-Slot |




### Räume und Zonen zuordnen

Räume und Zonen haben in der Tabelle *Lichter* **keine eigene Zeile**. Sie stehen als Spalten *Raum*, *Raum-ID*, *Zone* und *Zone-ID* bei jeder Lampe, die zu ihnen gehört.

Um einen Raum oder eine Zone auf einen Kanal zu legen:
1. Eine beliebige Lampe aus diesem Raum bzw. dieser Zone auswählen.
2. Bei dieser Lampe den Kanal wählen und *Steuern als* auf **Raum** bzw. **Zone** stellen.
3. Bei den übrigen Lampen desselben Raums bzw. derselben Zone den Kanal auf „–“ lassen.

Beim Übernehmen wird dann die Raum- bzw. Zonen-ID in den Kanal geschrieben, nicht die ID der Lampe. Im Beispiel oben steuert *Hue 2* über die Stehleuchte den ganzen Raum *Wohnzimmer* und *Hue 5* über die Leselampe die Zone *Erdgeschoss*.

Ein Raum oder eine Zone belegt nur einen Kanal, egal wie viele Lampen darin sind. Lampen, die immer gemeinsam geschaltet werden, sollten deshalb besser über ihren Raum oder ihre Zone angesteuert werden; das spart Kanäle.


Das **Übernahme-Resultat** listet jeden geänderten Kanal und Slot auf. Ist ein Kanal oder Slot doppelt belegt, fehlt einem Licht der gewählte Raum bzw. die gewählte Zone oder ist ein Gerätetyp nicht zuordenbar, wird **nichts** übernommen und die Ursache angezeigt. Wird ein Licht über einen eigenen Kanal und zusätzlich über seinen Raum bzw. seine Zone gesteuert, erscheint ein Hinweis; die Zuordnung wird trotzdem übernommen.


Hinweise:
- Die Tabellen werden nur im ETS-Projekt gespeichert und nicht in das Gerät geladen. Die übernommenen Werte werden erst mit dem nächsten Download wirksam.
- Namen und IDs in den Tabellen lassen sich markieren und kopieren (Strg+C), z. B. um eine ID von Hand in einen Kanal einzutragen. Änderungen in den Tabellen werden beim nächsten „Hue-Geräte laden" überschrieben; ID-Felder nehmen nur eine gültige Hue-ID an.
- Eine abgewählte Zuordnung (Kanal auf „–“) löscht keinen Kanal; bestehende Kanäle bleiben unverändert.
- Es werden bis zu 64 Lichter, 32 Schalter/Sensoren/Steckdosen und 64 Szenen angezeigt. Größere Installationen bitte über das WebUI (`http://<IP-des-OpenKNX-Geräts>/hue/scan`) einrichten.
- Hue-Steckdosen erscheinen nur unter *Schalter, Sensoren und Steckdosen*.
- Die Hue-Bridge selbst erscheint in der Tabelle *Schalter, Sensoren und Steckdosen* mit dem Typ *Sonstiges*, weil sie sich in der Hue API als Gerät meldet. Sie wird keinem Kanal zugeordnet und kann ignoriert werden.

