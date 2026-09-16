#include "HueGatewayLight.h"
#include "../HueGatewayClient.h"
#include <knx.h>
#include <math.h>

#if __has_include("LightManagerModule.h")
#include "LightManagerModule.h"
#define HUEGATEWAY_LIGHT_HAS_LIGHTMANAGER 1
#endif

namespace
{
static constexpr unsigned long kGlobalHclWriteSpacingMs = 220UL;
static unsigned long sGlobalHclWriteNextAllowedMs = 0UL;
static constexpr uint8_t kMaxHclMasters = 16;

static unsigned long relativeDimRepeatMs()
{
#ifdef ParamHUE_HUERelDimRepeatMs
    return (unsigned long)ParamHUE_HUERelDimRepeatMs;
#else
    return 120UL;
#endif
}

uint16_t stablePhaseOffsetMs(const String& id)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < id.length(); i++)
    {
        hash ^= static_cast<uint8_t>(id[i]);
        hash *= 16777619u;
    }

    return static_cast<uint16_t>(150u + (hash % 850u));
}
}

HueGatewayLight::HueGatewayLight(const String& lightId, const String& name, HueGatewayClient* client)
    : _lightId(lightId)
    , _name(name)
    , _client(client)
    , _koSwitch(0)
    , _koBrightness(0)
    , _koDimming(0)
    , _koStatusSwitch(0)
    , _koStatusBrightness(0)
    , _koStatusColorTemp(0)
    , _koStatusColorRGB(0)
    , _on(false)
    , _brightness(0)
    , _reachable(true)
    , _currentRed(255)
    , _currentGreen(255)
    , _currentBlue(255)
    , _lightType(0)
    , _minBrightnessPercent(0)
    , _minBrightnessHue(0)
    , _lastNonZeroBrightnessHue(0)
    , _isGroupedTarget(false)
    , _hclMasterNum(0)
    , _hclChannelLockActive(false)
    , _fadingActive(false)
    , _currentKelvin(4000)
    , _lastHCLUpdate(0)
    , _lastHCLBrightness(0)
    , _hclPhaseOffsetMs(0)
    , _nextHCLDueMs(0)
    , _pendingHclKelvin(4000)
    , _pendingHclBrightness(0)
    , _pendingHclFadeDuration(10)
    , _hclValuePending(false)
    , _lastHueWriteSuccessMs(0)
    , _initialized(false)
    , _lastUpdate(0)
    , _lastRelativeDimCmdMs(0)
    , _relativeDimCooldownUntilMs(0)
    , _relativeDimErrorStreak(0)
    , _switchOffPending(false)
    , _relativeDimHoldActive(false)
    , _relativeDimHoldBrighter(false)
    , _relativeDimHoldSteps(0)
    , _relativeDimNextMs(0)
    , _switchOnTransitionSec(2)
    , _switchOffTransitionSec(6)
{
    _hclPhaseOffsetMs = stablePhaseOffsetMs(_lightId);
}

HueGatewayLight::~HueGatewayLight()
{
}

unsigned long HueGatewayLight::globalHclWriteNextAllowedMs() { return sGlobalHclWriteNextAllowedMs; }

void HueGatewayLight::begin(uint16_t koSwitch, uint16_t koBrightness, uint16_t koDimming,
                            uint16_t koStatusSwitch, uint16_t koStatusBrightness,
                            uint16_t koStatusColorTemp, uint16_t koStatusColorRGB)
{
    _koSwitch = koSwitch;
    _koBrightness = koBrightness;
    _koDimming = koDimming;
    _koStatusSwitch = koStatusSwitch;
    _koStatusBrightness = koStatusBrightness;
    _koStatusColorTemp = koStatusColorTemp;
    _koStatusColorRGB = koStatusColorRGB;
    _initialized = true;
    
    Serial.printf("[HueGatewayLight] %s initialized - KO Switch:%d Brightness:%d Dimming:%d StatusSwitch:%d StatusBrightness:%d StatusColorTemp:%d StatusRGB:%d\n",
                  _name.c_str(), _koSwitch, _koBrightness, _koDimming, _koStatusSwitch, _koStatusBrightness, _koStatusColorTemp, _koStatusColorRGB);
}

