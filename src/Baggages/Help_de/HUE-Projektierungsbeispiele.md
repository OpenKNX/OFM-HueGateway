### Projektierungsbeispiele

### Beispiel 1: Schalten ohne Rückmeldung
- Lampentyp: Ein/Aus (On/Off)
- Sync: Nur KNX zu Hue
- Polling: entfällt; bei „Nur KNX zu Hue" blendet die ETS das Polling-Intervall aus, weil keine Rückmeldung von der Bridge gelesen wird


### Beispiel 2: Standard-Wohnraum
- Lampentyp: Dimmbar (Dimmable)
- Sync: Bidirektional
- Polling: 10 s


### Beispiel 3: Lichtmanager im Arbeitszimmer
- Lampentyp: Farbtemperatur (White Ambiance)
- Zuordnung Lichtmanager: Lichtmanager 1
- Aktualisierungsintervall des Lichtmanagers: 60 s
- Sperre Lichtmanager 1 via KO auf GA für Präsenz/Abwesenheit


### Beispiel 4: Raumsteuerung (Zone/Room) mit Rückmeldung
- Zieltyp: Raum-ID
- Hue Ziel-ID: ID des Raums
- Lampentyp: Dimmbar (Dimmable) oder höher
- Sync: Bidirektional
- Polling: 10 s


### Beispiel 5: Zone mit Farbtemperatur/RGB
- Zieltyp: Zone-ID
- Hue Ziel-ID: ID der Zone
- Lampentyp: Farbe RGB (Color Ambiance)
- Sync: Bidirektional
- Polling: 5..15 s
- Hinweis: Wirkung abhängig von Fähigkeiten der enthaltenen Leuchten


### Beispiel 6: Lichtmanager mit Sommer- und Winter-Profil
- Lampentyp: Farbtemperatur (White Ambiance)
- Zuordnung Lichtmanager: Lichtmanager 1
- Lichtmanager 1: **Saisonale Umschaltung** `Automatisch (Astro + Offset)`, **Anzahl Profile** `2 Profile`
- Profil 1 „Winter", Saison `Winter`: SP1 `06:30 / 30 % / 3000 K`, SP2 `12:00 / 80 % / 5000 K`, SP3 `19:00 / 60 % / 2700 K`, SP4 `22:00 / 20 % / 2200 K`
- Profil 2 „Sommer", Saison `Sommer`: SP1 `06:30 / 40 % / 4000 K`, SP2 `12:00 / 90 % / 5500 K`, SP3 `19:00 / 70 % / 3800 K`, SP4 `22:00 / 25 % / 2700 K`
- Beide Profile gelten für alle Wochentage (**Gilt für** Mo–So).
- Hinweis: Im Sommer ist SP3 um 19 Uhr deutlich kühler (3800 K statt 2700 K), weil das Umgebungslicht noch hell ist.
- Details zu Profilen und Saison: Applikationsbeschreibung OFM-LightManager

### Beispiel 7: Saison-Umschaltung per KNX-Logik
- Lichtmanager 1: **Saisonale Umschaltung** `Kommunikationsobjekt`
- KO `LM 1: Eingang, Sommer=1` mit dem Ausgang einer Logik verbinden, die aus Datum/Uhrzeit den Sommer erkennt
- Oder: KO an einen Taster hängen, der manuell zwischen Sommer/Winter umschaltet
- Vorteil: Vollständige externe Kontrolle; z. B. auch Zwischensaison-Profile möglich

