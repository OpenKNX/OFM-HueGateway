# OFM-HueGatewayModule

OpenKNX Funktionsmodul zur Integration von Philips Hue Geräten in KNX-Systeme.

## Status

🧪 **Beta**

### Bekannte Einschränkungen

- RGB/Farbtemperatur-Funktionen hängen von den Fähigkeiten der gewählten Hue-Leuchte ab.
- Der Polling-Fallback ist funktional, liefert aber keine Echtzeit-Updates wie der Event-Stream.
- Die Stabilität hängt von einem korrekt funktionierenden lokalen Netzwerk (mDNS/HTTP) ab.

## Features

- Automatische Hue Bridge Discovery (mDNS)
- Button-Press Authentifizierung
- Lampen & LED Stripes
  - Schalten (Ein/Aus)
  - Dimmen (0-100%)
  - Farbtemperatur (2000-6500K)
  - RGB Farben
- Zieltyp Raum/Zone (`grouped_light`) zusätzlich zu einzelnen Leuchten
- Gerätetyp Steckdose (Schalten, eigene reduzierte Szenensteuerung)
- Gerätetyp Bewegungsmelder (Präsenz, optional Temperatur, Helligkeit/Lux, Batterie)
- Gerätetyp Kontaktsensor (Kontaktstatus, optional Sabotage, Batterie)
- Gerätetyp Taster/Schalter (bis zu 4 Tasten, Kurz-/Langdruck frei belegbar, Drehregler-Unterstützung)
- Szenensteuerung (DPT 18.001, bis zu 8 Slots je Kanal; Abruf und optionales Speichern; inkl. Abruf global konfigurierter Hue-Szenen)
- ETS-Seite „Inbetriebnahme": Bridge-Pairing starten, Hue-Geräte laden und Kanälen bzw. Szenen-Slots direkt in der ETS zuordnen (Tabellenwerte markier- und kopierbar)
- Kanalauswahl-Seite zur Aktivierung der Kanäle, analog zum OpenKNX-Standardmuster
- Webinterface für Pairing, Gerätescan und Diagnose (`/hue`)
- Bidirektionale Status-Updates (Event Stream)
- ETS-Konfiguration
- Anbindung an [OFM-LightManager](../OFM-LightManager) (HCL — Human Centric Lighting); die Anbindung selbst ist Pflicht, nur ihre Nutzung ist pro Kanal optional (siehe unten)
  - Partial-Sink-Vertrag: achsenweise Gültigkeitsmaske via `onLightManagerPartial()` — Kanäle mit `HCL-Achsen = Nur CT` senden keine Helligkeitswerte und umgekehrt

## Abhängigkeiten