void HueGatewayLight::processKnxSwitch(bool value)
{
    if (!_initialized || !_client)
        return;

    _relativeDimHoldActive = false;
    
    Serial.printf("[HueGatewayLight] %s - KNX Switch: %d (onFade:%us offFade:%us => fade:%us)\n", 
                  _name.c_str(), value, 
                  static_cast<unsigned>(_switchOnTransitionSec),
                  static_cast<unsigned>(_switchOffTransitionSec),
                  static_cast<unsigned>(value ? _switchOnTransitionSec : _switchOffTransitionSec));
    
    // Stop ongoing fade immediately when switching off.
    if (!value && _fadingActive) {
        _fadingActive = false;
        Serial.printf("[HueGatewayLight] %s - Fade aborted by switch off\n", _name.c_str());
    }

    // Aus-Befehl hebt HCL-Kanalsperre auf, damit beim nächsten Ein der HCL-Wert wieder greift.
    if (!value && _hclChannelLockActive && _hclMasterNum > 0) {
        _hclChannelLockActive = false;
        Serial.printf("[HueGatewayLight] %s - HCL channel lock released by switch-off\n", _name.c_str());
    }

    // Track explicit off-commands so updateFromHue can suppress brightness
    // cache updates during the bridge's fade-out transition.
    _switchOffPending = !value;

    _on = value;
    uint8_t switchTransitionSec = value ? _switchOnTransitionSec : _switchOffTransitionSec;

    if (_on && _brightness == 0)
    {
        if (_lastNonZeroBrightnessHue > 0)
        {
            // Restore the last known brightness (covers grouped and non-grouped).
            _brightness = _lastNonZeroBrightnessHue;
        }
        else if (_initialized)
        {
            // No brightness known yet (first switch-on after power-up/reprogram).
            // Send on=true with brightness=0: the bridge will restore its own last
            // value. Do NOT apply _minBrightnessHue here — that would force the
            // light to 1% every time the gateway restarts.
            // _brightness stays 0; sendToHue() will pass it to the bridge as-is.
        }
    }

    if (_brightness > 0)
    {
        _lastNonZeroBrightnessHue = _brightness;
    }
    
    // On switch-on with HCL assignment, apply the current HCL value immediately.
    // _pendingHclKelvin/_pendingHclBrightness are only refreshed while _on==true
    // (the HCL loop skips !_on). Before the first switch-on — or after a long
    // off period — the cached values are stale or never set (_hclValuePending==false).
    // Always read fresh values from the LightManager directly so the light snaps
    // to the correct current value without any visible transition.
    if (value
        && _hclMasterNum > 0
        && _hclMasterNum <= kMaxHclMasters
        && !_hclChannelLockActive) {
#ifdef HUEGATEWAY_LIGHT_HAS_LIGHTMANAGER
        {
            auto* lmCh = openknxLightManagerModule.channel(_hclMasterNum);
            if (lmCh && !lmCh->applyBlocked() && !HCL::masterManager.isApplyBlocked()
                && lmCh->internalOutputEnabled())
            {
                const uint8_t validMask = lmCh->internalValidMask();
                if (validMask & 0x01) _pendingHclKelvin     = lmCh->appliedKelvin();
                if (validMask & 0x02) _pendingHclBrightness = lmCh->effectiveBrightness();
                _hclValuePending = true;
            }
        }
#endif
        if (_hclValuePending) {
            const uint16_t kelvin = _pendingHclKelvin;
            const uint8_t brightness = _pendingHclBrightness;

            // Convert brightness percent (0-100) to Hue scale (0-254).
            _brightness = static_cast<uint8_t>(roundf(brightness * 254.0f / 100.0f));

            _lastHCLUpdate = millis();
            _lastHCLBrightness = brightness;

            Serial.printf("[HueGatewayLight] %s - Applying HCL Master %d values on switch-on: %dK, %d%% (%d Hue)\n",
                         _name.c_str(), _hclMasterNum, kelvin, brightness, _brightness);

            if (_lightType >= 2)
            {
                // Use the user-configured switch-on transition time.
                // The correct CT is included in this call, so the bridge animates
                // TO the right value from the start — no wrong-old-CT artefact.
                sendToHueWithColorTemp(kelvin, switchTransitionSec);
                return;
            }

            sendToHue(switchTransitionSec);
            return;
        }
    }

    sendToHue(switchTransitionSec);
}

