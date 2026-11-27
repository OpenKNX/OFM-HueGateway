### Taste Kurzdruck

Bestimmt die Aktion beim kurzen Tastendruck. Je nach Wahl werden zusätzliche Sub-Parameter eingeblendet:

| Option | DPT | Sub-Parameter |
|---|---|---|
| **Keine Aktion** | — | — |
| **Schalten** | DPT 1.001 | **Schaltwert**: Toggle / Ein / Aus |
| **Dimmen relativ** | DPT 3.007 | **Richtung** (Heller / Dunkler), **Schrittweite** (2–25 %) |
| **Szenennummer** | DPT 18.001 | **Szenennummer** (1..64) |
| **Schritt/Stop** | DPT 1.007 | **Richtung** (Auf / Ab) |
| **Prozentwert** | DPT 5.001 | **Prozentwert** (0..100 %) |
| **Temperaturwert** | DPT 9.001 | **Temperatur** (5..40 °C) |
| **1-Byte Wert** | DPT 5.010 | **Wert** (0..255) |
| **2-Byte Wert** | DPT 7.001 | **Wert** (0..65535) |

Hinweis: Bei **Keine Aktion** wird für diese Taste kein Kurzdruck-KO in ETS eingeblendet.

