#pragma once

#include "HueGatewayDevice.h"
#include <knx.h>
#include "OpenKNX.h"

/**
 * @brief Hue switch/button channel (Taster/Schalter).
 *
 * Supports up to 4 buttons per physical switch and an optional rotary encoder.
 * Each button has two independent DPT-based function dropdowns: KurzTyp + LangTyp.
 * Sub-parameters (value, direction, scene number etc.) depend on the selected type.
 *
 * KurzTyp (short press): 0=Keine, 1=Schalten, 2=Dimmen, 3=Szene, 4=Schritt/Stop,
 *   5=Prozent, 6=Temperatur, 7=1-Byte, 8=2-Byte
 * LangTyp (long press): 0=Kein, 1=Schalten, 2=Dimmen Start/Stop, 3=Szene, 4=Fahren,
 *   5=Prozent, 6=Temperatur, 7=1-Byte, 8=2-Byte, 9=Schritt/Stop
 *
 * KO slot layout (per button, 2 slots each: Kurzdruck + Langdruck):
 *   Button 1: koBase+0, koBase+1
 *   Button 2: koBase+2, koBase+3
 *   Button 3: koBase+4, koBase+5
 *   Button 4: koBase+6, koBase+7
 *   Rotary:   koBase+8, koBase+9
 *   Reserved: koBase+10, koBase+11
 *
 * Device type discriminator: 2
 */
class HueGatewayButton : public HueGatewayDevice
{
public:
    static constexpr uint8_t MAX_BUTTONS = 4;

    /** Per-button configuration (KurzTyp + LangTyp + sub-values + runtime state) */
    struct ButtonConfig
    {
        uint8_t  kurzTyp        = 0;  // 0-8
        uint8_t  langTyp        = 0;  // 0-9
        // Kurzdruck sub-values
        uint8_t  kurzSchaltwert = 0;  // 0=Toggle, 1=Ein, 2=Aus
        bool     kurzDimUp      = true;
        uint8_t  kurzDimStep    = 4;  // ETS enum → step code via dimEnumToStep()
        uint8_t  kurzSceneNr    = 1;  // 1-64
        bool     kurzRichtung   = false; // Schritt/Stop: 0=Auf, 1=Ab
        uint8_t  kurzProzent    = 100;
        uint8_t  kurzTemp       = 21;
        uint8_t  kurzByte       = 0;
        uint16_t kurzWord       = 0;
        // Langdruck sub-values
        bool     langSchaltwert = false; // 0=Ein, 1=Aus
        bool     langDimUp      = true;
        uint8_t  langDimStep    = 4;
        uint8_t  langSceneNr    = 1;
        bool     langRichtung   = false; // Fahren/Schritt/Stop: 0=Auf, 1=Ab
        uint8_t  langProzent    = 0;
        uint8_t  langTemp       = 21;
        uint8_t  langByte       = 0;
        uint16_t langWord       = 0;
        // Runtime state
        bool     state          = false; // toggle state for Schalten
        bool     longActive     = false; // long press in progress
        String   serviceRid;

        bool hasKurz() const { return kurzTyp != 0; }
        bool hasLang() const { return langTyp != 0; }
    };

    /** Rotary encoder KNX function type */
    enum class RotaryFunction : uint8_t
    {
        Dimmen         = 0,
        Wertgeber      = 1,
        Lautstaerke    = 2,
        Farbtemperatur = 3,
        Lamelle        = 4,
    };

    /** Convert ETS DimStep enum (1-5) to DPT 3.007 step code */
    static uint8_t dimEnumToStep(uint8_t enumVal)
    {
        // 1=2%→7, 2=3%→6, 3=6%→5, 4=13%→4, 5=25%→3
        static const uint8_t map[] = {4, 7, 6, 5, 4, 3};
        return (enumVal >= 1 && enumVal <= 5) ? map[enumVal] : 4;
    }

    HueGatewayButton(const String& resourceId, const String& name, uint8_t buttonCount)
        : _resourceId(resourceId)
        , _name(name)
        , _buttonCount(buttonCount > MAX_BUTTONS ? MAX_BUTTONS : buttonCount)
        , _reachable(false)
        , _initialized(false)
        , _hasRotary(false)
        , _rotaryStepPercent(13)
        , _rotaryStepCode(4)
        , _rotaryValue(127)
    {
        for (uint8_t i = 0; i < MAX_BUTTONS; i++)
        {
            _koBtn[i][0] = 0;
            _koBtn[i][1] = 0;
            // _btnCfg[i] is default-initialized by ButtonConfig defaults
        }
        _rotaryKo[0]       = 0;
        _rotaryKo[1]       = 0;
        _rotaryFunction    = RotaryFunction::Dimmen;
        _rotaryServiceRid  = "";
    }

