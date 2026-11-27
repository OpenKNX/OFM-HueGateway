### Partial-Sink-Anbindung (ab 0.7.0)

Die HCL-Engine des Lichtmanagers ruft pro Tick die Bridge-Implementierung `HueLightManagerBridge::onLightManagerPartial(masterNum, kelvin, brightness, validMask, fade)` auf. Die Maske `validMask` zeigt achsenweise an, welche Werte aktuell gültig sind:

- **Bit 0** = Kelvin valide (gesperrte oder per `StatusKoOutput`/`HclAxes` ausgeblendete Kelvin-Achse → 0)
- **Bit 1** = Brightness valide (analog)

Nicht-valide Achsen werden in der Bridge aus dem letzten gecachten Wert aufgefüllt, sodass die Hue-Leuchte einen vollständigen Sollwert erhält, ohne dass eine gesperrte Achse fälschlich auf einen Defaultwert „springt". Die Bridge respektiert die per Lichtmanager projektierten Achsen (`HclAxes`) und überträgt nur tatsächlich konfigurierte Achsen an die Hue-Bridge.

Konsistenz mit dem KNX-Bus: Der Hue-Pfad konsumiert `appliedKelvin()` und `effectiveBrightness()`, also die bereits geslewten, adaptierten und ggf. mit externen Quellen gemischten Werte — identisch zu dem, was K00/K01/K08 auf den Bus senden.

