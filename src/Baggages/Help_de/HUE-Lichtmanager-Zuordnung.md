### Lichtmanager Zuordnung

Ordnet den Kanal einem Lichtmanager (1..N) zu.
Bei `Kein Lichtmanager` arbeitet der Kanal ohne automatische Sollwert-Übernahme.
Wirksam sind nur Lichtmanager, die im Modul Lichtmanager in der **Kanalauswahl** aktiviert und nicht suspendiert sind. Ist ein anderer Lichtmanager zugeordnet, arbeitet der Kanal wie bei `Kein Lichtmanager` und das Gateway meldet dies im Log.
Verfügbar ab Lampentyp `Dimmbar`. Dimmbare Lampen folgen nur der Helligkeit des Lichtmanagers; dessen HCL-Achsen dürfen dafür nicht auf `Nur Farbtemperatur` stehen.