    HueChannelType deviceType() const override { return HueChannelType::Taster; }
    String getResourceId() const override { return _resourceId; }
    String getName() const override { return _name; }
    bool isReachable() const override { return _reachable; }
    uint8_t getButtonCount() const { return _buttonCount; }

    /**
     * @brief Initializes KO mapping (2 KO slots per button, 2 for rotary).
     * koBase is the first KO of this channel's block.
     */
    void begin(uint16_t koBase)
    {
        for (uint8_t i = 0; i < MAX_BUTTONS; i++)
        {
            _koBtn[i][0] = koBase + i * 2;
            _koBtn[i][1] = koBase + i * 2 + 1;
        }
        _rotaryKo[0] = koBase + 8;
        _rotaryKo[1] = koBase + 9;
        _initialized = true;
    }

    /** Direct access to per-button config for initialization from ETS parameters. */
    ButtonConfig& buttonConfig(uint8_t idx) { return _btnCfg[idx < MAX_BUTTONS ? idx : 0]; }

    void setHasRotary(bool hasRotary) { _hasRotary = hasRotary; }
    void setRotaryFunction(RotaryFunction fn) { _rotaryFunction = fn; }
    void setRotaryStepPercent(uint8_t enumVal)
    {
        static const uint8_t pctMap[]  = {0, 2, 3, 6, 13, 25};
        static const uint8_t stepMap[] = {4, 7, 6, 5,  4,  3};
        _rotaryStepPercent = (enumVal >= 1 && enumVal <= 5) ? pctMap[enumVal]  : 13;
        _rotaryStepCode    = (enumVal >= 1 && enumVal <= 5) ? stepMap[enumVal] :  4;
    }
    /** Correctly encode and send a DPT 3.007 value (control bit + step code). */
    static void sendDpt3(uint16_t koNum, bool increase, uint8_t stepCode)
    {
        auto& ko = knx.getGroupObject(koNum);
        ko.valueNoSend(increase, Dpt(3, 7, 0));
        ko.value(static_cast<uint8_t>(stepCode & 0x07), Dpt(3, 7, 1));
    }
    /** Map DPT 3.007 step code (1-7) to approximate percent string for logging. */
    static const char* stepCodePct(uint8_t code)
    {
        static const char* map[] = {"stop", "100%", "50%", "25%", "13%", "6%", "3%", "2%"};
        return (code <= 7) ? map[code] : "?";
    }

    void updateReachable(bool reachable) { _reachable = reachable; }

    // ---- Service RID management ----

    /**
     * @brief Stores the service RID for button index 0-3.
     * This is the "button" service RID from the Hue device's services list.
     */
    void setButtonServiceRid(uint8_t idx, const String& rid)
    {
        if (idx < MAX_BUTTONS) _btnCfg[idx].serviceRid = rid;
    }
    void setRotaryServiceRid(const String& rid) { _rotaryServiceRid = rid; }

    /**
     * @brief Returns the button index (0-based) whose service RID matches the given rid,
     *        or -1 if not matched. outIsRotary is set true if the rotary matches.
     */
    int matchServiceRid(const String& rid, bool& outIsRotary) const
    {
        outIsRotary = false;
        if (_rotaryServiceRid.length() > 0
            && _rotaryServiceRid.equalsIgnoreCase(rid))
        {
            outIsRotary = true;
            return 0;
        }
        for (uint8_t i = 0; i < MAX_BUTTONS; i++)
        {
            if (_btnCfg[i].serviceRid.length() > 0
                && _btnCfg[i].serviceRid.equalsIgnoreCase(rid))
                return i;
        }
        return -1;
    }

    bool matchesEventRid(const String& rid) const override
    {
        bool dummy;
        return matchServiceRid(rid, dummy) >= 0;
    }

    // ---- Event handling ----

