### Nächste Schritte nach dem Pairing

Nach erfolgreicher Kopplung (LED grün dauerhaft) erfolgt die eigentliche Gerätezuordnung:

1. **Hue-Geräte laden** öffnen: `http://<IP-des-OpenKNX-Geräts>/hue/scan`
2. In der Liste die gewünschten Ziele (Licht/Raum/Zone) inkl. ID erfassen.
3. In ETS je Kanal **Zieltyp** setzen und **Hue Ziel (Light-/Room-/Zone-ID)** eintragen.
4. Pro Kanal Lampentyp, Synchronisationsrichtung und Polling prüfen.
5. Download ausführen und Funktion testen (Schalten, ggf. Helligkeit/Farbtemperatur/RGB).

Empfehlungen für robuste Inbetriebnahme:
- Primär die jeweilige **ID (RID)** verwenden, nicht den Namen.
- Namen nur verwenden, wenn sie im Hue-System eindeutig sind.
- Optional können Präfixe genutzt werden: `room:<id|name>` bzw. `zone:<id|name>`.

Empfehlung: Erst mit 1-2 Kanälen testen, danach auf alle Kanäle ausrollen.