void HueGatewayLight::processKnxBrightness(uint8_t value)
{
    if (!_initialized || !_client)
        return;

    _relativeDimHoldActive = false;
    
    Serial.printf("[HueGatewayLight] %s - KNX Brightness: %d%% (DPT 5.001)\n", _name.c_str(), value);
    
    const bool previousOn = _on;

    // KNX DPT 5.001: 0-100% -> Hue 0-254.
    _brightness = static_cast<uint8_t>(roundf((value / 100.0f) * 254.0f));

    if (_brightness > 0 && _brightness < _minBrightnessHue)
    {
        _brightness = _minBrightnessHue;
    }

    if (_brightness > 0)
    {
        _lastNonZeroBrightnessHue = _brightness;
    }
    
    // Auto-turn on when brightness is set above zero.
    if (_brightness > 0 && !_on)
    {
        _on = true;
        Serial.printf("[HueGatewayLight] %s - Auto-on due to brightness > 0\n", _name.c_str());
    }
    // Auto-turn off when brightness reaches zero.
    else if (_brightness == 0 && _on)
    {
        _on = false;
        Serial.printf("[HueGatewayLight] %s - Auto-off due to brightness = 0\n", _name.c_str());
    }
    
    if (previousOn != _on)
    {
        const uint8_t transitionSec = _on ? _switchOnTransitionSec : _switchOffTransitionSec;
        sendToHue(transitionSec);
        return;
    }

    sendToHue();
}

void HueGatewayLight::processKnxDimming(uint8_t control)
{
    if (!_initialized || !_client)
        return;
    
    // DPT 3.007: 4-bit dimming control.
    // Bit 3: 0=darker, 1=brighter
    // Bit 0-2: number of steps (0=stop, 1-7=steps)
    
    uint8_t steps = control & 0x07;  // Bits 0-2
    bool brighter = (control & 0x08) != 0;  // Bit 3
    unsigned long nowMs = millis();

    // Ignore stop telegram (0 steps).
    if (steps == 0)
    {
        _relativeDimHoldActive = false;
        _relativeDimHoldSteps = 0;
        _relativeDimNextMs = 0;

        Serial.printf("[HueGatewayLight] %s - KNX Dimming STOP\n", _name.c_str());

        bool stopOk = _isGroupedTarget
            ? _client->stopGroupedLightDimming(_lightId)
            : _client->stopLightDimming(_lightId);

        if (!stopOk)
        {
            _relativeDimErrorStreak = min<uint8_t>(static_cast<uint8_t>(_relativeDimErrorStreak + 1), static_cast<uint8_t>(10));
            if (_relativeDimErrorStreak >= 5)
            {
                _relativeDimCooldownUntilMs = nowMs + 2000UL;
                Serial.printf("[HueGatewayLight] %s - Relative dimming disabled for 2s after stop errors\n", _name.c_str());
            }
        }
        else
        {
            _relativeDimErrorStreak = 0;
            _relativeDimCooldownUntilMs = 0;
        }
        return;
    }

    // Debounce very fast telegram bursts to reduce API pressure.
    // STOP telegrams are intentionally excluded so key release is never suppressed.
    if ((nowMs - _lastRelativeDimCmdMs) < 80UL)
    {
        return;
    }
    _lastRelativeDimCmdMs = nowMs;

    _relativeDimHoldActive = true;
    _relativeDimHoldBrighter = brighter;
    _relativeDimHoldSteps = steps;
    _relativeDimNextMs = nowMs + relativeDimRepeatMs();

    // Use relative delta API when healthy, otherwise fallback to stable legacy path.
    if (_relativeDimCooldownUntilMs == 0 || nowMs >= _relativeDimCooldownUntilMs)
    {
        bool deltaOk = _isGroupedTarget
            ? _client->setGroupedLightDimmingDelta(_lightId, brighter, steps)
            : _client->setLightDimmingDelta(_lightId, brighter, steps);

        if (deltaOk)
        {
            _relativeDimErrorStreak = 0;
            _relativeDimCooldownUntilMs = 0;
            applyRelativeDimmingCache(brighter, steps);
            sendStatusToKnx();
            return;
        }

        _relativeDimErrorStreak = min<uint8_t>(static_cast<uint8_t>(_relativeDimErrorStreak + 1), static_cast<uint8_t>(10));
        if (_relativeDimErrorStreak >= 5)
        {
            _relativeDimCooldownUntilMs = nowMs + 2000UL;
            Serial.printf("[HueGatewayLight] %s - Relative dimming disabled for 2s, falling back to legacy\n", _name.c_str());
        }
    }

    applyRelativeDimmingLegacy(brighter, steps);
}

void HueGatewayLight::applyRelativeDimmingLegacy(bool brighter, uint8_t steps)
{
    applyRelativeDimmingCache(brighter, steps);
    sendToHue();
}

