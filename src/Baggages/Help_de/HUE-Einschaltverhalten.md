### Einschaltverhalten

Globale Einstellung für die Übergangszeit bei Zustandswechseln aller Hue-Kanäle.

- **Einschaltgeschwindigkeit (Sekunden)**: Übergangszeit beim Wechsel von `Aus` nach `Ein`
- **Ausschaltgeschwindigkeit (Sekunden)**: Übergangszeit beim Wechsel von `Ein` nach `Aus`

Hinweise:
- Die Werte gelten für alle Kanäle (keine kanal-spezifische Einstellung).
- Die Einstellungen wirken sowohl mit als auch ohne aktive Lichtmanager-Zuordnung.
- Die Zeiten werden insbesondere beim Schalten sowie bei Helligkeitswerten verwendet, die ein automatisches Ein- oder Ausschalten auslösen.
- Reine Dimmänderungen ohne Zustandswechsel verwenden diese Parameter nicht automatisch.
- Standardwerte: `2 s` für Ein, `6 s` für Aus.
- Für typische Praxisanforderungen: Einschalten eher kurz, Ausschalten eher länger.
- Nach einem Ausschalten mit Überblendung merkt sich die Hue Bridge als letzte Helligkeit den Wert am Ende der Rampe, etwa 1 %. Beim Einschalten über KNX sendet das Gateway deshalb die zuletzt bekannte Helligkeit mit. Das Gateway speichert diesen Wert je Kanal im Flash, damit er auch nach einem Neustart oder Stromausfall gilt (bei Kanälen mit Lichtmanager nicht nötig, sie übernehmen dessen Wert). Ausschaltgeschwindigkeit `0 s` vermeidet die 1 % ganz, die Lampen gehen dann aber hart aus.