    /**
     * @brief Handles a button SSE event for the button with the given Hue control_id.
     * @param hueControlId  Hue metadata.control_id (1-based from API, mapped to 0-based index)
     * @param eventType     "initial_press", "repeat", "short_release", "long_release"
     */
    void handleButtonEvent(int hueControlId, const String& eventType)
    {
        if (!_initialized) return;
        // Hue control_id is 1-based; map to 0-based
        const int idx = hueControlId - 1;
        if (idx < 0 || idx >= (int)_buttonCount) return;

        if (eventType == "initial_press")
        {
            _btnCfg[idx].longActive = false;
            // No KNX send yet — wait for repeat or release
        }
        else if (eventType == "repeat")
        {
            if (_btnCfg[idx].hasLang() && !_btnCfg[idx].longActive)
            {
                _btnCfg[idx].longActive = true;
                sendLongPressStart(static_cast<uint8_t>(idx));
            }
        }
        else if (eventType == "short_release")
        {
            if (!_btnCfg[idx].longActive && _btnCfg[idx].hasKurz())
                sendShortPress(static_cast<uint8_t>(idx));
            // else: long press was active, stop will come via long_release
        }
        else if (eventType == "long_release")
        {
            if (_btnCfg[idx].longActive)
            {
                // repeat was received earlier → dimming/move already started, now stop
                sendLongPressStop(static_cast<uint8_t>(idx));
            }
            else if (_btnCfg[idx].hasLang())
            {
                // No repeat event was received (some Hue devices skip it),
                // but bridge detected a long press → execute start+stop for one step
                sendLongPressStart(static_cast<uint8_t>(idx));
                sendLongPressStop(static_cast<uint8_t>(idx));
            }
            else if (_btnCfg[idx].hasKurz())
            {
                sendShortPress(static_cast<uint8_t>(idx));
            }
            _btnCfg[idx].longActive = false;
        }
    }

    /**
     * @brief Handles a relative_rotary SSE event.
     * @param clockwise   true = clock_wise
     * @param steps       step count from Hue API
     */
    /**
     * @brief Called from processGroupObject when a value arrives on the rotary KO (koType=8)
     *        from the bus — e.g. actuator status response or GroupValueRead response on init.
     *        Updates the internal tracking value so the next rotation starts from the correct position.
     */
    void handleRotaryStatusKo(GroupObject& ko)
    {
        if (!_hasRotary) return;
        using RF = RotaryFunction;
        switch (_rotaryFunction)
        {
            case RF::Wertgeber:
            case RF::Lamelle:
                _rotaryValue = static_cast<uint8_t>(ko.value(Dpt(5, 1)));
                logDebug("HueGatewayButton", "%s Rotary sync from KO -> %u",
                         _name.c_str(), _rotaryValue);
                break;
            case RF::Farbtemperatur:
                _rotaryValue16 = static_cast<uint16_t>(ko.value(Dpt(7, 600)));
                logDebug("HueGatewayButton", "%s Rotary CT sync from KO -> %u K",
                         _name.c_str(), _rotaryValue16);
                break;
            default:
                // RF::Dimmen, RF::Lautstaerke use DPT 3.007 (relative) — no state to sync
                break;
        }
    }

    void handleRotaryEvent(bool clockwise, int steps)
    {
        if (!_initialized || !_hasRotary || _rotaryKo[0] == 0) return;

        const bool invert = false;  // Rotary has no invert option
        const bool up = clockwise ^ invert;

        // Fixed delta per event: _rotaryStepPercent% of 254 (DPT 5.001 full scale).
        // Hue 'steps' is encoder tick count per burst — irrelevant for the step size.
        const int delta = (_rotaryStepPercent * 254 + 50) / 100;

        using RF = RotaryFunction;
        switch (_rotaryFunction)
        {
            case RF::Dimmen:
            case RF::Lautstaerke:
            {
                // Relative dimming — send step command. DPT 3.007 step code defines
                // the number of intervals (100%/2^(code-1)), actuator stops on its own.
                sendDpt3(_rotaryKo[0], up, _rotaryStepCode);
                logDebug("HueGatewayButton", "%s - Rotary %s DPT3.007 code=%u(%s)",
                         _name.c_str(), up ? "+" : "-", _rotaryStepCode, stepCodePct(_rotaryStepCode));
                break;
            }
            case RF::Wertgeber:
            case RF::Lamelle:
            {
                int newVal = (int)_rotaryValue + (up ? delta : -delta);
                if (newVal < 0)   newVal = 0;
                if (newVal > 254) newVal = 254;
                _rotaryValue = static_cast<uint8_t>(newVal);
                knx.getGroupObject(_rotaryKo[0]).value(_rotaryValue, Dpt(5, 1));
                logDebug("HueGatewayButton", "%s - Rotary %s -> KNX DPT5 %u",
                         _name.c_str(), up ? "+" : "-", _rotaryValue);
                break;
            }
            case RF::Farbtemperatur:
            {
                // DPT 7.600 (2-byte unsigned, Kelvin 2000-6536)
                int newVal = (int)_rotaryValue16 + (up ? steps * (int)_rotaryStepPercent * 50 / 5
                                                        : -(steps * (int)_rotaryStepPercent * 50 / 5));
                if (newVal < 2000) newVal = 2000;
                if (newVal > 6536) newVal = 6536;
                _rotaryValue16 = static_cast<uint16_t>(newVal);
                knx.getGroupObject(_rotaryKo[0]).value(_rotaryValue16, Dpt(7, 600));
                logDebug("HueGatewayButton", "%s - Rotary CT %s -> KNX DPT7.600 %u K",
                         _name.c_str(), up ? "+" : "-", _rotaryValue16);
                break;
            }
        }
    }

private:
    String _resourceId;
    String _name;
    uint8_t _buttonCount;
    bool _reachable;
    bool _initialized;

