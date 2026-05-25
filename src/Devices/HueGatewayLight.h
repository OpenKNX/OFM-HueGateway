#pragma once

#include <Arduino.h>
#include "HueGatewayDevice.h"

// Forward Declaration
class HueGatewayClient;

/**
 * @brief Single Hue light abstraction with KNX mapping.
 * 
 * Represents one Philips Hue light and manages:
 * - Bidirectional synchronization between KNX and Hue
 * - State caching for reduced bus/API traffic
 * - KO mapping (switch, brightness, dimming, status objects)
 * 
 * KNX → Hue: processKnxUpdate()
 * Hue → KNX: updateFromHue()
 */

class HueGatewayLight : public HueGatewayDevice
{
public:
    /**
    * @brief Constructor.
     * @param lightId Hue Light Resource ID
    * @param name Device name
    * @param client Pointer to HueGatewayClient
     */
    HueGatewayLight(const String& lightId, const String& name, HueGatewayClient* client);
    ~HueGatewayLight() override;

    /** Device type: 0 = Licht */
    uint8_t deviceType() const override { return 0; }
    
    /**
    * @brief Initializes KO mapping for this light channel.
    * @param koSwitch KO number for switch command (On/Off)
    * @param koBrightness KO number for absolute brightness (DPT 5.001, 0-100%)
    * @param koDimming KO number for relative dimming (DPT 3.007, 4-bit)
    * @param koStatusSwitch KO number for On/Off status feedback (DPT 1.001)
    * @param koStatusBrightness KO number for brightness status feedback (DPT 5.001)
     */
    void begin(uint16_t koSwitch, uint16_t koBrightness, uint16_t koDimming,
               uint16_t koStatusSwitch, uint16_t koStatusBrightness,
               uint16_t koStatusColorTemp, uint16_t koStatusColorRGB);
    
    /**
    * @brief Processes KNX switch command.
    * @param value true = on, false = off
     */
    void processKnxSwitch(bool value);
    
    /**
    * @brief Processes KNX absolute brightness command.
    * @param value Brightness 0-100% (DPT 5.001)
     */
    void processKnxBrightness(uint8_t value);
    
    /**
    * @brief Processes KNX relative dimming command.
    * @param control DPT 3.007 control nibble (3-bit steps + 1-bit direction)
    *               Bit 3: 0=darker, 1=brighter
    *               Bit 0-2: number of steps (0=stop, 1-7=steps)
     */
    void processKnxDimming(uint8_t control);

    /**
    * @brief Processes KNX color temperature command.
    * @param kelvin Color temperature in Kelvin (DPT 7.600)
     */
    void processKnxColorTemp(uint16_t kelvin);

    /**
    * @brief Processes KNX RGB color command.
     */
    void processKnxColorRGB(uint8_t red, uint8_t green, uint8_t blue);

    /**
     * @brief Executes an ETS scene preset on this light channel.
     * Called from HueGatewayModule when a matching scene slot is found.
     * @param on Target on/off state
     * @param brightnessPercent Target brightness 0-100%
     * @param kelvin Color temperature in Kelvin (2000-6500; 0 = no color-temp change)
     * @param red   RGB red component (0-255)
     * @param green RGB green component (0-255)
     * @param blue  RGB blue component (0-255)
     */
    void processKnxSceneRecall(bool on,
                               bool applyBrightness, uint8_t brightnessPercent,
                               bool applyCT, uint16_t kelvin,
                               bool applyColor, uint8_t red, uint8_t green, uint8_t blue);
    
    /**
    * @brief Updates local state from Hue Bridge data.
    * Called by event stream updates or by polling fallback.
    * @param on On/off state
    * @param brightness Brightness 0-254 (Hue range)
     */
    void updateFromHue(bool on, uint8_t brightness, uint16_t colorTempKelvin, uint8_t red, uint8_t green, uint8_t blue);
    
    /**
    * @brief Sends current state feedback to KNX status KOs.
     */
    void sendStatusToKnx() override;
    
    /**
    * @brief Loop handler for continuous HCL updates.
    * Periodically checks interpolated HCL values and applies deltas.
     */
    void loop() override;
    
    /**
    * @brief Returns the Hue light resource ID.
     */
    String getLightId() const { return _lightId; }
    String getResourceId() const override { return _lightId; }
    
    /**
    * @brief Returns the user-visible light name.
     */
    String getName() const override { return _name; }
    
    /**
    * @brief State accessors.
     */
    bool isOn() const override { return _on; }
    uint8_t getBrightness() const override { return _brightness; }
    bool isReachable() const override { return _reachable; }
    uint16_t getColorTempKelvin() const { return _currentKelvin; }
    uint8_t getRed() const { return _currentRed; }
    uint8_t getGreen() const { return _currentGreen; }
    uint8_t getBlue() const { return _currentBlue; }
    
    /**
    * @brief Assigns HCL master (0 = none, 1-16 = master number).
     */
    void setHCLMaster(uint8_t masterNum) override { _hclMasterNum = masterNum; }
    
