### Korrektur nach weichem Ausschalten

Nur sichtbar, wenn die **Ausschaltgeschwindigkeit** größer als `0 s` ist. Standard: deaktiviert.

Wird eine Lampe über KNX mit Überblendung ausgeschaltet und danach über die Hue-App, einen Hue-Schalter, eine Hue-Automation oder einen Sprachassistenten eingeschaltet, geht sie mit den von der Bridge gemerkten etwa 1 % an. Das Gateway ist an diesem Einschalten nicht beteiligt.

- **Deaktiviert**: Die Lampe bleibt bei etwa 1 %. Das Gateway übernimmt diesen Wert auch als zuletzt bekannte Helligkeit, bis die Helligkeit geändert wird.
- **Aktiviert**: Meldet die Bridge danach eine Helligkeit von höchstens 2 %, schickt das Gateway sofort die zuletzt bekannte Helligkeit (bei Kanälen mit Lichtmanager dessen Wert) mit der Einschaltgeschwindigkeit hinterher. Die Lampe geht kurz mit etwa 1 % an und blendet dann hoch.

Hinweise:
- Gilt für alle Kanäle, auch für Räume und Zonen.
- Die Korrektur greift nur, wenn das letzte Ausschalten ein Ausschalten mit Überblendung über KNX war. Nach einem Neustart des Gateways ist diese Information verloren; das erste Einschalten von außen wird dann nicht korrigiert.
- Wer eine Lampe von außen bewusst mit 1 % einschaltet (z. B. Hue-Szene als Nachtlicht), bekommt nach einem weichen Ausschalten über KNX ebenfalls die zuletzt bekannte Helligkeit.

