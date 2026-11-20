### Hue Lampen-ID (UUID)

Legacy-/Fallback-Feld für bestehende Projektierungen.

Für neue Projektierungen bitte **Hue Ziel (Light-/Room-/Zone-ID)** verwenden.

Gilt nur für Zieltyp **Licht**.

Hinweis:
- Dieses Feld dient primär der Migration älterer ETS-Projekte.

Ermittlung über:
- Webinterface: `http://<IP-des-OpenKNX-Geräts>/hue/scan`
- Konsole: `hue scan`
- Hue API v2 (`/clip/v2/resource/light`)

Wichtig: Die UUID muss je Kanal exakt zur gewünschten Leuchte passen.