    /**
    * @brief Returns assigned HCL master number.
     */
    uint8_t getHCLMaster() const override { return _hclMasterNum; }

    void setHCLChannelLock(bool lockActive) override { _hclChannelLockActive = lockActive; }
    bool isHCLChannelLocked() const override { return _hclChannelLockActive; }

    static unsigned long globalHclWriteNextAllowedMs();

    /**
    * @brief Sets ETS light type (0=switch,1=dimm,2=ct,3=rgb).
     */
    void setLightType(uint8_t lightType) { _lightType = lightType; }

    /**
    * @brief Sets minimum allowed brightness in percent (0-100).
     */
    void setMinBrightness(uint8_t minBrightness) override;

    void setSwitchTransitionDurations(uint8_t onTransitionSec, uint8_t offTransitionSec) override;
    uint8_t getSwitchOnTransitionSec() const override { return _switchOnTransitionSec; }
    uint8_t getSwitchOffTransitionSec() const override { return _switchOffTransitionSec; }

    void setGroupedTarget(bool groupedTarget) override { _isGroupedTarget = groupedTarget; }
    bool isGroupedTarget() const override { return _isGroupedTarget; }
    unsigned long getLastHueWriteSuccessMs() const override { return _lastHueWriteSuccessMs; }

private:
    String _lightId;
    String _name;
    HueGatewayClient* _client;
    
    // KNX communication object numbers.
    uint16_t _koSwitch;
    uint16_t _koBrightness;
    uint16_t _koDimming;
    uint16_t _koStatusSwitch;
    uint16_t _koStatusBrightness;
    uint16_t _koStatusColorTemp;
    uint16_t _koStatusColorRGB;
    
    // Cached runtime state.
    bool _on;
    uint8_t _brightness;  // 0-254 (Hue API Range)
    bool _reachable;
    uint8_t _currentRed;
    uint8_t _currentGreen;
    uint8_t _currentBlue;
    uint8_t _lightType;
    uint8_t _minBrightnessPercent;
    uint8_t _minBrightnessHue;
    uint8_t _lastNonZeroBrightnessHue;
    bool _isGroupedTarget;
    
    // HCL configuration and current interpolation state.
    uint8_t _hclMasterNum;  // 0 = no HCL, 1-16 = HCL master number
    bool _hclChannelLockActive;
    bool _fadingActive;     // true while a fade transition is active
    uint16_t _currentKelvin; // Current color temperature in Kelvin
    unsigned long _lastHCLUpdate; // Timestamp of last HCL update (millis)
    uint8_t _lastHCLBrightness; // Last applied HCL brightness (%)
    uint16_t _hclPhaseOffsetMs;
    unsigned long _nextHCLDueMs;

    // Pending HCL value pushed from LightManagerModule
    uint16_t _pendingHclKelvin;
    uint8_t  _pendingHclBrightness;
    uint8_t  _pendingHclFadeDuration;
    bool     _hclValuePending;
    unsigned long _lastHueWriteSuccessMs;
    
    // Lifecycle flags.
    bool _initialized;
    unsigned long _lastUpdate;
    unsigned long _lastRelativeDimCmdMs;
    unsigned long _relativeDimCooldownUntilMs;
    uint8_t _relativeDimErrorStreak;
    bool _relativeDimHoldActive;
    bool _relativeDimHoldBrighter;
    uint8_t _relativeDimHoldSteps;
    unsigned long _relativeDimNextMs;
    uint8_t _switchOnTransitionSec;
    uint8_t _switchOffTransitionSec;
    
    /**
    * @brief Sends current on/brightness state to Hue Bridge.
     */
    void sendToHue(uint8_t fadeDurationSec = 0);
    
    /**
    * @brief Sends current state including color temperature to Hue Bridge.
    * @param kelvin Color temperature in Kelvin (2000-6500)
    * @param fadeDuration Transition duration in seconds (0 = immediate)
     */
    void sendToHueWithColorTemp(uint16_t kelvin, uint8_t fadeDuration = 0);
    void applyRelativeDimmingLegacy(bool brighter, uint8_t steps);
    void applyRelativeDimmingCache(bool brighter, uint8_t steps);
    
    /**
    * @brief Converts Kelvin to mirek (micro reciprocal kelvin).
    * @param kelvin Color temperature in Kelvin (2000-6500)
    * @return mirek value (153-500)
     */
    static uint16_t kelvinToMirek(uint16_t kelvin);

    static void rgbToXy(uint8_t red, uint8_t green, uint8_t blue, float& x, float& y);
    
    /**
    * @brief Converts KNX brightness (0-255) to Hue scale (0-254).
     */
    uint8_t knxToHueBrightness(uint8_t knxValue);
    
    /**
    * @brief Converts Hue brightness (0-254) to KNX scale (0-255).
     */
    uint8_t hueToKnxBrightness(uint8_t hueValue);
};