void HueGatewayLight::applyRelativeDimmingCache(bool brighter, uint8_t steps)
{
    float deltaPercent = HueGatewayClient::relativeDimmingDeltaPercent(steps);
    int16_t brightnessChange = static_cast<int16_t>(roundf((deltaPercent / 100.0f) * 254.0f));
    if (brightnessChange < 1)
        brightnessChange = 1;

    if (!brighter)
        brightnessChange = -brightnessChange;

    int16_t newBrightness = static_cast<int16_t>(_brightness) + brightnessChange;
    if (newBrightness < 0)
        newBrightness = 0;
    if (newBrightness > 254)
        newBrightness = 254;

    if (!brighter && _on && newBrightness == 0)
    {
        newBrightness = (_minBrightnessHue > 0) ? _minBrightnessHue : 1;
    }

    _brightness = static_cast<uint8_t>(newBrightness);

    if (_brightness > 0 && _brightness < _minBrightnessHue)
    {
        _brightness = _minBrightnessHue;
    }

    if (_brightness > 0)
    {
        _lastNonZeroBrightnessHue = _brightness;
    }

    Serial.printf("[HueGatewayLight] %s - KNX Dimming: %s stepCode=%u (%.3f%%) -> Cached Brightness: %u\n",
                  _name.c_str(),
                  brighter ? "BRIGHTER" : "DARKER",
                  static_cast<unsigned>(steps),
                  deltaPercent,
                  static_cast<unsigned>(_brightness));

    if (_brightness > 0 && !_on)
    {
        _on = true;
    }
}

void HueGatewayLight::processKnxColorTemp(uint16_t kelvin)
{
    if (!_initialized || !_client)
        return;

    if (_lightType < 2)
    {
        Serial.printf("[HueGatewayLight] %s - Ignoring ColorTemp for non-CT light type %u\n", _name.c_str(), _lightType);
        return;
    }

    if (kelvin < 2000) kelvin = 2000;
    if (kelvin > 6500) kelvin = 6500;

    uint16_t mirek = kelvinToMirek(kelvin);
    bool ok = _isGroupedTarget
        ? _client->setGroupedLightColorTemperature(_lightId, mirek)
        : _client->setLightColorTemperature(_lightId, mirek);

    if (ok)
    {
        _currentKelvin = kelvin;
        _lastUpdate = millis();
        sendStatusToKnx();
        Serial.printf("[HueGatewayLight] %s - KNX ColorTemp: %uK (%u mirek)\n", _name.c_str(), kelvin, mirek);
    }
    else
    {
        Serial.printf("[HueGatewayLight] %s - ERROR: Failed to set ColorTemp\n", _name.c_str());
    }
}

void HueGatewayLight::processKnxColorRGB(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!_initialized || !_client)
        return;

    if (_lightType < 3)
    {
        Serial.printf("[HueGatewayLight] %s - Ignoring RGB for non-RGB light type %u\n", _name.c_str(), _lightType);
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    rgbToXy(red, green, blue, x, y);

    bool ok = _isGroupedTarget
        ? _client->setGroupedLightColor(_lightId, x, y)
        : _client->setLightColor(_lightId, x, y);

    if (ok)
    {
        _currentRed = red;
        _currentGreen = green;
        _currentBlue = blue;
        _lastUpdate = millis();
        sendStatusToKnx();
        Serial.printf("[HueGatewayLight] %s - KNX RGB: (%u,%u,%u) -> XY:(%.3f,%.3f)\n",
                      _name.c_str(), red, green, blue, x, y);
    }
    else
    {
        Serial.printf("[HueGatewayLight] %s - ERROR: Failed to set RGB\n", _name.c_str());
    }
}

