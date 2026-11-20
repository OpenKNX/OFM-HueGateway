#pragma once

#include "HueGatewayDevice.h"

// Forward declaration — avoid pulling the full client header into device headers
class HueGatewayClient;
class GroupObject;

/**
 * @brief Hue smart plug channel (Steckdose).
 *
 * Represents a Hue smart plug resource (in Hue API v2 exposed as a light
 * resource).  KNX switch commands are forwarded to the plug, and the
 * plug's on/off state is reflected back as a KNX status telegram.
 * KO slot 0: bidirektionales Schalten (DPST-1-1)
 * KO slot 3: Status Schalten (DPST-1-1)
 *
 * Device type discriminator: 4
 */
class HueGatewayPlug : public HueGatewayDevice
{
public:
    HueGatewayPlug(const String& resourceId, const String& name, HueGatewayClient* client)
        : _resourceId(resourceId)
        , _name(name)
        , _client(client)
        , _on(false)
        , _reachable(false)
        , _koSwitch(0)
        , _koStatusSwitch(0)
        , _optReachable(false)
        , _koReachable(0)
        , _initialized(false)
    {}

    PT_HUEChannelType deviceType() const override { return PT_HUEChannelType::Steckdose; }

    String getResourceId() const override { return _resourceId; }
    String getName() const override { return _name; }
    bool isOn() const override { return _on; }
    bool isReachable() const override { return _reachable; }

    /**
     * @brief Initializes KO mapping.
     * @param koSwitch KO number for bidirectional switch (KO slot 0)
     * @param koStatusSwitch KO number for status feedback (KO slot 3)
     * @param optReachable Enable Reachable-KO (slot 9, DPST-1-2)
     * @param koReachable KO number for reachable status
     */
    void begin(uint16_t koSwitch, uint16_t koStatusSwitch,
               bool optReachable = false, uint16_t koReachable = 0)
    {
        _koSwitch        = koSwitch;
        _koStatusSwitch  = koStatusSwitch;
        _optReachable    = optReachable;
        _koReachable     = koReachable;
        _initialized     = true;
    }

    /**
     * @brief Updates state from Hue polling/event and sends to KNX if changed.
     */
    void updateFromHue(bool on, bool reachable)
    {
        const bool changed = (_on != on || _reachable != reachable);
        _on = on;
        _reachable = reachable;
        if (changed)
            sendStatusToKnx();
    }

    void sendStatusToKnx() override;
    bool processKoInput(uint8_t koType, GroupObject& ko) override;

    /**
     * @brief Executes an ETS scene preset on this plug channel.
     * Called from HueGatewayModule when a matching scene slot is found.
     * @param on Target on/off state
     */
    void processKnxSceneRecall(bool on);
    unsigned long getLastHueWriteSuccessMs() const override { return _lastHueWriteSuccessMs; }

private:
    String _resourceId;
    String _name;
    HueGatewayClient* _client;
    bool _on;
    bool _reachable;
    uint16_t _koSwitch;
    uint16_t _koStatusSwitch;
    bool _optReachable;
    uint16_t _koReachable;
    bool _initialized;
    unsigned long _lastHueWriteSuccessMs = 0;
};
