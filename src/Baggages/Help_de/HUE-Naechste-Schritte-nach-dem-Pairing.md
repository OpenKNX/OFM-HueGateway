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
- Bei Namen, die für Licht, Raum und Zone gleich lauten, kann ein Präfix die Suche eingrenzen: `room:<name>` bzw. `zone:<name>`. Eine ID braucht kein Präfix, sie ist eindeutig (zudem passt `room:`/`zone:` plus ID nicht in das 40-Zeichen-Feld).

Empfehlung: Erst mit 1-2 Kanälen testen, danach auf alle Kanäle ausrollen.

