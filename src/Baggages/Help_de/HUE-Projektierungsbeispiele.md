### Projektierungsbeispiele

### Beispiel 1: Schalten ohne Rückmeldung
- Lampentyp: Nur schalten
- Sync: Nur KNX zu Hue
- Polling: 0

### Beispiel 2: Standard-Wohnraum
- Lampentyp: Dimmbar
- Sync: Bidirektional
- Polling: 10 s

### Beispiel 3: Lichtmanager im Arbeitszimmer
- Lampentyp: Farbtemperatur
- Lichtmanager: 1
- Intervall Lichtmanager: 60 s
- Sperre Lichtmanager 1 via KO auf GA für Präsenz/Abwesenheit

### Beispiel 4: Raumsteuerung (Zone/Room) mit Rückmeldung
- Zieltyp: Raum
- Hue Ziel: Room-ID (RID)
- Lampentyp: Dimmbar oder höher
- Sync: Bidirektional
- Polling: 10 s

### Beispiel 5: Zone mit Farbtemperatur/RGB
- Zieltyp: Zone
- Hue Ziel: Zone-ID (RID)
- Lampentyp: Farbe RGB
- Sync: Bidirektional
- Polling: 5..15 s
- Hinweis: Wirkung abhängig von Fähigkeiten der enthaltenen Leuchten

### Beispiel 6: Lichtmanager mit Saison-Profil (Auto-DST)
- Lampentyp: Farbtemperatur
- Lichtmanager: 1
- Saison-Modus: Auto-DST
- Winter-Stützpunkte: SP1 `06:30 / 3000K / 30%`, SP2 `12:00 / 5000K / 80%`, SP3 `19:00 / 2700K / 60%`, SP4 `22:00 / 2200K / 20%`
- Sommer-Stützpunkte (SP1–SP4 jeweils Sommer Aktiv = Ja): SP1 `06:30 / 4000K / 40%`, SP2 `12:00 / 5500K / 90%`, SP3 `19:00 / 3800K / 70%`, SP4 `22:00 / 2700K / 25%`
- Hinweis: Im Sommer ist SP3 um 19 Uhr deutlich kühler (3800K statt 2700K), weil das Umgebungslicht noch hell ist.

### Beispiel 7: Saison-Umschaltung per KNX-Logik (Modus „Per Objekt")
- Saison-Modus: Per Objekt
- KO `LM 1: Sommer aktiv` mit Ausgang einer Logik verbinden, die aus Datum/Uhrzeit den Sommer erkennt
- Oder: KO an einen Taster hängen, der manuell zwischen Sommer/Winter umschaltet
- Vorteil: Vollständige externe Kontrolle; z. B. auch Zwischensaison-Profile möglich