void HueGatewayLight::updateFromHue(bool on, uint8_t brightness, uint16_t colorTempKelvin, uint8_t red, uint8_t green, uint8_t blue)
{
    // grouped_light can transiently report on=false while brightness is still >0
    // around relative dim stop; keep channel ON in this short reconciliation window.
    if (_isGroupedTarget && !on && brightness > 0)
    {
        const unsigned long nowMs = millis();
        if ((nowMs - _lastRelativeDimCmdMs) <= 3000UL)
        {
            on = true;
        }
    }

    bool changed = false;

    if (_on != on)
    {
        _on = on;
        changed = true;
    }

    // Clear the pending-off flag once the bridge confirms the light is off.
    if (!on)
        _switchOffPending = false;

    // Only update brightness and the restore-value cache when not in the middle
    // of a commanded fade-out AND the bridge reports the light as ON.
    // _switchOffPending is set by processKnxSwitch(false) and cleared above when
    // the bridge finally confirms on=false.
    // The on==true gate is essential: a Hue bridge keeps reporting its stored
    // dimming level even while the light is OFF. Without this gate an off-state
    // poll would write that value into _brightness, and the next plain switch-on
    // would replay it to the bridge instead of letting the bridge restore its own
    // last on-brightness. After a gateway restart _brightness stays 0 until the
    // light is actually on, so processKnxSwitch sends on-only (no dimming) and the
    // bridge restores its own value.
    // This blocks three corruption paths without blocking legitimate bridge
    // updates (e.g. Hue App changes while the gateway has _on==false at startup):
    //   1. Bridge sends on=false with fading brightness.
    //   2. Bridge sends on=true with decreasing brightness during fade-out
    //      while _switchOffPending is still set.
    //   3. Bridge reports its stored off-state dimming level on a poll while off.
    if (!_switchOffPending && on)
    {
        if (_brightness != brightness)
        {
            _brightness = brightness;
            changed = true;
        }

        if (_brightness > 0)
        {
            _lastNonZeroBrightnessHue = _brightness;
        }
    }

    if (_lightType >= 2 && colorTempKelvin >= 2000 && colorTempKelvin <= 6500 && _currentKelvin != colorTempKelvin)
    {
        _currentKelvin = colorTempKelvin;
        changed = true;
    }

    if (_lightType >= 3 && (_currentRed != red || _currentGreen != green || _currentBlue != blue))
    {
        _currentRed = red;
        _currentGreen = green;
        _currentBlue = blue;
        changed = true;
    }
    
    if (changed)
    {
        _lastUpdate = millis();
        Serial.printf("[HueGatewayLight] %s - Updated from Hue: On:%d Bri:%d\n",
                      _name.c_str(), _on, _brightness);
        
        sendStatusToKnx();
    }
}

void HueGatewayLight::sendStatusToKnx()
{
    if (!_initialized)
        return;
    
    // Publish status to dedicated feedback KOs.
    // Hue 0-254 -> KNX 0-100%.
    // While switched off, always report 0% brightness on KNX status.

    // KO Status Switch: DPT 1.001 (bool) - On/Off feedback.
    knx.getGroupObject(_koStatusSwitch).value(_on, Dpt(1, 1));

    // KO Status Brightness: DPT 5.001 (0-100%) - brightness feedback.
    // If the lamp is on but _brightness is still 0 (gateway hasn't heard back from the
    // bridge yet — cold-start case), skip the brightness update so we don't overwrite a
    // previously valid value with 0%.  The EventStream/poll will trigger another
    // sendStatusToKnx() call as soon as the actual brightness is known.
    if (!_on || _brightness > 0)
    {
        uint8_t statusBrightnessHue = _on ? _brightness : 0;
        uint8_t brightnessPercent = static_cast<uint8_t>(roundf((statusBrightnessHue / 254.0f) * 100.0f));
        if (brightnessPercent > 100) brightnessPercent = 100;
        knx.getGroupObject(_koStatusBrightness).value(brightnessPercent, Dpt(5, 1));
    }

    if (_lightType >= 2 && _koStatusColorTemp > 0)
    {
        knx.getGroupObject(_koStatusColorTemp).value(_currentKelvin, Dpt(7, 600));
    }

    if (_lightType >= 3 && _koStatusColorRGB > 0)
    {
        GroupObject& rgbKo = knx.getGroupObject(_koStatusColorRGB);
        uint8_t* rgb = rgbKo.valueRef();
        rgb[0] = _currentRed;
        rgb[1] = _currentGreen;
        rgb[2] = _currentBlue;
        rgbKo.objectWritten();
    }
    
    Serial.printf("[HueGatewayLight] %s - Sent to KNX: On:%d Bri:%s (Hue:%d)\n",
                  _name.c_str(), _on,
                  (_on && _brightness == 0) ? "?" : String(static_cast<uint8_t>(roundf((_brightness / 254.0f) * 100.0f))).c_str(),
                  _brightness);
}

// ===== Private Methods =====

void HueGatewayLight::sendToHue(uint8_t fadeDurationSec, bool includeOnState)
{
    if (!_client || !_client->isInitialized())
    {
        Serial.printf("[HueGatewayLight] %s - ERROR: Client not ready\n", _name.c_str());
        return;
    }
    
    bool success = _isGroupedTarget
        ? _client->setGroupedLightState(_lightId, _on, _brightness, fadeDurationSec, includeOnState)
        : _client->setLightState(_lightId, _on, _brightness, fadeDurationSec, includeOnState);
    
    if (success)
    {
        _lastUpdate = millis();
        _lastHueWriteSuccessMs = _lastUpdate;
        sendStatusToKnx();
        Serial.printf("[HueGatewayLight] %s - Sent to Hue: On:%d Bri:%d fade:%us\n",
                  _name.c_str(), _on, _brightness, static_cast<unsigned>(fadeDurationSec));
    }
    else
    {
        Serial.printf("[HueGatewayLight] %s - ERROR: Failed to send to Hue\n", _name.c_str());
    }
}

