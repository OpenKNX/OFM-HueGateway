### Hue Szenen (global)

Bis zu **8 Hue-Szenen** können global (auf Modulebene) als RID-Referenz hinterlegt werden. Diese werden in der Szenensteuerung der Kanäle bei Aktion **"Hue Szene abrufen"** ausgewählt.

- **Hue Szene 1..8 (RID)**: Ressourcen-ID der Szene aus dem Hue-System

Ermittlung der Scene-RID:
- ETS: Seite **Inbetriebnahme** (Szene laden, Szenen-Slot wählen, übernehmen)
- Webinterface: `http://<IP-des-OpenKNX-Geräts>/hue/scan`
- Hue App → Szenen-Details (nicht immer direkt zugänglich)
- Hue API v2: `GET /clip/v2/resource/scene`

Hinweis: Eine Hue-Szene wird direkt über die Bridge aktiviert und kann beliebig viele Leuchten umfassen. Sie eignet sich für komplexe Beleuchtungseffekte, die nicht per ETS-Preset abgebildet werden können.

Wichtig: Eine Hue-Szene wird immer über den Szenen-Slot eines normalen Kanals mit gültigem Licht-, Raum- oder Zonenziel abgerufen (Aktion **"Hue Szene abrufen"**). Die Szenen-RID gehört ausschließlich in diese Liste — nicht als **Hue Ziel** eines Kanals.

