### Synchronisationsrichtung


1. **Nur KNX zu Hue**: Telegramme steuern Hue, Statusrückmeldungen aus Hue werden ignoriert. Sichtbar sind nur die Steuer-KOs.
2. **Nur Hue zu KNX**: KNX-Kommandos werden blockiert, Status wird aus Hue übernommen. Sichtbar sind die Status-KOs; das KO **Schalten** bleibt ebenfalls eingeblendet.
3. **Bidirektional**: KNX-Kommandos und Hue-Statusübernahme aktiv. Sichtbar sind Steuer- und Status-KOs. (Standardempfehlung)

Hinweis:
- Für relatives Dimmen gibt es kein separates KO `Status Dimmen`. Die Rückmeldung des aktuellen Dimmstands erfolgt über `Status Helligkeit`.

Latenz-Hinweis:
- `Nur KNX zu Hue`: direkte KNX-Steuerung, aber keine Hue-Rückmeldung.
- `Nur Hue zu KNX`: Statusänderungen werden nur mit Polling-/Abfrage-Latenz auf KNX sichtbar.
- `Bidirektional`: meist beste Alltagswahl; Änderungen aus App oder Direktbedienung erscheinen dennoch nicht instantan, sondern gemäß Abfrageintervall.

Empfehlung: Für einzelne Leuchten meist `Bidirektional`, für reine Statusobjekte oder Monitoring-Kanäle auch `Nur Hue zu KNX`.