uint8_t HueGatewayLight::knxToHueBrightness(uint8_t knxValue)
{
    // KNX: 0-255
    // Hue: 0-254 (1-254 for dimmable light, 0 = off)
    
    if (knxValue == 0)
        return 0;
    
    // Linear mapping: 1-255 -> 1-254
    return (uint8_t)((knxValue / 255.0f) * 254.0f);
}

uint8_t HueGatewayLight::hueToKnxBrightness(uint8_t hueValue)
{
    // Hue: 0-254
    // KNX: 0-255
    
    if (hueValue == 0)
        return 0;
    
    // Linear mapping: 0-254 -> 0-255
    return (uint8_t)((hueValue / 254.0f) * 255.0f);
}

void HueGatewayLight::setMinBrightness(uint8_t minBrightness)
{
    if (minBrightness > 100)
    {
        minBrightness = 100;
    }

    _minBrightnessPercent = minBrightness;
    _minBrightnessHue = static_cast<uint8_t>((static_cast<uint16_t>(_minBrightnessPercent) * 254U) / 100U);
}

void HueGatewayLight::setSwitchTransitionDurations(uint8_t onTransitionSec, uint8_t offTransitionSec)
{
    _switchOnTransitionSec = onTransitionSec;
    _switchOffTransitionSec = offTransitionSec;
}

uint16_t HueGatewayLight::kelvinToMirek(uint16_t kelvin)
{
    // Kelvin -> mirek (micro reciprocal kelvin)
    // mirek = 1.000.000 / Kelvin
    // Hue range: 153-500 mirek (equivalent to 6500K-2000K)
    
    if (kelvin < 2000) kelvin = 2000;
    if (kelvin > 6500) kelvin = 6500;
    
    uint16_t mirek = 1000000 / kelvin;
    
    // Clamp to Hue's valid range
    if (mirek < 153) mirek = 153;  // 6500K
    if (mirek > 500) mirek = 500;  // 2000K
    
    return mirek;
}

void HueGatewayLight::rgbToXy(uint8_t red, uint8_t green, uint8_t blue, float& x, float& y)
{
    float r = red / 255.0f;
    float g = green / 255.0f;
    float b = blue / 255.0f;

    r = (r > 0.04045f) ? powf((r + 0.055f) / 1.055f, 2.4f) : (r / 12.92f);
    g = (g > 0.04045f) ? powf((g + 0.055f) / 1.055f, 2.4f) : (g / 12.92f);
    b = (b > 0.04045f) ? powf((b + 0.055f) / 1.055f, 2.4f) : (b / 12.92f);

    float X = r * 0.664511f + g * 0.154324f + b * 0.162028f;
    float Y = r * 0.283881f + g * 0.668433f + b * 0.047685f;
    float Z = r * 0.000088f + g * 0.072310f + b * 0.986039f;

    float sum = X + Y + Z;
    if (sum <= 0.000001f)
    {
        x = 0.3127f;
        y = 0.3290f;
        return;
    }

    x = X / sum;
    y = Y / sum;
}

void HueGatewayLight::sendToHueWithColorTemp(uint16_t kelvin, uint8_t fadeDuration, bool includeOnState)
{
    if (!_initialized || !_client)
        return;

    uint16_t mirek = kelvinToMirek(kelvin);
    _currentKelvin = kelvin;
    
    Serial.printf("[HueGatewayLight] %s - Sending to Hue: %s, %d%% (%d/254), %dK (%d mirek), fade:%ds\n",
                  _name.c_str(), 
                  _on ? "ON" : "OFF",
                  (_brightness * 100) / 254,
                  _brightness,
                  kelvin,
                  mirek,
                  fadeDuration);
    
    bool success = _isGroupedTarget
        ? _client->setGroupedLightStateWithColorTemp(_lightId, _on, _brightness, mirek, fadeDuration, includeOnState)
        : _client->setLightStateWithColorTemp(_lightId, _on, _brightness, mirek, fadeDuration, includeOnState);

    if (!success && _isGroupedTarget)
    {
        Serial.printf("[HueGatewayLight] %s - grouped CT update failed, fallback to state-only\n", _name.c_str());
        success = _client->setGroupedLightState(_lightId, _on, _brightness, fadeDuration, includeOnState);
    }

    if (success)
    {
        _lastUpdate = millis();
        _lastHueWriteSuccessMs = _lastUpdate;
        sendStatusToKnx();
    }
    else
    {
        Serial.printf("[HueGatewayLight] %s - ERROR: Failed to send color-temp state to Hue\n", _name.c_str());
    }
}