- **OFM-LightManager ≥ 0.3** *(Pflichtabhängigkeit zum Bauen — nicht optional)*
  - Formell definiert in `HueGatewayModule.share.xml` per
    `<op:minModuleVersion prefix="LMG" dependency="forceUsage" value="0.3" />`;
    der OpenKNXproducer (≥ 4.0.1) bricht den Build ab, wenn das Modul fehlt
    oder älter ist. Ab 0.3 gibt es den Partial-Sink-Vertrag
    (`onLightManagerPartial`) und die KO-Blockgröße, auf die HUE aufbaut.
  - Sowohl der C++-Code (`openknxLightManagerModule`, siehe `HueGatewayModule.cpp`)
    als auch die ETS-XML (`ParameterType` `PT-LMGMasterSelect`, siehe
    `HueGatewayModule.share.xml`) setzen `OFM-LightManager` unbedingt voraus.
    Ohne dieses Modul im Build bzw. im kombinierten ETS-Projekt lässt sich
    `OFM-HueGatewayModule` weder kompilieren noch zu einem gültigen
    `.knxprod` zusammenbauen; `library.json` listet die Abhängigkeit daher
    auch unbedingt (`"OFM-LightManager": "*"`).
  - Liefert die HCL-Master (Brightness/ColorTemperature-Verläufe); optional
    ist lediglich deren *Nutzung* je Kanal, nicht die Modulabhängigkeit selbst.
  - Stellt den gemeinsamen ETS-`ParameterType` `PT-LMGMasterSelect` bereit,
    der von der HueGateway-Kanalkonfiguration referenziert wird
    (Dropdown *„Zuordnung Lichtmanager"*).
  - Pro Kanal wird über `ParamHUE_CHHCLMaster` ein Master (1..N) ausgewählt;
    `0` deaktiviert lediglich die HCL-Kopplung für diesen Kanal, ändert aber
    nichts an der Pflichtabhängigkeit des Moduls selbst.
  - Integrationsdetails siehe `OFM-LightManager/doc/integration.md`.

## Development

```bash
git clone https://github.com/OpenKNX/OFM-HueGateway OFM-HueGatewayModule
cd OFM-HueGatewayModule
pio run
```

## ETS Runtime-Verhalten

- `SyncDir` steuert die Datenrichtung pro Kanal:
  - `0` = nur KNX → Hue
  - `1` = nur Hue → KNX
  - `2` = bidirektional
- `PollInterval` gilt pro Kanal für Hue → KNX Status-Übernahme:
  - `0` = deaktiviert (kein Polling auf diesem Kanal)
  - `>0` = Pollingintervall in Sekunden
- `MinBrightness` wird bei KNX → Hue Helligkeitswerten erzwungen (Switch-On, absolut, relativ), solange der Wert > 0 ist.

## Runtime-Feldtest

1. Vorbereitung
  - Einen Hue-Kanal aktivieren und einer realen Leuchte zuordnen (UUID gesetzt).
  - Für den Test dieselbe Leuchte in der Hue App sichtbar lassen.

2. `SyncDir = 0` (nur KNX → Hue)
  - ETS senden: Schalten, Helligkeit, Dimmen.
  - Erwartung: Leuchte reagiert in Hue.
  - Änderung in Hue App (manuell):
  - Erwartung: keine Status-Rückmeldung nach KNX (Status-KOs bleiben unverändert).

3. `SyncDir = 1` (nur Hue → KNX)
  - ETS senden: Schalten/Helligkeit.
  - Erwartung: wird ignoriert (keine Änderung an der Leuchte).
  - Änderung in Hue App (manuell):
  - Erwartung: Status-KOs werden gemäß `PollInterval` aktualisiert.

4. `SyncDir = 2` (bidirektional)
  - ETS und Hue App abwechselnd verwenden.
  - Erwartung: beide Richtungen funktionieren.

5. `PollInterval`
  - `PollInterval = 0`:
  - Erwartung: Hue → KNX Polling für diesen Kanal deaktiviert.
  - `PollInterval = 5`:
  - Erwartung: Hue-seitige Änderungen erscheinen innerhalb von ca. 5-10s in KNX.

6. `MinBrightness`
  - `MinBrightness = 30` setzen.
  - ETS Helligkeit auf 1..29% senden.
  - Erwartung: Leuchte fährt mindestens auf ~30%.
  - ETS Helligkeit = 0%:
  - Erwartung: Leuchte AUS (MinBrightness greift nicht bei AUS).

## P3 Verhalten (Polling-Scheduler)

- Das Modul prüft Hue→KNX in 1s-Ticks, damit auch kleine `PollInterval`-Werte korrekt aufgelöst werden.
- Der Abruf von `/clip/v2/resource/light` erfolgt nur, wenn mindestens ein Kanal wirklich fällig ist.
- Damit sinkt unnötige Last auf der Hue Bridge bei deaktivierten oder langen Kanal-Intervallen.

## Inbetriebnahme: Log-Cheat-Sheet

### 1) Discovery (Bridge finden)

- `Commissioning: Discovery start`  
  Start der Bridge-Suche.
- `Using saved IP: ...` + `Reachability check passed`  
  Gespeicherte Bridge-IP ist gültig und erreichbar.
- `No Hue Bridge found via mDNS` + `Falling back to N-UPnP`  
  mDNS erfolglos, Cloud-Discovery wird versucht.
- `N-UPnP HTTP begin failed (TLS/connection)`  
  TLS/Netzwerkproblem beim Discovery-Endpunkt.
- `Commissioning: Discovery failed`  
  Keine Bridge gefunden.

### 2) Pairing/Auth (erstes Koppeln)

- `No stored App-Key found, entering pairing workflow`  
  Erstinbetriebnahme / kein Key in NVS.
- `Awaiting Hue Bridge link button...`  
  Modul wartet auf Button an der Hue Bridge.
- `Pairing attempt at t=... (remaining=...)`  
  Zyklische Pairing-Versuche laufen.
- `Bridge reports button not pressed yet (error 101)`  
  Erwartetes Verhalten, Button noch nicht gedrückt.
- `Authentication successful`  
  App-Key wurde akzeptiert.
- `Authentication timeout after ...`  
  Pairingfenster abgelaufen.

### 3) Laufbetrieb (nach dem Pairing)

- `EventStream initial start: ok`  
  Echtzeit-Events aktiv.
- `EventStream initial start: failed (fallback polling active)`  
  EventStream fehlgeschlagen, Polling übernimmt.
- `Polling fallback active (EventStream disconnected)`  
  Fallback aktiv.
- `EventStream active, polling fallback suspended`  
  EventStream wieder aktiv, Polling-Fallback aus.
- `Event applied -> channel ...`  
  Hue-Event wurde auf KNX-Kanal gemappt und verarbeitet.

### 4) Typische Erstdiagnose bei Problemen

- Discovery scheitert sofort: Netzwerk/IP/Gateway prüfen.
- Pairing läuft, aber kein Erfolg: Bridge-Button innerhalb Fenster drücken.
- Auth erfolgreich, aber keine Updates: `SyncDir`, Kanal-UUID und Event-/Polling-Logs prüfen.

## 5-Minuten Inbetriebnahme

1. Firmware starten und auf Log warten: `Commissioning: Bridge setup start`.
2. Prüfen, ob Discovery erfolgreich war (`Using saved IP` oder `Found via mDNS/N-UPnP`).
3. Falls kein Key vorhanden: auf `Awaiting Hue Bridge link button...` achten und Bridge-Button drücken.
4. Erfolg prüfen: `Authentication successful` und danach `HueGatewayClient ready`.
5. Laufmodus prüfen:
  - bevorzugt `EventStream initial start: ok`
  - alternativ `fallback polling active` (funktional, aber nicht Echtzeit)
6. End-to-End prüfen: Hue App ändern und auf `Event applied -> channel ...` bzw. Polling-Update im Log achten.

## Lizenz

GPL-3.0 (wie alle OpenKNX Module)

## Links

- OpenKNX: https://openknx.de
- Hue API: https://developers.meethue.com/develop/hue-api-v2/
