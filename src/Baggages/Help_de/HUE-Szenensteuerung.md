### Szenensteuerung

Ermöglicht die Zuordnung von KNX-Szenen (DPT 18.001) zu Hue-Aktionen. Bis zu **8 Szenen-Slots** je Kanal stehen zur Verfügung.

#### Szenensteuerung aktivieren

Aktiviert die Szenensteuerung für den Kanal und blendet das Szenen-KO sowie die Szenen-Konfigurationsseite ein.

#### Szene speichern

Legt fest, ob das Szenen-KO auch Speicherbefehle (DPT 18.001, Bit 7 = 1) auswertet.

- **Deaktiviert**: Nur Abruf (DPT 17.001-kompatibel, Bit 7 wird ignoriert)
- **Aktiviert**: Abruf und Speichern (DPT 18.001); ein Speicherbefehl sichert den aktuellen Istzustand in den jeweiligen Slot

#### Szene A..H (Slots)

Jeder Slot kann unabhängig parametriert werden. Ist die **Szenennummer** auf `0` (inaktiv) gesetzt, wird der Slot ignoriert.

**Szenennummer**: KNX-Szenennummer 1..64 (entspricht Bit 0..5 im DPT, also KNX-intern 0..63)

**Aktion**: Legt fest, was beim Abruf dieser Szene passiert. Die verfügbaren Optionen hängen vom **Lampentyp** des Kanals ab.

**Helligkeit**: Prozentwert 0..100 % (wird bei Aktionen mit Helligkeit verwendet)

**Farbtemperatur**: Kelvin-Wert 2000..6500 K (wird bei Aktionen mit CT verwendet)

**Rot / Grün / Blau**: RGB-Werte 0..255 (werden bei Aktionen mit Farbe verwendet)

**Hue-Szene**: Referenz auf eine der 8 global konfigurierten Hue-Szenen (→ Abschnitt Hue Szenen (global)). Sichtbar nur bei Aktion „Hue Szene abrufen".

Die Hue-Szene wird über den Szenen-Slot dieses Kanals abgerufen. Der Kanal selbst muss dafür ein gültiges Licht-, Raum- oder Zonenziel haben; ein eigener Kanal mit der Szenen-ID als Ziel funktioniert nicht. Empfehlung: den Slot auf dem Raum- oder Zonen-Kanal der betroffenen Leuchten anlegen und eine Szenennummer wählen, die nicht schon für ETS-Presets verwendet wird.

#### Lichtmanager-Sperre bei Szenen

Wenn einem Kanal ein Lichtmanager zugeordnet ist, **sperrt ein Szenen-Abruf, der den Kanal einschaltet, automatisch die HCL-Kanalausgabe** für diesen Kanal. Damit behält das Licht nach dem Szenen-Abruf seinen Szenen-Wert, ohne dass der Lichtmanager ihn überschreibt. Szenen, die den Kanal ausschalten, setzen keine Sperre.

Die Sperre wird aufgehoben durch:
- **Aus-Befehl** per KNX-Schalten-KO → Sperre wird sofort zurückgesetzt
- Ablauf der konfigurierten Rückfallzeit (→ Abschnitt Sperre (kanal-spezifisch))
- Globales Entsperren per KO

#### Szene speichern (Laufzeit)

Wenn **Szene speichern** aktiviert ist und ein Speicherbefehl (DPT 18.001, Bit 7 = 1) empfangen wird, wird der aktuelle Istzustand des Kanals (Schaltzustand, Helligkeit, CT, RGB) persistent gespeichert. Beim nächsten Abruf dieser Szenennummer wird der gespeicherte Wert anstelle des ETS-Preset verwendet.

Hinweis: Gespeicherte Szenen werden im Flash des Geräts abgelegt und überleben einen Neustart.

#### DPT 18.001 kurz erklärt

DPT 18.001 erweitert den reinen Szenenabruf um eine Speicherfunktion:

- Bit 7 = `0`: Szene abrufen
- Bit 7 = `1`: Szene speichern
- Bits 0..5: Szenennummer `1..64`

Beispiel:

- GA `2/1/10` sendet `Szene 4 abrufen` -> HueGateway führt den konfigurierten Slot 4 aus.
- Dieselbe GA sendet `Szene 4 speichern` -> HueGateway speichert den aktuellen Istzustand in Slot 4, wenn `Szene speichern` aktiviert ist.

#### Empfehlung für die Praxis

- Für klassische Tastszenen denselben GA-Typ konsequent für Abruf und optionales Speichern verwenden.
- Bei Kanälen mit Lichtmanager prüfen, ob nach dem Szenenabruf eine Sperre oder Rückfallstrategie gewünscht ist.
- Für Steckdosen gibt es eine eigene reduzierte Szenensteuerung mit Ein-/Aus-Aktionen.