void HueGatewayLight::processKnxSceneRecall(bool on,
                                             bool applyBrightness, uint8_t brightnessPercent,
                                             bool applyCT, uint16_t kelvin,
                                             bool applyColor, uint8_t red, uint8_t green, uint8_t blue)
{
    if (!_initialized || !_client)
        return;

    // Convert percent (0-100) to Hue range (0-254).
    uint8_t hueBrightness = static_cast<uint8_t>(
        (static_cast<uint16_t>(brightnessPercent) * 254U) / 100U);

    if (applyBrightness && on && _minBrightnessHue > 0 && hueBrightness < _minBrightnessHue)
        hueBrightness = _minBrightnessHue;

    _on = on;
    if (applyBrightness)
    {
        _brightness = hueBrightness;
        if (on && hueBrightness > 0)
            _lastNonZeroBrightnessHue = hueBrightness;
    }

    const uint8_t fadeDuration = on ? _switchOnTransitionSec : _switchOffTransitionSec;

    const bool doColor = applyColor && (_lightType >= 3) && (red != 0 || green != 0 || blue != 0);
    const bool doCT    = applyCT && !doColor && (_lightType >= 2) && (kelvin >= 2000 && kelvin <= 6500);

    bool success = false;

    if (doColor)
    {
        float x = 0.0f, y = 0.0f;
        rgbToXy(red, green, blue, x, y);
        _currentRed   = red;
        _currentGreen = green;
        _currentBlue  = blue;

        // Two-step: set on+brightness first, then apply colour.
        success = _isGroupedTarget
            ? _client->setGroupedLightState(_lightId, _on, _brightness, fadeDuration)
            : _client->setLightState(_lightId, _on, _brightness, fadeDuration);

        if (success)
        {
            _isGroupedTarget
                ? _client->setGroupedLightColor(_lightId, x, y)
                : _client->setLightColor(_lightId, x, y);
        }
    }
    else if (doCT)
    {
        uint16_t mirek = kelvinToMirek(kelvin);
        _currentKelvin = kelvin;

        success = _isGroupedTarget
            ? _client->setGroupedLightStateWithColorTemp(_lightId, _on, _brightness, mirek, fadeDuration)
            : _client->setLightStateWithColorTemp(_lightId, _on, _brightness, mirek, fadeDuration);

        if (!success && _isGroupedTarget)
            success = _client->setGroupedLightState(_lightId, _on, _brightness, fadeDuration);
    }
    else if (applyBrightness)
    {
        success = _isGroupedTarget
            ? _client->setGroupedLightState(_lightId, _on, _brightness, fadeDuration)
            : _client->setLightState(_lightId, _on, _brightness, fadeDuration);
    }
    else
    {
        success = _isGroupedTarget
            ? _client->setGroupedLightOnOff(_lightId, _on)
            : _client->setLightOnOff(_lightId, _on);
    }

    if (success)
    {
        _lastUpdate = millis();
        _lastHueWriteSuccessMs = _lastUpdate;
        if (_hclMasterNum > 0)
            _hclChannelLockActive = true;
        sendStatusToKnx();
        Serial.printf("[HueGatewayLight] %s - Scene recall: On:%d Bri:%u%% (Hue:%u)\n",
                      _name.c_str(), static_cast<int>(on),
                      static_cast<unsigned>(brightnessPercent),
                      static_cast<unsigned>(hueBrightness));
    }
    else
    {
        Serial.printf("[HueGatewayLight] %s - ERROR: Scene recall failed\n", _name.c_str());
    }
}

