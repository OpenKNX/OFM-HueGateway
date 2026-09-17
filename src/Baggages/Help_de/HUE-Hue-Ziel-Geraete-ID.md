### Hue Ziel (Geräte-ID)

Zielfeld für Gerätetypen, die direkt auf ein einzelnes Hue-Gerät zeigen, insbesondere:

- Bewegungsmelder
- Taster/Schalter
- Kontaktsensor
- Steckdose

Verwenden Sie hier die von der Hue Bridge gemeldete Geräte-ID oder einen eindeutigen Gerätenamen.

Ermittlung über:
- Webinterface: `http://<IP-des-OpenKNX-Geräts>/hue/scan`
- Konsole: `hue scan`

Empfehlung:
- Für produktive Projekte bevorzugt die ID (RID) eintragen.
- Namen nur bei eindeutiger Benennung verwenden.

Hinweis:
- Im Unterschied zu **Hue Ziel (Light-/Room-/Zone-ID)** wird hier kein Raum/Zone-Ziel aufgelöst, sondern ein konkretes Hue-Gerät adressiert.

