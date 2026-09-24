### Hue-Ziel-Light-Room-Zone-ID-oder-Name

Primäres Zielfeld für alle Zieltypen. Akzeptiert sowohl eine numerische ID (RID) als auch einen eindeutigen Namen.

Verwenden Sie je nach Zieltyp:
- Light-ID oder Light-Name
- Room-ID oder Room-Name
- Zone-ID oder Zone-Name

Ermittlung über:
- ETS: Seite **Inbetriebnahme** (Hue-Geräte laden und zuordnen)
- Webinterface: `http://<IP-des-OpenKNX-Geräts>/hue/scan`
- Konsole: `hue scan`

Empfehlung:
- Für produktive Projekte bevorzugt die ID (RID) eintragen.
- Namen nur bei eindeutiger Benennung verwenden.

Hinweis: Die Scan-Seite listet Hue-Szenen gesondert auf. Deren IDs sind **keine** gültigen Ziele — ein Kanal mit einer Szenen-ID als Ziel wird nicht angelegt. Hue-Szenen werden über die Szenensteuerung eines Kanals abgerufen. Ein nicht auflösbares Ziel zeigt der Diagnosebericht (`/hue/diagnose.txt`) in der Kanalzeile mit `targetUnresolved=1`.
