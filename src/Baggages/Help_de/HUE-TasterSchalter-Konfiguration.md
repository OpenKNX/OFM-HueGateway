### Taster/Schalter – Konfiguration

Bei Gerätetyp **Taster/Schalter** werden Tastenereignisse (Kurz-/Langdruck) vom Hue-System empfangen und als KNX-Telegramme auf den Bus gesendet.

#### Anzahl Tasten

Legt fest, wie viele Tasten des Hue-Geräts konfiguriert werden (1–4). Entsprechend viele Taste-N-Sektionen werden eingeblendet.

#### Native Hue Aktion

Steuert, ob das Hue-Gerät zusätzlich seine eigene Hue-Nativaktion ausführt, wenn eine Taste gedrückt wird:

- **Beibehalten**: Das Gerät führt seine native Hue-Aktion UND das KNX-Telegramm aus (Parallelausführung).
- **Deaktivieren**: Das Gerät führt nur das KNX-Telegramm aus – die native Hue-Aktion wird über die API unterdrückt.

Empfehlung: **Deaktivieren**, wenn die Hue-Leuchten vollständig über KNX gesteuert werden sollen, um Doppelreaktionen zu vermeiden.

#### Beispielkonfigurationen

#### 2-Tasten-Dimmer

- Taste 1: Kurzdruck = **Schalten (DPT 1.001)**, Langdruck = **Dimmen Start/Stop (DPT 3.007)**
- Taste 2: Kurzdruck = **Schalten (DPT 1.001)**, Langdruck = **Dimmen Start/Stop (DPT 3.007)**

Geeignet für kompakte Wandtaster mit Auf/Ab-Logik.

#### 4-Tasten-Szenentaster

- Taste 1–4: Kurzdruck = **Szenennummer (DPT 18.001)**, Langdruck = **Kein Langdruck**

Geeignet für Raumsteuerungen mit fester Szenenzuordnung wie `Arbeiten`, `Entspannen`, `Abend`, `Aus`.

#### Jalousie-Taster

- Kurzdruck = **Schritt/Stop (DPT 1.007)** (Richtung: Auf oder Ab)
- Langdruck = **Fahren (DPT 1.008)** (Richtung: Auf oder Ab)

Sinnvoll, wenn ein Hue-Taster nicht für Licht, sondern für eine KNX-Jalousie-Funktion genutzt werden soll.

