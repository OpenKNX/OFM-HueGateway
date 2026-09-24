### Erstinbetriebnahme und Authentifizierung

Die Inbetriebnahme kann auf drei Arten erfolgen:

#### Variante 1: Pairing auf der ETS-Seite Inbetriebnahme (empfohlen)
. ETS-Download ausführen.. Auf der Seite **Inbetriebnahme** den Button **Pairing starten** drücken (Busverbindung erforderlich).. Innerhalb des Pairing-Fensters den Link-Button an der Hue Bridge drücken. Die ETS zeigt die verbleibende Zeit und meldet den Erfolg.. Das Modul speichert den App-Key persistent.
 die Bridge bereits gekoppelt, startet der Button kein neues Pairing. Für ein erneutes Pairing zuerst **Authentication zurücksetzen** oder eine der folgenden Varianten nutzen.

#### Variante 2: Pairing per ETS (Pairing Trigger KO)

1. ETS-Download ausführen.
2. Warten, bis das Modul die Bridge gefunden hat bzw. zur Bridge verbunden ist.
3. Auf das KO **Pairing Trigger** eine `1` senden.
4. Innerhalb des Pairing-Fensters den Link-Button an der Hue Bridge drücken.
5. Das Modul speichert den App-Key persistent.

#### Variante 3: Pairing per Webinterface

1. Webinterface öffnen: `http://<IP-des-OpenKNX-Geräts>/hue`
2. **Kopplung starten** aufrufen (`/hue/pair`).
3. Innerhalb des Pairing-Fensters den Link-Button an der Hue Bridge drücken.
4. Auf der Statusseite den Verbindungsaufbau prüfen (`/hue/status`).
5. Das Modul speichert den App-Key persistent.

Hinweise zu Zuständen und Fortschritt finden Sie unter LED-Signale.

