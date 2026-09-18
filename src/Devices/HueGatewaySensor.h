#pragma once

#include "HueGatewayDevice.h"
#include "../HueGatewayClient.h"

/**
 * @brief Hue motion sensor channel (Bewegungsmelder).
 *
 * Represents a Hue motion sensor resource (/clip/v2/resource/motion).
 * The sensor sends its motion state (DPST-1-8) via KNX status
 * telegram on KO slot 0 of the channel block.
 *
 * Device type discriminator: 1
 */
class HueGatewaySensor : public HueGatewayDevice
{
public:
    HueGatewaySensor(const String& resourceId, const String& name)
        : _resourceId(resourceId)
        , _name(name)
        , _motion(false)
        , _reachable(false)
        , _temperature(NAN)
        , _lightLevelLux(0.0f)
        , _batteryPercent(255)
        , _koMotionStatus(0)
        , _optReachable(false), _koReachable(0)
        , _optTemp(false),      _koTemp(0)
        , _optLux(false),       _koLux(0)
        , _optBattery(false),   _koBattery(0)
        , _initialized(false)
    {}

    HueChannelType deviceType() const override { return HueChannelType::Bewegungsmelder; }

    String getResourceId() const override { return _resourceId; }
    String getName() const override { return _name; }
    bool isReachable() const override { return _reachable; }
    bool isMotion() const { return _motion; }

    /**
     * @brief Initializes KO mapping.
     * @param koMotionStatus KO number for motion status (KO slot 0, DPST-1-8)
     */
    void begin(uint16_t koMotionStatus,
               bool optReachable = false, uint16_t koReachable = 0,
               bool optTemp = false,      uint16_t koTemp = 0,
               bool optLux = false,       uint16_t koLux = 0,
               bool optBattery = false,   uint16_t koBattery = 0)
    {
        _koMotionStatus = koMotionStatus;
        _optReachable   = optReachable;  _koReachable = koReachable;
        _optTemp        = optTemp;       _koTemp      = koTemp;
        _optLux         = optLux;        _koLux       = koLux;
        _optBattery     = optBattery;    _koBattery   = koBattery;
        _initialized    = true;
    }

    /** Sets the motion service RID (for SSE event matching). */
    void setMotionServiceRid(const String& rid) { _motionServiceRid = rid; }

    /** Returns true if the given SSE resource ID matches this sensor. */
    bool matchesServiceRid(const String& rid) const
    {
        if (_motionServiceRid.length() > 0)
            return _motionServiceRid.equalsIgnoreCase(rid);
        return _resourceId.equalsIgnoreCase(rid);
    }

    bool matchesEventRid(const String& rid) const override { return matchesServiceRid(rid); }

    /**
     * @brief Lightweight update from SSE event (motion only, no optional KO data).
     */
    void updateMotionOnly(bool motion)
    {
        const bool changed = (_motion != motion);
        _motion = motion;
        if (changed && _initialized)
            sendStatusToKnx();
    }

    /**
     * @brief Updates state from Hue polling and sends changed values to KNX.
     */
    void updateFromState(const HueGatewayMotionState& s)
    {
        if (!_initialized) return;
        const bool motionChanged    = (_motion    != s.motionDetected);
        const bool reachableChanged = (_reachable != s.reachable);
        const bool batteryChanged   = (_batteryPercent != s.batteryPercent);
        const bool tempChanged      = (isnan(_temperature) != isnan(s.temperature)) ||
                                      (!isnan(_temperature) && fabsf(_temperature - s.temperature) > 0.1f);
        const bool luxChanged       = (fabsf(_lightLevelLux - s.lightLevelLux) > 0.5f);

        _motion         = s.motionDetected;
        _reachable      = s.reachable;
        _temperature    = s.temperature;
        _lightLevelLux  = s.lightLevelLux;
        _batteryPercent = s.batteryPercent;

        if (motionChanged)
            sendStatusToKnx();
        if (reachableChanged || tempChanged || luxChanged || batteryChanged)
            sendOptionalKosToKnx();
    }

    void sendStatusToKnx() override;
    void sendOptionalKosToKnx();

private:
    String _resourceId;
    String _name;
    String _motionServiceRid;   // motion service RID for SSE matching
    bool _motion;
    bool _reachable;
    float _temperature;
    float _lightLevelLux;
    uint8_t _batteryPercent;
    uint16_t _koMotionStatus;
    bool _optReachable; uint16_t _koReachable;
    bool _optTemp;      uint16_t _koTemp;
    bool _optLux;       uint16_t _koLux;
    bool _optBattery;   uint16_t _koBattery;
    bool _initialized;
};
