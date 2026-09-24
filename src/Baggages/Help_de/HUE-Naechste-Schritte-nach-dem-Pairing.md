### Nächste Schritte nach dem Pairing

Nach erfolgreicher Kopplung (LED grün dauerhaft) erfolgt die eigentliche Gerätezuordnung:

1. In der ETS die Seite **Inbetriebnahme** öffnen, **Hue-Geräte laden** ausführen, Kanäle zuordnen und **Zuordnungen übernehmen**.
   Alternativ im WebUI **Hue-Geräte laden** öffnen (`http://<IP-des-OpenKNX-Geräts>/hue/scan`) und je Kanal **Zieltyp** und **Hue Ziel (Light-/Room-/Zone-ID)** von Hand eintragen.
2. Pro Kanal Lampentyp, Synchronisationsrichtung und Polling prüfen.
3. Download ausführen und Funktion testen (Schalten, ggf. Helligkeit/Farbtemperatur/RGB).

Empfehlungen für robuste Inbetriebnahme:
- Primär die jeweilige **ID (RID)** verwenden, nicht den Namen.
- Namen nur verwenden, wenn sie im Hue-System eindeutig sind.
- Bei Namen, die für Licht, Raum und Zone gleich lauten, kann ein Präfix die Suche eingrenzen: `room:<name>` bzw. `zone:<name>`. Eine ID braucht kein Präfix, sie ist eindeutig (zudem passt `room:`/`zone:` plus ID nicht in das 40-Zeichen-Feld).

Empfehlung: Erst mit 1-2 Kanälen testen, danach auf alle Kanäle ausrollen.
