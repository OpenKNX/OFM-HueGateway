# Changelog

Alle wesentlichen Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

## [0.7.5] - 2026-09-17 — Kanalauswahl nach OpenKNX-Standard

> **Breaking (ETS):** Der Gerätetyp je Kanal liegt jetzt in einem neuen Parameter mit dem Wert `Deaktiviert` an Position 0. Nach dem Applikations-Update steht jeder Kanal auf `Deaktiviert` und muss einmalig neu typisiert werden. Alle übrigen Kanaleinstellungen (Ziel, Lampentyp, Szenen, Taster, Lichtmanager) bleiben erhalten, das Speicherlayout ist unverändert.

### Geändert

- **Kanalauswahl als eigene Seite** (#13), direkt über dem ersten Kanal, mit je einer Zeile pro Kanal: Kanal, Gerätetyp, Beschreibung. Das Feld „Anzahl aktiver Kanäle" auf der Seite „Allgemein" entfällt; welche Kanäle sichtbar sind, ergibt sich allein aus dem Gerätetyp.
- **Deaktivierte Kanäle erscheinen nicht mehr im Baum.** Beim ersten Öffnen ist kein Kanal aktiv, statt bisher vier.
- **Einheitlicher Kanalkopf**: Beschreibung, Gerätetyp (ohne `Deaktiviert`, synchron zur Kanalauswahl) und `Suspendiert`. Der Gerätetyp wird über `BASE_SyncChannelType` aus OGM-Common abgeglichen.
- **„Kanal deaktivieren (zu Testzwecken)" heißt jetzt „Suspendiert"** (Nein/Ja, `PT-Suspended`) und steht für alle Gerätetypen zur Verfügung, bisher nur beim Gerätetyp Licht. Suspendierte Kanäle sind im Baum mit ⛔ markiert.
- „Bezeichnung" heißt jetzt „Beschreibung" (OpenKNX-Glossar).
- Firmware: Ein Kanal wird nur noch angelegt, wenn ein Gerätetyp gewählt und der Kanal nicht suspendiert ist. Telegramme an deaktivierte oder suspendierte Kanäle werden verworfen und lösen kein wiederholtes `setupDevices()` mehr aus.
- Kanalauswahl gegen das Muster in `OGM-Common/.claude/agents/openknx-channelselect.md` geprüft: Template-Blöcke heißen jetzt `Settings` und `Channel`, und die Trennlinie hinter der Überschrift „Kanaldefinition" entfällt. `VisibleChannels` bleibt als versteckte Konstante, wie es der Abschnitt „Änderungen share.xml" des Musters und die Module OFM-LogicModule und OFM-NetworkBridge vorsehen.

### Hinzugefügt
- VS-Code-Task „OpenKNXproducer Documentation" wird jetzt mitversioniert (`.vscode/tasks.json`), damit die Kontexthilfe überall gleich erzeugt wird (#14). Das doppelte Skript `createDoc.ps1` entfällt.

## [0.7.4] - 2026-09-16

### Behoben (Performance & Bridge-Kommunikation)
- **Jeder Schaltbefehl kostete einen vollen TLS-Handshake zur Bridge (~900 ms–1000 ms), auch wenn kurz zuvor schon ein Befehl lief.** `httpGet()` schaltete den gemeinsam genutzten `HTTPClient` per `useHTTP10(true)` dauerhaft auf HTTP/1.0 und damit `_reuse` ab — das betraf danach auch jeden PUT. Drei Lampen kurz hintereinander geschaltet brauchten dadurch gut 3 s. `httpGet()`/`httpPut()`/`httpDelete()` aktivieren Keep-Alive jetzt für ihre eigene Anfrage; eine offene Verbindung bleibt 15 s im Leerlauf bestehen (Wert von aiohttp/Home-Assistant übernommen, da die Bridge selbst keine öffentliche Angabe dazu macht) und wird danach automatisch geschlossen. Gemessen: erster Befehl einer Serie weiterhin ~900 ms, jeder folgende innerhalb der 15 s zwischen 50–150 ms. Eine Verbindung, die die Bridge zwischenzeitlich beendet hat, wird einmal automatisch neu aufgebaut und der Befehl wiederholt.
- **Polling-Abfragen (Lampen, Räume, Zonen, Geräte) blockierten die Hauptschleife aus demselben Grund mit bis zu ~850 ms pro Aufruf.** Laufen jetzt ebenfalls über eine wiederverwendete Verbindung: 16–30 ms statt ~850 ms, gemessen auf REG1-LAN-TP-Base.
- **Der EventStream verband sich auf dieser Hardware praktisch nie dauerhaft.** Jede HTTPS-Anfrage prüfte vorab den freien internen RAM und trennte den EventStream vorsorglich, sobald weniger als ~46 KB frei bzw. 32 KB am Stück verfügbar waren — was bei laufendem EventStream nahezu immer zutraf, weil dessen eigener TLS-Kontext genau diesen Speicher belegte. Die Trennung erfolgt jetzt nur noch, wenn eine Anfrage tatsächlich fehlgeschlagen ist, gefolgt von einem Wiederholversuch.
- **mbedTLS nahm seine Sitzungspuffer aus internem RAM,** wovon auf REG1-LAN-TP-Base nur für einen TLS-Kontext gleichzeitig Platz war — EventStream und ein Schaltbefehl konnten dadurch nie gleichzeitig bestehen. Ein zur Laufzeit eingehängter Allokator (`mbedtls_platform_set_calloc_free`) verlegt die Puffer ins PSRAM, ohne dass die Firmware-Toolchain dafür angepasst werden muss. Freier interner RAM stieg dadurch von ca. 23 KB auf ca. 60 KB, ohne messbaren Tempoverlust beim Handshake. Geräte ohne PSRAM verhalten sich unverändert.
- **Ein Schaltbefehl konnte mehrere Status-Telegramme auf den Bus bringen, obwohl sich nur ein Wert geändert hatte.** `GroupObject::value()` sendet unabhängig davon, ob der neue Wert vom alten abweicht. `sendStatusToKnx()` nutzt jetzt bei Leuchten, Sensoren, Kontakten und Steckdosen `valueCompare()`, das nur bei tatsächlicher Änderung sendet. Betraf u. a. wiederholte Schalt-/Helligkeits-/Farbtemperatur-Telegramme nach jedem Bestätigungs-Poll sowie bei Sensoren und Kontakten zyklisch wiederholte, unveränderte Temperatur-/Lux-/Batteriewerte.

### Geändert
- Schwelle für die Farbtemperatur-Nachführung durch den Lichtmanager von 12 K auf 20 K angehoben, um bei langsamen HCL-Verläufen nicht bei jeder für das Auge nicht wahrnehmbaren Kelvin-Änderung einen Bridge-Schreibzugriff auszulösen.
- Abhängigkeit zu OFM-LightManager formell definiert (#6): `op:minModuleVersion prefix="LMG" dependency="forceUsage" value="0.3"` und `op:minOpenKNXproducerVersion value="4.0.1"` in `HueGatewayModule.share.xml`. Der Producer bricht den Build nun mit klarer Meldung ab, wenn OFM-LightManager fehlt oder älter als 0.3 ist, statt erst beim unauflösbaren `PT-LMGMasterSelect`.
- ETS prüft Eingaben in den Hue-ID-Feldern per RegEx (#7): **Hue Lampen-ID (UUID)** akzeptiert nur noch leer oder eine UUID im Format `8-4-4-4-12` (Hex). **Hue Ziel-ID** akzeptiert weiterhin UUIDs, Namen und die Präfixe `room:`/`zone:`, lehnt aber führende/abschließende Leerzeichen ab. Beide Felder haben dafür eigene ParameterTypes (`PT-HUELightUUID`, `PT-HUETargetRID`) statt des gemeinsam genutzten `PT-Text40Byte`.

### Hinzugefügt
- Diagnose-Log-Zeilen `TIMING GET`/`TIMING PUT` (Dauer, Verbindungswiederverwendung, freier interner RAM je Anfrage) sowie `TIMING idle close` beim Schließen einer Leerlaufverbindung — zur Fehlersuche bei Bridge-Kommunikation über die Gerätekonsole abrufbar.

### Dokumentation
- Applikationsbeschreibung um einen Abschnitt „ETS Konfiguration" ergänzt, der die tatsächliche Seiten-/Tab-Struktur der ETS-Applikation als verschachtelten Baum mit Links auf die Detailbeschreibungen zeigt (#9).
- Kommunikationsobjekte als Tabellen zusammengefasst (global und pro Kanal, mit KO-Nummer/Offset und DPT je Objekt), verlinkt zu den bestehenden Detailabschnitten (#10, #11).
- Sperr- und Sollwert-Objekte des Lichtmanagers in der KO-Übersicht als Objekte von OFM-LightManager gekennzeichnet, da sie nicht zu diesem Modul gehören.
- KO-Übersicht je Kanal als Tabelle mit Spalte „Geräte-Typ" für alle fünf Gerätetypen, inkl. abweichender DPTs und Einblendbedingungen (#10). Die Belegung ist aus den `ComObjectRef`-Sichtbarkeitsregeln der ETS-Applikation abgeleitet.
- Doku an das tatsächliche ETS-Verhalten angeglichen: Lampentyp *Nur schalten* hat kein KO „Status Schalten"; bei *Nur Hue zu KNX* bleibt „Schalten" eingeblendet; Kontaktsensor ohne Temperatur-KO.
- Tote Links korrigiert: Verweis auf „Rückfallstrategie nach Sperre" zeigt jetzt auf die Applikationsbeschreibung von OFM-LightManager; LightManager-Links auf Branch `v1` statt des nicht existierenden `main`; Projektlink auf `OpenKNX/OFM-HueGateway`. Hilfetext „Kommunikationsobjekte" um die KO-Übersicht ergänzt.
- Tabellen in der Applikationsbeschreibung wiederhergestellt, die am 2026-05-10 in Aufzählungen umgewandelt worden waren (Gerätekategorien, unterstützte Hue- und Drittanbieter-Geräte, Friends of Hue, Gerätetyp, Taste Kurz-/Langdruck). Dabei korrigiert: überzählige Spalte in der Zeile „Licht", Smart-Plug-Archetype `plug`, Steckdose mit Szenen.
- Präfixe `room:`/`zone:` für die Hue Ziel-ID nur noch für Namen empfohlen: Eine ID ist eindeutig und braucht kein Präfix, und `room:`/`zone:` plus UUID (41 Zeichen) passt nicht in das 40-Zeichen-Feld.

### Entfernt
- Ungenutztes ETS-Scan-Skript `HueGatewayModule.script.js` entfernt (FunctionProperty-Aufruf `0xA0`). Es war seit dem ersten Commit in keiner XML-Seite verdrahtet und hatte im Firmware-Code nie einen passenden `0xA0`-Handler; die Scan-Funktion läuft seit Längerem über das Webinterface (`/hue/scan`) (#1).

## [0.7.3] — intern übersprungen, nie an Feldtester ausgeliefert

`library.json` trug diese Nummer kurz zwischen dem Release-Commit von [0.7.2]
und dem Versionssprung auf [0.7.4], aber es wurde nie ein Build unter 0.7.3
an Feldtester oder sonst jemanden ausgegeben. Die an Feldtester ausgelieferte
Version war [0.7.2] (siehe dort).

## [0.7.2] - 2026-09-16

### Behoben
- **Statusrückmeldung der kanal-spezifischen Lichtmanager-Sperre fehlte vollständig.** `publishHclChannelLockStatus()` übergab `HUE_KoCHHCLLockStatus` — den **kanalrelativen Index 10** — direkt an `knx.getGroupObject()` statt der absoluten KO-Nummer. Der Status ging damit an das globale Objekt 10 statt an `HUE_KoBlockOffset + Kanal × HUE_KoBlockSize + 10`. Die Sperre selbst arbeitete korrekt, nur die Rückmeldung erreichte das projektierte KO nie; zusätzlich wurde ein unbeteiligtes Objekt beschrieben. Die KO-Nummer wird jetzt analog zu `LightManagerModule::processInputKo` von Hand berechnet (der Makro `KoHUE_CHHCLLockStatus` ist im Modul-Scope nicht nutzbar, da er `_channelIndex` voraussetzt). Betraf alle Kanäle.
- **Überlappende Zonen: Der Lichtmanager schaltete ausgeschaltete Leuchten zyklisch wieder ein.** Hue meldet `grouped_light.on.on` bereits als `true`, sobald *irgendeine* Leuchte der Gruppe an ist. Der zyklische HCL-Abgleich lief dadurch weiter und sendete über `setGroupedLightState`/`setGroupedLightStateWithColorTemp` bedingungslos `on:true` mit — auf eine `grouped_light`-Ressource ist das ein Einschaltbefehl an **alle** Mitglieder, nicht bloß eine Statusbestätigung. Was eine überlappende Zone ausgeschaltet hatte, ging im Takt des Aktualisierungsintervalls wieder an. Die vier Schreibfunktionen kennen jetzt `includeOnState`; der Abgleich in `HueGatewayLight::loop()` ruft mit `false` auf. Regel: Eine zyklische Nachführung schaltet nie etwas ein, sie führt nur bereits eingeschaltete Leuchten nach — das schützt auch Einzellampen, die außerhalb von KNX ausgeschaltet wurden.
- **Szenen-Presets sperrten den Lichtmanager ohne Rückfall.** `processKnxSceneRecall()` setzte das Geräte-Flag direkt; die Modul-Schleife übernahm den Wert zwar, bewaffnete aber keinen Rückfall-Timer — die Sperre lief nie ab und endete nur durch Ausschalten. Presets laufen jetzt wie der Hue-Szenenaufruf über `setHclChannelLock()`, und zwar nur wenn die Szene einschaltet (eine ausschaltende Szene hinterließ sonst eine Sperre, die auch das nächste Einschalten blockierte).
- **Schreib-Dauerlauf bei herabgestuften Kanälen mit Lichtmanager.** `setupDevices()` stuft Farbtemperatur-/Farbkanäle auf „dimmbar" herab, wenn die Bridge keine CT-Unterstützung meldet, behielt dabei aber die Lichtmanager-Zuordnung. Weil `_currentKelvin` für dimmbare Lampen nie aktualisiert wird, galt die Farbtemperatur in `HueGatewayLight::loop()` dauerhaft als geändert — alle ~220 ms ein PUT an die Bridge plus KNX-Statustelegramm, und der gemeinsame HCL-Schreibslot blockierte alle anderen Lampen. Farbtemperatur zählt jetzt nur noch ab Lampentyp Farbtemperatur; dimmbare Lampen übernehmen nur die Helligkeitsachse. Kanäle vom Typ Ein/Aus verwerfen eine übrig gebliebene Lichtmanager-Zuordnung.
- **Überblendzeit des Lichtmanagers bei reiner Helligkeits-Nachführung ignoriert.** Die Nachführung ohne Farbtemperatur sendete mit Überblendzeit 0, die Helligkeit sprang.
- **Einschalten mit veraltetem Lichtmanager-Wert.** Beim Einschalten wurde der Lichtmanager-Wert auch angelegt, wenn keine gültige Achse vorlag; jetzt wie in der zyklischen Nachführung nur bei gültigem Wert.

### Hinzugefügt
- **Lichtmanager-Standby nach manueller Verstellung** (`CH%C%HCLAutoStandby`, je Hue-Kanal): `Nein` (Vorgabe, Verhalten unverändert), `Bei Helligkeit und Dimmen` oder `Bei Helligkeit, Dimmen, Farbtemperatur und Farbe`. Eine manuelle Verstellung versetzt den Lichtmanager für diesen Kanal selbsttätig in den Standby, sodass er nicht mehr von Hand gesperrt werden muss. Schaltbefehle lösen bewusst nicht aus. Die Rückkehr nutzt die bestehende **Rückfallstrategie nach Sperre** des Kanals; jede weitere Verstellung zieht die Rückfallzeit neu auf. Der Parameter belegt freie Bits einer vorhandenen Union, das Speicherlayout bleibt unverändert.
- `setHclChannelLock()` meldet den Status nur noch bei tatsächlicher Zustandsänderung, damit wiederholte Verstellungen keine redundanten Telegramme erzeugen.
- **Rampen-Diagnose** für Raum-/Zonen-Ziele: `/hue/fadetest?ch=<n>` bzw. Konsole `hue fadetest <n>` fährt einen Kanal durch vier Hue-Nutzlastvarianten (Ausschalten mit/ohne `dimming`-Zielwert, zweistufige Rampe, Einschalten aus dem Aus-Zustand) und tastet dabei alle 500 ms die Helligkeit ab. Damit lässt sich „rampt gleichmäßig" objektiv von „wartet und springt am Ende" unterscheiden. Nach jeder Variante wird zusätzlich ohne Helligkeitsangabe eingeschaltet und protokolliert, welchen Wert die Bridge wiederherstellt — inklusive Kontrollmessung nach der ersten Variante. Ergebnis im Diagnosebericht unter `[FadeTest]`. Ein zugeordneter Lichtmanager wird für die Testdauer automatisch gesperrt und danach wieder freigegeben.
- **Lichtmanager ab Lampentyp „Dimmbar"**: Zuordnung, Sperre, Auto-Standby und Rückfallstrategie stehen jetzt auch dimmbaren Lampen zur Verfügung. Diese folgen nur der Helligkeit; die ETS zeigt dazu einen Hinweis.
- Diagnosebericht weist ungelöste Kanalziele je Kanal aus (`targetUnresolved=` in der Kanalzeile).

### Geändert
- Diagnoseschlüssel `HasUnresolvedGroupTarget` heißt jetzt `HasUnresolvedTarget` — er galt schon immer für alle Zieltypen, nicht nur für Räume und Zonen.

### Dokumentation
- Hue-Szenen: Der Slot muss auf einem Kanal mit gültigem Ziel liegen; Szenen-IDs sind keine Ziel-IDs.
- Automatische Lichtmanager-Sperre präzisiert: Nur Szenen, die den Kanal einschalten, sperren.
- Fehlersuche um nicht auflösbare Ziele und deren Auswirkung auf die Live-Rückmeldung ergänzt.
- Veraltete Pfade `/openknx/hue/…` auf `/hue/…` korrigiert.
- Abschnitt „Lichtmanager nach manueller Verstellung sperren" in die Applikationsbeschreibung übernommen, aus der die Kontexthilfe erzeugt wird.

## [0.7.1] - 2026-06-15

### Behoben (Helligkeit & Status)
- **Fortschreitendes Abdimmen bei wiederholten Schaltzyklen.** Während des Ausschalt-Fade-outs sendet die Hue-Bridge mehrere `on=true`-Frames mit fallender Helligkeit. Diese Zwischenwerte wurden fälschlich als neue Zielhelligkeit in den Cache übernommen. Ein neues `_switchOffPending`-Flag (gesetzt von `processKnxSwitch(false)`, gelöscht sobald die Bridge `on=false` bestätigt) unterdrückt Cache-Updates während der kommandierten Überblendung.
- **Lampe startet nach Neustart/Neuprogrammierung auf ~1 %.** `setLightState`/`setGroupedLightState`/`setLightStateWithColorTemp`/`setGroupedLightStateWithColorTemp` sendeten bei unbekannter Helligkeit aktiv `dimming: 0 %` an die Bridge, die das als Minimum (~1 %) interpretiert. Das `dimming`-Feld wird jetzt bei `brightness == 0` weggelassen → die Bridge stellt ihren eigenen zuletzt bekannten An-Wert wieder her.
- **Aus-Zustands-Poll vergiftete die zwischengespeicherte Helligkeit.** Die Bridge meldet ihren gespeicherten Dimmwert auch im Aus-Zustand. `updateFromHue` aktualisiert `_brightness` jetzt nur noch, wenn die Bridge die Lampe als **an** (`on == true`) meldet, sodass ein Aus-Zustands-Poll den Wert nicht mehr überschreibt und das nächste Einschalten ihn nicht abspielt.
- **Helligkeits-Status blieb bei manuell gesteuerten Leuchten auf 0 % stehen.** Nach einem reinen Ein-Befehl liefert der EventStream nur ein `on`-Event ohne `dimming`-Feld, sodass der Gateway die tatsächliche Helligkeit nicht erfuhr. `refreshLightStatus(bool fastTrackOnly)` führt den Fast-Track-Verifikationspoll (mit vollem Light-Fetch inkl. Helligkeit) jetzt auch bei aktivem EventStream aus — aber nur für Kanäle mit kürzlich abgesetztem Befehl und ohne die regulären Intervall-Polls anzustoßen. `sendStatusToKnx` überschreibt das Helligkeits-KO nicht mehr mit 0 %, solange der echte Wert noch unbekannt ist (Ein-Zustand, `_brightness == 0`).

### Hinzugefügt (HCL-Rückfallstrategie)
- Kanal-spezifische **Rückfallstrategie nach Lichtmanager-Sperre** (`CH%C%HCLFallbackPolicy`): Definierte Rückfallzeit, Freie Dauer, Freie Uhrzeit, Dauer oder Uhrzeit, Nur externes Entsperren, Deaktiviert.
- Zugehörige Parameter **Freie Rückfalldauer** (`CH%C%HCLFallbackDurationSec`, 0–65535 s) und **Rückfall-Uhrzeit** (`CH%C%HCLFallbackReleaseTime`, 0–1439 min), in der ETS abhängig von der gewählten Strategie eingeblendet.
- Laufzeit-Auswertung über `_hclChannelFallbackPolicy`/`_hclChannelFallbackDurationMs`/`_hclChannelFallbackReleaseMinuteOfDay` und `shouldReleaseByPolicyTime(...)`.

## [0.7.0] - 2026-05-22 — Partial-Sink-Integration

### Geändert (kompatibilitätsrelevant)
- Mindestversion **OFM-LightManager 0.3.0** (ProfileV2-HCL-Engine + Variante-E-Dispatch). KO-Block des Lichtmanagers wächst von 12 auf **22** KOs je Kanal (+10 KOs/Kanal) → alle HUE-KO-Nummern in diesem OAM verschieben sich um **+160** (16 Kanäle × 10 neue KOs). HUE-Gruppenadressen müssen in der ETS nach dem Update **neu verknüpft** werden.
- `LMG_KoOffset` und `LMG_KoSingleOffset` müssen im OAM um den neuen `LMG_KoBlockSize=22` neu berechnet werden.

### Hinzugefügt
- **`ILightManagerOutput::onLightManagerPartial(masterNum, kelvin, brightness, validMask, fade)`-Implementierung** für die `HueLightManagerBridge`. `validMask` Bit 0 = Kelvin valid, Bit 1 = Brightness valid; nicht-valide Achsen werden aus dem letzten Cache aufgefüllt. Bridge respektiert Achsen-Maske gemäß `HclAxes`/`StatusKoOutput` und überträgt nur projektierte Achsen an die Hue-Bridge.
- Konsistenz zwischen KNX-Bus- und Hue-Helligkeit: Bridge konsumiert `appliedKelvin()` / `effectiveBrightness()` (geslewt + adaptiert + ext-gemischt) statt des rohen HCL-Sollwerts.

## [0.6.0] - 2026-05-19

### Geändert (kompatibilitätsrelevant)
- Mindestversion **OFM-LightManager 0.2.0** (DPT 249.600 + per-Kanal-Timing). Der KO-Block des Lichtmanagers wächst um 4 KOs je Kanal → alle HUE-KO-Nummern in diesem OAM sind um **+64** verschoben (`KoOffset 533 → 597`, `KoSingleOffset 531 → 595`).
  - Alle HUE-Gruppenadressen müssen in der ETS nach dem Update **neu verknüpft** werden.
- Diagnostische Anzeigen für `Aktualisierungsintervall`/`Überblenddauer` (Konsole `hue`/`hue hcl`, Web-UI-Status) zeigen jetzt `pro Kanal`, da diese Werte nicht mehr global geführt werden.
- HCL-Throttle in `HueGatewayModule` verwendet die per-Kanal-Werte aus `LightManagerModule::channelUpdateIntervalSec(masterNum)` und `channelFadeDurationSec(masterNum)`.

## [0.5.0] - 2026-05-18

### Geändert (Architektur)
- HCL-Engine und Lichtmanager-Konfiguration in eigenständiges Modul **OFM-LightManagerModule** extrahiert (neue Abhängigkeit). Die Entwicklungsgeschichte aller HCL/LM-bezogenen Funktionen ist im dortigen CHANGELOG dokumentiert.
- Tote HCL-`ifdef`-Blöcke entfernt; HCL-Datenzugriff ausschließlich über `HCL::masterManager`.
- `share.xml` bereinigt (ApplicationVersion 22 → 23): überflüssige HCL-Typen und Stub-Datei `HueGatewayHCL.templ.xml` entfernt.
- `PT-HUEHCLMasterSelect` durch gemeinsamen `PT-LMGMasterSelect` aus OFM-LightManager ersetzt.
- Dynamische HCL-Master-Begrenzung gegen `LightManagerModule.getMasterCount()` zur Laufzeit.

## [0.4.1] - 2026-04-20

### Hinzugefügt
- Neue Baggage-Dateien: `HUE-Hue-Ziel-Geraete-ID.md`, `HUE-Hue-Ziel-Light-Room-Zone-ID.md`

### Geändert
- ETS-Taster-Parametrierung: Gewerk-basiertes Modell durch DPT-Typ-Dispatch ersetzt – Kurz- und Langdruck wählen direkt den gewünschten DPT (Schalten, Dimmen relativ, Szenennummer, Schritt/Stop, Prozentwert, Temperaturwert, 1-Byte-Wert, 2-Byte-Wert).
- Drehregler-Funktion: Optionen korrigiert auf Dimmen (DPT 3.007), Wertgeber (DPT 5.001), Lautstärke (DPT 3.007), Farbtemperatur (DPT 7.600); veraltete Option „Lamelle (DPT 5.001)" entfernt.
- Szenen-Slots A–H: Benennung von „Szene 1..8" auf „Szene A..H" vereinheitlicht.
- Baggage-Dateien überarbeitet: HUE-Drehregler, HUE-HCL-Manager-18, HUE-HCL-Sperre-global(-Status), HUE-Kanal, HUE-Lichtmanager-18, HUE-TasterSchalter-Konfiguration, HUE-Taste-Kurzdruck, HUE-Taste-Langdruck, HUE-Szenensteuerung, HUE-Inbetriebnahme-Checkliste, HUE-Projektierungsbeispiele, HUE-Haeufige-Fehler-und-Loesungen, HUE-Naechste-Schritte-nach-dem-Pairing, HUE-Hue-Lampen-ID-UUID, HUE-Hue-Ziel-Geraete-ID-oder-Name, HUE-Hue-Ziel-Light-Room-Zone-ID-oder-Name

### Entfernt (Baggages)
- `HUE-Taste-Gewerk.md` (Gewerk-Modell nicht mehr vorhanden)
- `HUE-HCL-Entsperren-Trigger.md`, `HUE-HCL-Manager-Auswahl.md`, `HUE-HCL-Manager-Zuordnung.md`, `HUE-HCL-Manager.md`
- `HUE-HCL-Sperre-Lichtmanager-18-Status.md`, `HUE-HCL-Sperre-Manager-14-Status.md`, `HUE-HCL-Sperre-Manager-18-Status.md`
- `HUE-HCL-Sperre-kanal-spezifisch.md`, `HUE-HCL-Sperre-kanal-spezifisch-Status-HCL-Sperre.md`
- `HUE-HCL-Status-Helligkeit-Soll-Farbtemperatur-Soll.md`, `HUE-Human-Centric-Lighting-HCL.md`
- `HUE-Rueckfallstrategie-nach-HCL-Sperre.md`, `HUE-Status-KOs-je-HCL-Manager.md`

### Dokumentation
- Applikationsbeschreibung überarbeitet: Taste-Kurzdruck/-Langdruck als vollständige DPT-Tabellen mit Sub-Parametern dokumentiert; Drehregler-Funktion korrigiert; Szenen A–H aktualisiert; Beispielkonfigurationen ohne veraltete Gewerk-Referenzen.

## [0.4.0] - 2026-04-17

### Hinzugefügt
- Dedizierte Szenen-Aktionen für Hue-Taster ergänzt.
- ETS-Tasteraktionen und Drehregler-Handling erweitert.
- behavior_instance wird jetzt automatisch via SSE-Event gelöscht (kein Neustart erforderlich).

### Geändert
- Replaced all `Serial.print*` calls in `HueGatewayAuth`, `HueGatewayClient`, and `HueGatewayButton` with OpenKNX logger (`logInfo`, `logDebug`, `logWarning`, `logError`). Debug output is now stripped in release builds and routed through the standard OpenKNX logging infrastructure.
- Taster-Modell von Gewerk-basiertem Dispatch auf DPT-Typ-Dispatch umgestellt.
- Jalousie: Langdruck sendet kein Stop-Telegramm beim Loslassen; Kurzdruck in „Auf/Stop" umbenannt.
- Dimm-Wiederholungsintervall präzisiert und Beta-Labels ergänzt.
- ETS-Sichtbarkeit für Taster-Parameter und HCL-Parameter überarbeitet.

### Geändert (KO-Benennung)
- ComObjectRef-Namen aller Kanal-Kommunikationsobjekte auf OpenKNX-Standard umgestellt: `"Hue %C% - {{0}}: …"` → `"{{0:Hue %C%}}: …"` (96 Objekte in templ.xml, 32 Objekte in share.xml).
- Alle ComObject-FunctionTexts auf OpenKNX-Richtungsformat vereinheitlicht: `"Modulname: Eingang/Ausgang[, Qualifier]"` – gilt für alle 12 Kanalobjekte (templ.xml) sowie alle globalen und LM-1–8-Objekte (share.xml).

### Geändert (Tester-Rückmeldung)
- Kommunikationsobjekte „Soll-Helligkeit" und „Soll-Farbtemperatur" des Lichtmanagers in „Status-Soll-Helligkeit" und „Status-Soll-Farbtemperatur" umbenannt, um den Ausgangscharakter der Objekte klar zu kennzeichnen (LM 1–8, global und kanalweise).
- DPT der Sperr-Eingangsobjekte von `DPST-1-1` (switch) auf `DPST-1-3` (disable/enable) geändert – gilt für globale LM-Sperre, LM 1–8 und kanalweise Sperre.
- DPT der Sperr-Status-Objekte von `DPST-1-1` (switch) auf `DPST-1-11` (state) geändert.

### Behoben
- HCL-Updates: EventStream-TLS-Connect erfolgt jetzt asynchron via FreeRTOS-Task auf Core 0. Der Arduino-Loop auf Core 1 blockiert nicht mehr während des TLS-Handshakes (~1-3s reduziert auf ~800ms).
- HCL-Updates: EventStream-Retry und Polling-Fallback werden während des aktiven HCL-Schreibfensters (220ms nach PUT-Abschluss) zurückgestellt, um unnötige Stop/Start-Zyklen bei mehreren aufeinanderfolgenden HCL-PUTs zu vermeiden.
- Polling-Fallback: Startet nicht mehr während eines laufenden async EventStream-Connects, verhindert TLS-Speicher-OOM-Kollision zwischen Core 0 und Core 1.
- HCL-Spacing-Gate: Wird jetzt nach dem blockierenden PUT gesetzt (statt davor), damit das volle 220ms-Fenster ab PUT-Abschluss gilt.
- Relatives Dimmen: Fehler-Cooldown erst nach 5 aufeinanderfolgenden HTTP-Fehlern (statt 3) und Dauer von 5 s auf 2 s verkürzt – verhindert mehrsekundige Dimmstopps bei transienten Bridge-Fehlern.
- HCL-Update-Intervall für KNX-Bus-Sends wird nun korrekt berücksichtigt.
- Jalousie-DPT-Mapping in ETS-Tasterobjekten korrigiert.
- DPT 3.007 Dimm-Schritte werden konsistent kodiert.
- Dimm-Button-Objekte für Licht-Kurzdruckaktionen sichtbar gemacht.
- ETS-Parameter und Labels für gemeinsame Hue-Parameter verfeinert.
- Drehregler-Steuerelemente als Beta markiert.
- Langdruck- und Jalousie-Mapping für Taster verfeinert.
- Runtime-Handling und Diagnoseausgaben gehärtet.

## [0.3.9] - 2026-03-23

### Hinzugefügt
- Neue Basisklasse `HueGatewayDevice` mit fünf konkreten Gerätetypen:
  - **Licht** (`HueGatewayLight`) – Schalten, Dimmen, CT, RGB
  - **Steckdose** (`HueGatewayPlug`) – Schalten; Erkennung über Archetype-Feld der Bridge
  - **Bewegungsmelder** (`HueGatewaySensor`) – Präsenz, optional Temperatur, Lux, Batterie
  - **Kontaktsensor** (`HueGatewayContact`) – Offen/Geschlossen, optional Manipulation, Batterie
  - **Taster/Schalter** (`HueGatewayButton`) – bis zu 4 Tasten, Kurz-/Langdruck, Drehregler
- Szenen-Engine: bis zu 8 Szenen pro Kanal (Schalten, Dimmen, CT, RGB); Recall und Store über KNX-Gruppenadresse.
- ETS-Parameter für Tasterfunktion, Invertierung, Szenen und Drehregler (`relative_rotary`).
- Konfigurierbares Dimm-Wiederholungsintervall `HUERelDimRepeatMs` (20–500 ms, Standard 120 ms).
- NativeHueAction-Parameter pro Kanal für Taster/Schalter.
- Accessory-Device-Scan (`getAccessoryDevices`, `HueGatewayAccessoryDevice`).
- HCL-Lock-Diagnose und Button-ETS-Mapping im SSE-Eventhandler.
- behavior_instance DELETE via Hue API v2.

### Geändert
- HCL-Lock wird automatisch freigegeben, wenn das Licht ausgeschaltet wird.
- SSE-Eventerkennung für Button, Motion, Contact und Rotary auf Service-RID-Auflösung umgestellt.

### Behoben
- Taster-Dispatch: Jalousie-DPT-Tausch, `hasLong`/`hasShort`-Logik und `invertLong` korrigiert.
- UIHint `CheckBox` auf `TypeRestriction` ist in ETS ungültig – auf `DropDown` geändert.

## [0.3.5] - 2026-03-18

### Hinzugefügt
- Transition-Getter in `HueGatewayLight` und verbessertes Switch-Logging.

### Geändert
- ETS-Hilfetexte und Applikationsbeschreibung vollständig überarbeitet.

### Behoben
- DPT 3.007 Halten/Loslassen stabilisiert; Statusrückmeldung verbessert.
- Hue API sendet bei AUS-Befehlen keine überflüssigen Dimm- und CT-Werte mehr.
- Grouped-Light-Status und Helligkeits-Feedback präzisiert.
- RAM-Verbrauch reduziert; Diagnose-Ringpuffer auf sinnvolle Tiefe begrenzt.

## [0.3.2] - 2026-03-04

### Hinzugefügt
- Event-Stream-Absicherung und Setup-Leitplanken für stabilen Langzeitbetrieb.
- Verbesserte KNX-Write-Trace-Telemetrie für Diagnose.

### Behoben
- Channel-Mappings bleiben bei Bridge-Scan-Neustarts erhalten.
- Grouped-Light: letzter Helligkeitswert wird nach Verbindungsunterbrechung korrekt wiederhergestellt.
- ETS-Layout-Mappings und Schaltverhaltens-Hilfetext verfeinert.

## [0.3] - 2026-02-26

### Hinzugefügt
- Räume und Zonen als eigener Kanaltyp (`grouped_light`-Ressource der Hue Bridge); unterstützt Schalten, Dimmen, CT und RGB analog zu Einzelleuchten.
- Kanal-Template mit kumulativer KO-Sichtbarkeit und dynamischen KO-Labels.

### Geändert
- Versionszweig 0.3.x gestartet (Nachfolger von 0.2.x).

## [0.2.2] - 2026-02-26

### Hinzugefügt
- Globale ETS-Parameter für Schalt-Übergangszeiten (separates Ein- und Ausschalten).

### Geändert
- Übergangszeiten werden konsistent in der Runtime sowohl für HCL- als auch Nicht-HCL-Schaltwege angewendet.

## [0.2.1] - 2026-02-25

### Behoben
- Doppelter globaler HCL-Manager-Status-KO-Bereich in der ETS-Oberfläche entfernt.
- Build-Warnung für ungenutzte Variable im Channel-Lock-Status-Handler beseitigt.

## [0.2.0] - 2026-02-24

### Geändert
- Start des 0.2.x-Entwicklungszweigs für Raum-/Zonen-Unterstützung.
- Modulversion von 0.1.0 auf 0.2.0 erhöht.

## [0.1.0] - 2026-02-03

### Hinzugefügt
- Projektinitialisierung mit grundlegender Modulstruktur.
- Hue Bridge Discovery via mDNS; Button-Press-Authentifizierung ohne manuelle Token-Konfiguration.
- Bidirektionale Lichtkanal-Steuerung: Schalten, Dimmen (0–100 %), Farbtemperatur (2000–6500 K), RGB.
- Bidirektionale Statusaktualisierung via Hue API v2 Event Stream (SSE); Polling als Fallback.
- OFM-WebUI-Anbindung für kanalbasierte Hue-UUID-Konfiguration.
- TLS-Unterstützung für HTTPS-Kommunikation mit der Hue Bridge.
- NVS-Persistierung von Bridge-UUID und API-Key.

