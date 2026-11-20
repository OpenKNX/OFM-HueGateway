#include "HueGatewayPlug.h"
#include "../HueGatewayClient.h"
#include <knx.h>

void HueGatewayPlug::sendStatusToKnx()
{
    if (!_initialized)
        return;

    // valueCompare(): value() would send on every poll, not just on a change.
    if (_koSwitch > 0)
        knx.getGroupObject(_koSwitch).valueCompare(_on, Dpt(1, 1));

    if (_koStatusSwitch > 0)
        knx.getGroupObject(_koStatusSwitch).valueCompare(_on, Dpt(1, 1));

    // KO9: Erreichbar (optional, DPST-1-2)
    if (_optReachable && _koReachable > 0)
        knx.getGroupObject(_koReachable).valueCompare(_reachable, Dpt(1, 2));

    Serial.printf("[HueGatewayPlug] %s - Status -> KNX: On=%d\n",
                  _name.c_str(), _on ? 1 : 0);
}

bool HueGatewayPlug::processKoInput(uint8_t koType, GroupObject& ko)
{
    // KO slot 0: Schalten
    if (koType != 0)
        return false;

    if (!_client || !_client->isInitialized())
    {
        Serial.printf("[HueGatewayPlug] %s - ERROR: Client not ready\n", _name.c_str());
        return false;
    }

    const bool cmd = ko.value(Dpt(1, 1));
    Serial.printf("[HueGatewayPlug] %s - KNX Switch: cmd=%d\n", _name.c_str(), cmd ? 1 : 0);

    const bool ok = _client->setLightOnOff(_resourceId, cmd);
    if (ok)
    {
        _on = cmd;
        _lastHueWriteSuccessMs = millis();
        sendStatusToKnx();
    }
    return true;
}

void HueGatewayPlug::processKnxSceneRecall(bool on)
{
    if (!_initialized || !_client || !_client->isInitialized())
    {
        Serial.printf("[HueGatewayPlug] %s - ERROR: Client not ready for scene recall\n", _name.c_str());
        return;
    }

    const bool ok = _client->setLightOnOff(_resourceId, on);
    if (ok)
    {
        _on = on;
        _lastHueWriteSuccessMs = millis();
        sendStatusToKnx();
        Serial.printf("[HueGatewayPlug] %s - Scene recall: On=%d\n", _name.c_str(), on ? 1 : 0);
    }
    else
    {
        Serial.printf("[HueGatewayPlug] %s - ERROR: Scene recall failed\n", _name.c_str());
    }
}
