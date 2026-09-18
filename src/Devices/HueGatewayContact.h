#pragma once

#include "HueGatewayDevice.h"
#include "../HueGatewayClient.h"

/**
 * @brief Hue contact sensor channel (Kontaktsensor).
 *
 * Represents a Hue contact sensor resource (/clip/v2/resource/contact_sensor).
 * The sensor sends its open/closed state (DPST-1-9) via KNX 1-bit
 * status telegram on KO slot 0.
 *
 * Device type discriminator: 3
 */
class HueGatewayContact : public HueGatewayDevice
{
public:
    HueGatewayContact(const String& resourceId, const String& name)
        : _resourceId(resourceId)
        , _name(name)
        , _contactOpen(false)
        , _reachable(false)
        , _tampered(false)
        , _batteryPercent(255)
        , _koContactStatus(0)
        , _optTamper(false),    _koTamper(0)
        , _optBattery(false),   _koBattery(0)
        , _optReachable(false), _koReachable(0)
        , _initialized(false)
    {}

    PT_HUEChannelType deviceType() const override { return PT_HUEChannelType::Kontaktsensor; }

    String getResourceId() const override { return _resourceId; }
    String getName() const override { return _name; }
    bool isReachable() const override { return _reachable; }

    /** Returns true when the contact is open. */
    bool isContactOpen() const { return _contactOpen; }

    /**
     * @brief Initializes KO mapping.
     * @param koContactStatus KO number for contact status (KO slot 0, DPST-1-9)
     * @param optTamper    Enable Tamper-KO (slot 3, DPST-1-1)
     * @param optBattery   Enable Battery-KO (slot 4, DPST-5-1)
     * @param optReachable Enable Reachable-KO (slot 9, DPST-1-2)
     */
    void begin(uint16_t koContactStatus,
               bool optTamper    = false, uint16_t koTamper    = 0,
               bool optBattery   = false, uint16_t koBattery   = 0,
               bool optReachable = false, uint16_t koReachable = 0)
    {
        _koContactStatus = koContactStatus;
        _optTamper    = optTamper;    _koTamper    = koTamper;
        _optBattery   = optBattery;   _koBattery   = koBattery;
        _optReachable = optReachable; _koReachable = koReachable;
        _initialized  = true;
    }

    /** Sets the contact service RID (for SSE event matching). */
    void setContactServiceRid(const String& rid) { _contactServiceRid = rid; }

    /** Returns true if the given SSE resource ID matches this sensor. */
    bool matchesServiceRid(const String& rid) const
    {
        if (_contactServiceRid.length() > 0)
            return _contactServiceRid.equalsIgnoreCase(rid);
        return _resourceId.equalsIgnoreCase(rid);
    }

    bool matchesEventRid(const String& rid) const override { return matchesServiceRid(rid); }

    /**
     * @brief Lightweight update from SSE event (contact only, no optional KO data).
     */
    void updateContactOnly(bool contactOpen)
    {
        const bool changed = (_contactOpen != contactOpen);
        _contactOpen = contactOpen;
        if (changed && _initialized)
            sendStatusToKnx();
    }

    /**
     * @brief Updates state from Hue polling and sends changed values to KNX.
     */
    void updateFromState(const HueGatewayContactState& s)
    {
        if (!_initialized) return;
        const bool contactChanged   = (_contactOpen != s.contactOpen);
        const bool reachableChanged = (_reachable   != s.reachable);
        const bool tamperedChanged  = (_tampered    != s.tampered);
        const bool batteryChanged   = (_batteryPercent != s.batteryPercent);

        _contactOpen    = s.contactOpen;
        _reachable      = s.reachable;
        _tampered       = s.tampered;
        _batteryPercent = s.batteryPercent;

        if (contactChanged)
            sendStatusToKnx();
        if (reachableChanged || tamperedChanged || batteryChanged)
            sendOptionalKosToKnx();
    }

    void sendStatusToKnx() override;
    void sendOptionalKosToKnx();

private:
    String _resourceId;
    String _name;
    String _contactServiceRid;  // contact_sensor service RID for SSE matching
    bool _contactOpen;
    bool _reachable;
    bool _tampered;
    uint8_t _batteryPercent;
    uint16_t _koContactStatus;
    bool _optTamper;    uint16_t _koTamper;
    bool _optBattery;   uint16_t _koBattery;
    bool _optReachable; uint16_t _koReachable;
    bool _initialized;
};
