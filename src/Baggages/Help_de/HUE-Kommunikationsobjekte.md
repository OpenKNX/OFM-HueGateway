### Kommunikationsobjekte

### Übersicht

Die folgenden Tabellen fassen alle Kommunikationsobjekte zusammen. Details zu Sichtbarkeit und Verhalten stehen in den jeweiligen Abschnitten weiter unten.

#### Globale Kommunikationsobjekte

| KO | DPT | Bezeichnung | Erklärung |
|---:|---|---|---|
| 0 | 1.001 | Bridge Verbindungsstatus | Optionales Statusobjekt (0=offline, 1=online) |
| 1 | 1.017 | Pairing Trigger | Eingang zum Auslösen des ETS-gestützten Pairings |
| (LMG) | 1.003 / 1.011 | Sperre (global) / Status Sperre | Globale Lichtmanager-Sperre inkl. Statusrückmeldung |
| (LMG) | 1.017 | Entsperren Trigger | Hebt globale, manager- und kanal-spezifische Sperren gleichzeitig auf |
| (LMG) | 1.003 / 1.011 | Sperre Lichtmanager 1..8 / Status | Sperrobjekte je Lichtmanager; Sichtbarkeit abhängig von der Anzahl konfigurierter Lichtmanager |
| (LMG) | 5.001 / 7.600 | Lichtmanager Status Helligkeit Soll / Farbtemperatur Soll | Je Lichtmanager zwei Sollwert-KOs; Sichtbarkeit abhängig von Option „Status-KOs je Lichtmanager" |

(LMG) = Objekt des Moduls OFM-LightManager, das im kombinierten Gerät gemeinsam mit den Hue-spezifischen Objekten erscheint; KO-Nummer siehe dessen Applikationsbeschreibung.

#### Pro-Kanal Kommunikationsobjekte

Jeder Hue-Kanal belegt einen festen Block von 12 aufeinanderfolgenden KOs (Offset `+0`..`+11`, absolute Nummer abhängig vom Kanalindex). Je nach **Gerätetyp** werden dieselben Slots unterschiedlich belegt, teils mit anderem DPT. Welche KOs eingeblendet werden, steht in der Spalte Erklärung.

|  KO | Geräte-Typ | DPT | Bezeichnung | Erklärung |
|----:|---|---:|---|---|
|  +0 | Licht | 1.001 | Schalten | Eingang Ein/Aus. Bei jeder Synchronisationsrichtung eingeblendet |
|  +0 | Steckdose | 1.001 | Schalten | Eingang Ein/Aus |
|  +0 | Bewegungsmelder | 1.008 | Status Bewegung | Ausgang, 1 = Bewegung erkannt |
|  +0 | Kontaktsensor | 1.009 | Status Kontakt offen | Ausgang, 1 = offen, 0 = geschlossen |
|  +0 | Taster/Schalter | ¹ | Taste 1: Kurzdruck | Ausgang, wenn Kurzdruck ≠ *Keine Aktion* |
|  +1 | Licht | 5.001 | Helligkeit | Eingang 0..100 %. Ab Lampentyp *Dimmbar*, nicht bei *Nur Hue zu KNX* |
|  +1 | Taster/Schalter | ² | Taste 1: Langdruck | Ausgang, wenn Langdruck ≠ *Kein Langdruck* |
|  +2 | Licht | 3.007 | Dimmen | Eingang relatives Dimmen. Wie +1 |
|  +2 | Taster/Schalter | ¹ | Taste 2: Kurzdruck | Wie +0, ab 2 Tasten |
|  +3 | Licht | 1.001 | Status Schalten | Ausgang Ein/Aus. Ab Lampentyp *Dimmbar*, nicht bei *Nur KNX zu Hue* |
|  +3 | Steckdose | 1.001 | Status Schalten | Ausgang Ein/Aus |
|  +3 | Bewegungsmelder | 1.002 | Gerät erreichbar | Ausgang, 1 = erreichbar. Optional, siehe Gerät erreichbar verwenden |
|  +3 | Kontaktsensor | 1.001 | Sabotage | Ausgang, 1 = Sabotagekontakt ausgelöst. Optional, siehe Sabotage verwenden |
|  +3 | Taster/Schalter | ² | Taste 2: Langdruck | Wie +1, ab 2 Tasten |
|  +4 | Licht | 5.001 | Status Helligkeit | Ausgang 0..100 %. Wie +3 |
|  +4 | Bewegungsmelder, Kontaktsensor | 5.001 | Batterie | Ausgang Ladung 0..100 %. Optional, siehe Batterie verwenden |
|  +4 | Taster/Schalter | ¹ | Taste 3: Kurzdruck | Wie +0, ab 3 Tasten |
|  +5 | Licht | 7.600 | Farbtemperatur | Eingang Kelvin. Ab Lampentyp *Farbtemperatur*, nicht bei *Nur Hue zu KNX* |
|  +5 | Bewegungsmelder | 9.001 | Temperatur | Ausgang °C. Optional, siehe Temperatur verwenden |
|  +5 | Taster/Schalter | ² | Taste 3: Langdruck | Wie +1, ab 3 Tasten |
|  +6 | Licht | 7.600 | Status Farbtemperatur | Ausgang Kelvin. Ab Lampentyp *Farbtemperatur*, nicht bei *Nur KNX zu Hue* |
|  +6 | Bewegungsmelder | 9.004 | Helligkeit (Lux) | Ausgang Lux. Optional, siehe Helligkeit verwenden |
|  +6 | Taster/Schalter | ¹ | Taste 4: Kurzdruck | Wie +0, bei 4 Tasten |
|  +7 | Licht | 232.600 | Farbe RGB | Eingang RGB. Nur Lampentyp *Farbe RGB*, nicht bei *Nur Hue zu KNX* |
|  +7 | Taster/Schalter | ² | Taste 4: Langdruck | Wie +1, bei 4 Tasten |
|  +8 | Licht | 232.600 | Status RGB | Ausgang RGB. Nur Lampentyp *Farbe RGB*, nicht bei *Nur KNX zu Hue* |
|  +8 | Taster/Schalter | 3.007 / 5.001 / 3.007 / 7.600 | Drehregler | Ausgang, DPT je Funktion Dimmen / Wertgeber / Lautstärke / Farbtemperatur. Nur mit Drehregler |
|  +9 | Licht | 1.003 | Sperre Lichtmanager | Eingang, 1 = gesperrt. Ab Lampentyp *Dimmbar* mit Lichtmanager-Zuordnung |
|  +9 | Kontaktsensor, Steckdose | 1.002 | Gerät erreichbar | Ausgang, 1 = erreichbar. Optional, siehe Gerät erreichbar verwenden |
| +10 | Licht | 1.011 | Status Sperre Lichtmanager | Ausgang. Wie +9 |
| +11 | Licht, Steckdose | 18.001 / 17.001 | Szene | Eingang, nur mit Szenensteuerung. DPT 18.001 wenn *Szene speichern* aktiv, sonst 17.001 (nur Abruf) |

¹ Kurzdruck, DPT je Option: 1.001 Schalten, 3.007 Dimmen relativ, 18.001 Szenennummer, 1.007 Schritt/Stop, 5.001 Prozentwert, 9.001 Temperaturwert, 5.010 1-Byte Wert, 7.001 2-Byte Wert — siehe Taste Kurzdruck.

² Langdruck, DPT je Option: wie ¹, zusätzlich 1.008 Fahren — siehe Taste Langdruck.

Slots, die für einen Gerätetyp nicht aufgeführt sind, bleiben dort ungenutzt.

### Globale Kommunikationsobjekte