void HueGatewayLight::loop()
{
    if (_initialized && _client && _relativeDimHoldActive && _relativeDimHoldSteps > 0)
    {
        unsigned long nowMs = millis();
        if ((long)(nowMs - _relativeDimNextMs) >= 0)
        {
            bool commandOk = false;
            if (_relativeDimCooldownUntilMs == 0 || nowMs >= _relativeDimCooldownUntilMs)
            {
                commandOk = _isGroupedTarget
                    ? _client->setGroupedLightDimmingDelta(_lightId, _relativeDimHoldBrighter, _relativeDimHoldSteps)
                    : _client->setLightDimmingDelta(_lightId, _relativeDimHoldBrighter, _relativeDimHoldSteps);

                if (commandOk)
                {
                    _relativeDimErrorStreak = 0;
                    _relativeDimCooldownUntilMs = 0;
                    applyRelativeDimmingCache(_relativeDimHoldBrighter, _relativeDimHoldSteps);
                    sendStatusToKnx();
                }
                else
                {
                    _relativeDimErrorStreak = min<uint8_t>(static_cast<uint8_t>(_relativeDimErrorStreak + 1), static_cast<uint8_t>(10));
                    if (_relativeDimErrorStreak >= 5)
                    {
                        _relativeDimCooldownUntilMs = nowMs + 2000UL;
                    }
                }
            }
            else
            {
                applyRelativeDimmingLegacy(_relativeDimHoldBrighter, _relativeDimHoldSteps);
                commandOk = true;
            }

            _relativeDimNextMs = nowMs + relativeDimRepeatMs();
        }
    }

    // Only run HCL loop when initialized, switched on, and assigned to a valid master.
    if (!_initialized
        || !_on
        || _hclMasterNum == 0
        || _hclMasterNum > kMaxHclMasters
        || _hclChannelLockActive)
        return;

#ifdef HUEGATEWAY_LIGHT_HAS_LIGHTMANAGER
    {
        auto* lmCh = openknxLightManagerModule.channel(_hclMasterNum);
        if (lmCh && !lmCh->applyBlocked() && !HCL::masterManager.isApplyBlocked()
            && lmCh->internalOutputEnabled())
        {
            const uint8_t validMask = lmCh->internalValidMask();
            if (validMask != 0)
            {
                if (validMask & 0x01) _pendingHclKelvin     = lmCh->appliedKelvin();
                if (validMask & 0x02) _pendingHclBrightness = lmCh->effectiveBrightness();
                _pendingHclFadeDuration = lmCh->fadeDurationSec();
                _hclValuePending = true;
            }
        }
    }
#endif

    if (!_hclValuePending)
        return;

    unsigned long now = millis();

    if (now < sGlobalHclWriteNextAllowedMs)
    {
        // Pending value is retained; retry in next loop cycle.
        return;
    }

    // Consume the pending pushed value from LightManagerModule.
    const uint16_t kelvin = _pendingHclKelvin;
    const uint8_t brightness = _pendingHclBrightness;
    const uint8_t fadeDuration = _pendingHclFadeDuration;
    _hclValuePending = false;

    // Update when at least a small effective step changed.
    static constexpr uint8_t kHclMinBrightnessStepPercent = 2;
    static constexpr uint16_t kHclMinKelvinStep = 12;
    const int previousKelvin = static_cast<int>(_currentKelvin);
    const int previousBrightness = static_cast<int>(_lastHCLBrightness);
    const bool kelvinChanged = abs(static_cast<int>(kelvin) - previousKelvin) >= static_cast<int>(kHclMinKelvinStep);
    const bool brightnessChanged = abs(static_cast<int>(brightness) - previousBrightness) >= static_cast<int>(kHclMinBrightnessStepPercent);

    if (!kelvinChanged && !brightnessChanged)
        return;

    // Apply changed values.
    _lastHCLUpdate = now;
    _lastHCLBrightness = brightness;

    // Convert brightness percent (0-100) to Hue scale (0-254).
    _brightness = static_cast<uint8_t>(roundf(brightness * 254.0f / 100.0f));

    Serial.printf("[HueGatewayLight] %s - HCL Update: %dK -> %dK, %d%% -> %d%%\n",
                 _name.c_str(), previousKelvin, kelvin,
                 previousBrightness, brightness);

    // Periodic follow-up must never switch anything on: on a grouped target it
    // would re-activate members an overlapping zone had switched off, and on a
    // single light it would revive one that was switched off outside KNX.
    if (_lightType >= 2)
    {
        sendToHueWithColorTemp(kelvin, fadeDuration, false);
    }
    else
    {
        sendToHue(fadeDuration, false);
    }

    // Set spacing gate AFTER the blocking PUT so the gate covers the full
    // 220 ms window after completion.  This keeps the EventStream retry
    // deferred while further HCL writes for other lights are still pending.
    sGlobalHclWriteNextAllowedMs = millis() + kGlobalHclWriteSpacingMs;
}
