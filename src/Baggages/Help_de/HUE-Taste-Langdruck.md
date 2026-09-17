### Taste Langdruck

Bestimmt die Aktion beim langen Tastendruck. Je nach Wahl werden zusätzliche Sub-Parameter eingeblendet:

| Option | DPT | Sub-Parameter |
|---|---|---|
| **Kein Langdruck** | — | — |
| **Schalten** | DPT 1.001 | **Schaltwert**: Ein / Aus |
| **Dimmen Start/Stop** | DPT 3.007 | **Richtung** (Heller / Dunkler), **Schrittweite** (2–25 %) |
| **Szenennummer** | DPT 18.001 | **Szenennummer** (1..64) |
| **Fahren** | DPT 1.008 | **Richtung** (Auf / Ab) |
| **Schritt/Stop** | DPT 1.007 | **Richtung** (Auf / Ab) |
| **Prozentwert** | DPT 5.001 | **Prozentwert** (0..100 %) |
| **Temperaturwert** | DPT 9.001 | **Temperatur** (5..40 °C) |
| **1-Byte Wert** | DPT 5.010 | **Wert** (0..255) |
| **2-Byte Wert** | DPT 7.001 | **Wert** (0..65535) |

Hinweis: Bei **Kein Langdruck** wird für diese Taste kein Langdruck-KO in ETS eingeblendet.