    // Per-button state (KurzTyp + LangTyp config + runtime)
    uint16_t _koBtn[MAX_BUTTONS][2];     // [btn][0=Kurzdruck KO, 1=Langdruck KO]
    ButtonConfig _btnCfg[MAX_BUTTONS];

    // Rotary state
    bool _hasRotary;
    RotaryFunction _rotaryFunction;
    uint8_t _rotaryStepPercent;
    uint8_t _rotaryStepCode;
    uint16_t _rotaryKo[2];
    String _rotaryServiceRid;
    uint8_t _rotaryValue   = 127;        // current absolute value (DPT 5.001)
    uint16_t _rotaryValue16 = 4000;      // current absolute value (DPT 7.600, Kelvin)

    // ---- Action dispatch ----

    void sendShortPress(uint8_t idx)
    {
        auto& cfg = _btnCfg[idx];
        const uint16_t ko = _koBtn[idx][0]; // Kurzdruck KO
        if (!ko) return;

        switch (cfg.kurzTyp)
        {
            case 0: break; // Keine Aktion

            case 1: // Schalten (DPT 1.001)
                switch (cfg.kurzSchaltwert)
                {
                    case 0: // Toggle
                        cfg.state = !cfg.state;
                        knx.getGroupObject(ko).value(cfg.state, Dpt(1, 1));
                        logDebug("HueGatewayButton", "%s btn%u Kurz Schalten toggle -> %d",
                                 _name.c_str(), idx+1, cfg.state ? 1 : 0);
                        break;
                    case 1: // Ein
                        cfg.state = true;
                        knx.getGroupObject(ko).value(true, Dpt(1, 1));
                        logDebug("HueGatewayButton", "%s btn%u Kurz Schalten -> Ein", _name.c_str(), idx+1);
                        break;
                    case 2: // Aus
                        cfg.state = false;
                        knx.getGroupObject(ko).value(false, Dpt(1, 1));
                        logDebug("HueGatewayButton", "%s btn%u Kurz Schalten -> Aus", _name.c_str(), idx+1);
                        break;
                }
                break;

            case 2: // Dimmen relativ (DPT 3.007)
            {
                const uint8_t step = dimEnumToStep(cfg.kurzDimStep);
                sendDpt3(ko, cfg.kurzDimUp, step);
                logDebug("HueGatewayButton", "%s btn%u Kurz Dimmen %s step=%u(%s)",
                         _name.c_str(), idx+1, cfg.kurzDimUp ? "+" : "-", step, stepCodePct(step));
                break;
            }

            case 3: // Szene (DPT 18.001)
            {
                const uint8_t sceneVal = 0x80 | ((cfg.kurzSceneNr - 1) & 0x3F);
                knx.getGroupObject(ko).value(sceneVal, Dpt(18, 1));
                logDebug("HueGatewayButton", "%s btn%u Kurz Szene %u", _name.c_str(), idx+1, cfg.kurzSceneNr);
                break;
            }

            case 4: // Schritt/Stop (DPT 1.007)
                knx.getGroupObject(ko).value(cfg.kurzRichtung, Dpt(1, 7));
                logDebug("HueGatewayButton", "%s btn%u Kurz Schritt %s",
                         _name.c_str(), idx+1, cfg.kurzRichtung ? "Ab" : "Auf");
                break;

            case 5: // Prozent (DPT 5.001)
                knx.getGroupObject(ko).value(cfg.kurzProzent, Dpt(5, 1));
                logDebug("HueGatewayButton", "%s btn%u Kurz Prozent %u%%", _name.c_str(), idx+1, cfg.kurzProzent);
                break;

            case 6: // Temperatur (DPT 9.001)
                knx.getGroupObject(ko).value((float)cfg.kurzTemp, Dpt(9, 1));
                logDebug("HueGatewayButton", "%s btn%u Kurz Temp %u\xC2\xB0C", _name.c_str(), idx+1, cfg.kurzTemp);
                break;

            case 7: // 1-Byte (DPT 5.010)
                knx.getGroupObject(ko).value(cfg.kurzByte, Dpt(5, 10));
                logDebug("HueGatewayButton", "%s btn%u Kurz 1-Byte %u", _name.c_str(), idx+1, cfg.kurzByte);
                break;

            case 8: // 2-Byte (DPT 7.001)
                knx.getGroupObject(ko).value(cfg.kurzWord, Dpt(7, 1));
                logDebug("HueGatewayButton", "%s btn%u Kurz 2-Byte %u", _name.c_str(), idx+1, cfg.kurzWord);
                break;
        }
    }

