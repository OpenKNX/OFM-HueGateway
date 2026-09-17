#include "HueGatewayContact.h"
#include <knx.h>

void HueGatewayContact::sendStatusToKnx()
{
    if (!_initialized || _koContactStatus == 0)
        return;

    // DPST-1-9: DPT 1.009 — 0=geschlossen (contact), 1=geöffnet (no_contact)
    // valueCompare(): value() would send on every poll, not just on a change.
    knx.getGroupObject(_koContactStatus).valueCompare(_contactOpen, Dpt(1, 9));

    Serial.printf("[HueGatewayContact] %s - Status -> KNX: ContactOpen=%d\n",
                  _name.c_str(), _contactOpen ? 1 : 0);
}

void HueGatewayContact::sendOptionalKosToKnx()
{
    if (!_initialized) return;

    // valueCompare(): these run per poll cycle, so value() would repeat an unchanged
    // battery level or tamper state on the bus indefinitely.

    // KO3: Sabotage (DPST-1-1)
    if (_optTamper && _koTamper > 0)
        knx.getGroupObject(_koTamper).valueCompare(_tampered, Dpt(1, 1));

    // KO4: Batterie (DPST-5-1)
    if (_optBattery && _koBattery > 0 && _batteryPercent != 255)
        knx.getGroupObject(_koBattery).valueCompare(_batteryPercent, Dpt(5, 1));

    // KO9: Erreichbar (DPST-1-2)
    if (_optReachable && _koReachable > 0)
        knx.getGroupObject(_koReachable).valueCompare(_reachable, Dpt(1, 2));
}