    void sendLongPressStart(uint8_t idx)
    {
        auto& cfg = _btnCfg[idx];
        const uint16_t ko = _koBtn[idx][1]; // Langdruck KO
        if (!ko) return;

        switch (cfg.langTyp)
        {
            case 0: break; // Kein Langdruck

            case 1: // Schalten (DPT 1.001)
                knx.getGroupObject(ko).value(!cfg.langSchaltwert, Dpt(1, 1));
                logDebug("HueGatewayButton", "%s btn%u Lang Schalten -> %s",
                         _name.c_str(), idx+1, cfg.langSchaltwert ? "Aus" : "Ein");
                break;

            case 2: // Dimmen Start/Stop (DPT 3.007) — start
            {
                const uint8_t step = dimEnumToStep(cfg.langDimStep);
                sendDpt3(ko, cfg.langDimUp, step);
                logDebug("HueGatewayButton", "%s btn%u Lang Dimmen start %s step=%u(%s)",
                         _name.c_str(), idx+1, cfg.langDimUp ? "+" : "-", step, stepCodePct(step));
                break;
            }

            case 3: // Szene (DPT 18.001)
            {
                const uint8_t sceneVal = 0x80 | ((cfg.langSceneNr - 1) & 0x3F);
                knx.getGroupObject(ko).value(sceneVal, Dpt(18, 1));
                logDebug("HueGatewayButton", "%s btn%u Lang Szene %u", _name.c_str(), idx+1, cfg.langSceneNr);
                break;
            }

            case 4: // Fahren (DPT 1.008)
                knx.getGroupObject(ko).value(cfg.langRichtung, Dpt(1, 8));
                logDebug("HueGatewayButton", "%s btn%u Lang Fahren %s",
                         _name.c_str(), idx+1, cfg.langRichtung ? "Ab" : "Auf");
                break;

            case 5: // Prozent (DPT 5.001)
                knx.getGroupObject(ko).value(cfg.langProzent, Dpt(5, 1));
                logDebug("HueGatewayButton", "%s btn%u Lang Prozent %u%%", _name.c_str(), idx+1, cfg.langProzent);
                break;

            case 6: // Temperatur (DPT 9.001)
                knx.getGroupObject(ko).value((float)cfg.langTemp, Dpt(9, 1));
                logDebug("HueGatewayButton", "%s btn%u Lang Temp %u\xC2\xB0C", _name.c_str(), idx+1, cfg.langTemp);
                break;

            case 7: // 1-Byte (DPT 5.010)
                knx.getGroupObject(ko).value(cfg.langByte, Dpt(5, 10));
                logDebug("HueGatewayButton", "%s btn%u Lang 1-Byte %u", _name.c_str(), idx+1, cfg.langByte);
                break;

            case 8: // 2-Byte (DPT 7.001)
                knx.getGroupObject(ko).value(cfg.langWord, Dpt(7, 1));
                logDebug("HueGatewayButton", "%s btn%u Lang 2-Byte %u", _name.c_str(), idx+1, cfg.langWord);
                break;

            case 9: // Schritt/Stop (DPT 1.007)
                knx.getGroupObject(ko).value(cfg.langRichtung, Dpt(1, 7));
                logDebug("HueGatewayButton", "%s btn%u Lang Schritt %s",
                         _name.c_str(), idx+1, cfg.langRichtung ? "Ab" : "Auf");
                break;
        }
    }

    void sendLongPressStop(uint8_t idx)
    {
        const auto& cfg = _btnCfg[idx];
        const uint16_t ko = _koBtn[idx][1]; // Langdruck KO
        if (!ko) return;

        // Only Dimmen Start/Stop (langTyp==2) needs a stop telegram
        if (cfg.langTyp == 2)
        {
            sendDpt3(ko, false, 0); // DPT 3.007 stop
            logDebug("HueGatewayButton", "%s btn%u Lang Dimmen stop", _name.c_str(), idx+1);
        }
    }
};
