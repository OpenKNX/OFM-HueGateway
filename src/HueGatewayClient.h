#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <vector>

/**
 * @brief Hue API v2 Client
 * 
 * Communicates with the Hue Bridge using Hue API v2.
 * Supports:
 * - Fetching devices (lights, grouped lights, etc.)
 * - Controlling lights (on/off, brightness, color)
 * - Error handling and retry logic
 * 
 * API v2 documentation: https://developers.meethue.com/develop/hue-api-v2/
 */

struct HueGatewayLightState
{
    bool on;
    uint8_t brightness;  // 0-254 (Hue API Range)
    bool reachable;
    uint16_t colorTempKelvin;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    bool supportsColorTemp;
    bool supportsColor;
    String id;
    String name;
    String room;
    String roomRid;
    String roomIdV1;
    String zone;
    String zoneRid;
    String zoneIdV1;
};

struct HueGatewayEventLightUpdate
{
    String lightId;
    bool isGroupedResource;
    bool hasOn;
    bool on;
    bool hasBrightness;
    uint8_t brightness;
    bool hasColorTemp;
    uint16_t colorTempKelvin;
    bool hasColorRgb;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct HueGatewayTargetInfo
{
    String id;
    String name;
    String groupedLightId;
};

// ---- Sensor-Zustände für Nicht-Licht-Gerätetypen ----

struct HueGatewayMotionState
{
    String id;
    bool motionDetected;
    bool reachable;
    // Optionale Zusatzdaten (NaN / 255 wenn nicht verfügbar)
    float temperature;       // °C, NAN wenn nicht verfügbar
    float lightLevelLux;     // Lux, 0 wenn nicht verfügbar
    uint8_t batteryPercent;  // 0-100, 255 wenn nicht verfügbar

    HueGatewayMotionState()
        : motionDetected(false), reachable(true)
        , temperature(NAN), lightLevelLux(0.0f), batteryPercent(255) {}
};

struct HueGatewayContactState
{
    String id;
    bool contactOpen;        // true = geöffnet (no_contact), false = geschlossen (contact)
    bool reachable;
    // Optionale Zusatzdaten
    bool tampered;           // Sabotagekontakt ausgelöst
    float temperature;       // °C, NAN wenn nicht verfügbar
    uint8_t batteryPercent;  // 0-100, 255 wenn nicht verfügbar

    HueGatewayContactState()
        : contactOpen(false), reachable(true)
        , tampered(false), temperature(NAN), batteryPercent(255) {}
};

struct HueGatewayButtonState
{
    String id;
    int buttonIndex;        // 0-basierter Index der gedrückten Taste
    String lastEventType;   // "short_release", "long_release", "repeat", "initial_press"
};

/** Nicht-Leuchten-Gerät von der Bridge (Schalter, Bewegungsmelder, Kontaktsensor etc.) */
struct HueGatewayAccessoryDevice
{
    String id;    // Device Resource ID
    String name;  // Gerätename
    String type;  // z.B. "Taster", "Bewegungsmelder", "Kontaktsensor"
};

/** Hue-Szene von der Bridge */
struct HueGatewayScene
{
    String id;         // Scene Resource UUID (wird in ETS als RID eingetragen)
    String name;       // metadata.name
    String groupRid;   // group.rid — grouped_light UUID des zugehörigen Raums/Zone
    String groupName;  // Aufgelöster Raum-/Zonenname (oder "-")
};

/** Einzelner Service-Eintrag eines Hue-Geräts (z.B. button, motion, relative_rotary) */
struct HueGatewayServiceRid
{
    String rtype;  // z.B. "button", "motion", "contact_sensor", "relative_rotary"
    String rid;    // Service Resource ID (UUID)
};

/** Sensorereignis aus dem SSE-Eventstream (Bewegungsmelder, Kontakt, Taster, Drehregler) */
struct HueGatewayEventSensorUpdate
{
    enum class Type { Unknown, Motion, Contact, Button, Rotary };
    Type type;
    String resourceId;
    // Motion
    bool motionDetected;
    // Contact
    bool contactOpen;
    // Button
    int buttonIndex;
    String buttonEventType;
    // Rotary
    bool rotaryClockwise;
    int rotarySteps;
    int rotaryDurationMs;
};

class HueGatewayClient
{
public:
    struct DiagnosticsStats
    {
        uint32_t httpGetCount;
        uint32_t httpPutCount;
        uint32_t httpGetErrorCount;
        uint32_t httpPutErrorCount;
        uint32_t httpTimeoutCount;
        uint32_t eventConnectOk;
        uint32_t eventConnectFail;
        uint32_t eventStopCount;
        uint32_t eventDisconnectCount;
        uint32_t eventHandshakeTimeoutCount;
        uint32_t eventHttpErrorCount;
        int lastHttpStatusCode;
        String lastHttpMethod;
        String lastHttpEndpoint;
        String lastJsonError;
        int lastContentLength;
    };

    HueGatewayClient();
    ~HueGatewayClient();
    
    /**
    * @brief Initializes the client with bridge connection data.
    * @param bridgeIP Bridge IP address
    * @param appKey Application key (provided by HueGatewayAuth)
    * @return true on success
     */
    bool begin(const String& bridgeIP, const String& appKey);
    
    /**
    * @brief Fetches all lights from the bridge.
    * @param lights Array that receives light states
    * @param maxLights Maximum number of entries (array size)
    * @return number of discovered lights
     */
    int getLights(HueGatewayLightState* lights, int maxLights, bool includeLocations = true);
    int getGroupedLights(HueGatewayLightState* groupedLights, int maxLights);
    int getAccessoryDevices(HueGatewayAccessoryDevice* devices, int maxDevices);
    int getScenes(HueGatewayScene* scenes, int maxScenes);
    
    /**
    * @brief Switches a light on or off.
     * @param lightId Light Resource ID (z.B. "abc12345-...")
    * @param on true = on, false = off
    * @return true on success
     */
    bool setLightOnOff(const String& lightId, bool on);
    bool setGroupedLightOnOff(const String& groupedLightId, bool on);
    
    /**
    * @brief Sets the brightness of a light.
     * @param lightId Light Resource ID
    * @param brightness Brightness 0-254
    * @return true on success
     */
    bool setLightBrightness(const String& lightId, uint8_t brightness);
    bool setGroupedLightBrightness(const String& groupedLightId, uint8_t brightness);
    
    /**
    * @brief Sets on/off and brightness in one request.
     * @param lightId Light Resource ID
    * @param on true = on
    * @param brightness Brightness 0-254
    * @param fadeDurationSec Transition duration in seconds (0 = immediate)
    * @return true on success
     */
    bool setLightState(const String& lightId, bool on, uint8_t brightness, uint8_t fadeDurationSec = 0, bool includeOnState = true);
    bool setGroupedLightState(const String& groupedLightId, bool on, uint8_t brightness, uint8_t fadeDurationSec = 0, bool includeOnState = true);

    /**
    * @brief Applies relative dimming delta to a light.
    * @param lightId Light Resource ID
    * @param brighter true = up, false = down
    * @param steps KNX 3.007 step count (1-7)
    * @return true on success
     */
    bool setLightDimmingDelta(const String& lightId, bool brighter, uint8_t steps);
    bool setGroupedLightDimmingDelta(const String& groupedLightId, bool brighter, uint8_t steps);

    /**
    * @brief Stops an active relative dimming action on the light.
    * @param lightId Light Resource ID
    * @return true on success
     */
    bool stopLightDimming(const String& lightId);
    bool stopGroupedLightDimming(const String& groupedLightId);
    
    /**
    * @brief Sets on/off, brightness, and color temperature with fade duration.
     * @param lightId Light Resource ID
    * @param on true = on
    * @param brightness Brightness 0-254
    * @param mirek Color temperature in mirek (153-500, clamped)
    * @param fadeDurationSec Fade duration in seconds (0 = instant)
    * @return true on success
     */
    bool setLightStateWithColorTemp(const String& lightId, bool on, uint8_t brightness,
                                    uint16_t mirek, uint8_t fadeDurationSec, bool includeOnState = true);
    bool setGroupedLightStateWithColorTemp(const String& groupedLightId, bool on, uint8_t brightness,
                                           uint16_t mirek, uint8_t fadeDurationSec, bool includeOnState = true);
    
    /**
    * @brief Sets the color temperature of a light.
     * @param lightId Light Resource ID
    * @param mirek Color temperature in mirek (153-500, clamped)
    *              153 = cold/6536K, 500 = warm/2000K
    * @return true on success
     */
    bool setLightColorTemperature(const String& lightId, uint16_t mirek);
    bool setGroupedLightColorTemperature(const String& groupedLightId, uint16_t mirek);
    
    /**
    * @brief Sets the XY color of a light (CIE 1931 color space).
     * @param lightId Light Resource ID
    * @param x X coordinate (0.0-1.0, clamped)
    * @param y Y coordinate (0.0-1.0, clamped)
    * @return true on success
     */
    bool setLightColor(const String& lightId, float x, float y);
    bool setGroupedLightColor(const String& groupedLightId, float x, float y);

    bool pingBridgeApiV2();

    /**
    * @brief Closes an idle kept-alive REST connection. Call from the module loop.
    * Commands keep the TLS connection open so a series of them needs only one
    * handshake. Once idle it has to go: the TLS context occupies internal RAM, and
    * that is exactly what the EventStream needs to come up.
     */
    void loopIdleDisconnect();

    /**
    * @brief Frees the EventStream's TLS context so a failed request can be retried.
    * Only worth doing once a request has actually failed: internal RAM fits one TLS
    * context at a time, but giving the EventStream up in advance cost it on every
    * single request.
    * @return true when the context was freed, i.e. a retry makes sense
     */
    bool releaseEventStreamForRetry(const char* method, bool eventWasActive);

    /**
    * @brief Diagnostics only: sends a raw payload to a resource (fade test).
    * @param resourceType "light" or "grouped_light"
    */
    bool diagRawPut(const char* resourceType, const String& rid, const String& payload);

    /**
    * @brief Diagnostics only: reads on-state and brightness percent of a resource.
    */
    bool diagReadState(const char* resourceType, const String& rid, bool& on, uint8_t& brightnessPercent);

    bool startEventStream();
    void stopEventStream();
    bool isEventStreamConnected() { return _eventStreamConnected && _eventClient.connected(); }
    bool isEventStreamConnectPending() const { return _eventConnectPending; }
    void checkEventStreamConnect();
    void setEventStreamAutoRestartEnabled(bool enabled) { _eventAutoRestartEnabled = enabled; }
    bool isEventStreamAutoRestartEnabled() const { return _eventAutoRestartEnabled; }
    int pollEventStream(HueGatewayEventLightUpdate* updates, int maxUpdates);
    const DiagnosticsStats& getDiagnosticsStats() const { return _diagStats; }
    
    /**
    * @brief Converts Kelvin to mirek.
    * @param kelvin Color temperature in Kelvin (2000-6536)
    * @return mirek value (153-500)
     */
    static uint16_t kelvinToMirek(uint16_t kelvin);
    
    /**
    * @brief Converts mirek to Kelvin.
    * @param mirek Color temperature in mirek (153-500)
    * @return Kelvin value (2000-6536)
     */
    static uint16_t mirekToKelvin(uint16_t mirek);

    /**
    * @brief Converts KNX DPT 3.007 step code to a bounded practical dimming delta in percent.
    * Step code 1 = fastest practical dimming, 7 = slowest practical dimming.
    * @param steps Step code (0-7)
    * @return Brightness delta in percent
     */
    static float relativeDimmingDeltaPercent(uint8_t steps);
    
    /**
    * @brief Returns whether the client has been initialized.
     */
    bool isInitialized() const { return _initialized; }
    
    /**
    * @brief Returns the configured bridge IP.
     */
    String getBridgeIP() const { return _bridgeIP; }

    bool resolveGroupedLightForRoom(const String& roomRid, String& groupedLightRid, String& roomName);
    bool resolveGroupedLightForZone(const String& zoneRid, String& groupedLightRid, String& zoneName);
    int getRoomTargets(HueGatewayTargetInfo* targets, int maxTargets);
    int getZoneTargets(HueGatewayTargetInfo* targets, int maxTargets);

    // ---- Sensoren und Taster (Gerätetypen 1-3) ----

    /**
     * @brief Liest den aktuellen Zustand eines Bewegungsmelders.
     * @param motionRid Hue Resource ID des Motion-Sensors (/clip/v2/resource/motion)
     * @param state Ausgabe-Struktur
     * @return true bei Erfolg
     */
    bool getMotionState(const String& motionRid, HueGatewayMotionState& state);

    /**
     * @brief Liest den aktuellen Zustand eines Kontaktsensors.
     * @param contactRid Hue Resource ID des Contact-Sensors (/clip/v2/resource/contact_sensor)
     * @param state Ausgabe-Struktur
     * @return true bei Erfolg
     */
    bool getContactState(const String& contactRid, HueGatewayContactState& state);

    /**
     * @brief Liest den letzten Tastenzustand eines Taster-Geräts.
     * @param buttonRid Hue Resource ID des Tasters (/clip/v2/resource/button)
     * @param state Ausgabe-Struktur
     * @return true bei Erfolg
     */
    bool getButtonState(const String& buttonRid, HueGatewayButtonState& state);

    /**
     * @brief Wie pollEventStream, liefert aber zusätzlich Sensor-Ereignisse zurück.
     * @param lightUpdates Array für Licht-Ereignisse
     * @param maxLightUpdates Maximale Anzahl Licht-Ereignisse
     * @param sensorUpdates Array für Sensor-Ereignisse
     * @param maxSensorUpdates Maximale Anzahl Sensor-Ereignisse
     * @return Anzahl Licht-Ereignisse (sensorCount per Referenz)
     */
    int pollEventStreamFull(HueGatewayEventLightUpdate* lightUpdates, int maxLightUpdates,
                            HueGatewayEventSensorUpdate* sensorUpdates, int maxSensorUpdates,
                            int& sensorCount);

    // ---- Optionale Sensor-Zusatzdaten ----
    /** Liest Temperatur eines Geräts (über Besitzer-RID). Gibt NAN zurück falls nicht gefunden. */
    bool fetchTemperatureByOwner(const String& ownerRid, float& celsius);
    /** Liest Lux-Wert eines Geräts (über Besitzer-RID). */
    bool fetchLightLevelByOwner(const String& ownerRid, float& lux);
    /** Liest Batteriestand eines Geräts (über Besitzer-RID), 0-100%. */
    bool fetchBatteryByOwner(const String& ownerRid, uint8_t& percent);
    /** Liest Sabotage-Status eines Geräts (über Besitzer-RID). */
    bool fetchTamperByOwner(const String& ownerRid, bool& tampered);
    /** Liest Zigbee-Erreichbarkeit (über Besitzer-RID). */
    bool fetchZigbeeReachableByOwner(const String& ownerRid, bool& reachable);
    /** Ermittelt die Besitzer-RID (Device) aus einer beliebigen Resource-Response. */
    bool getOwnerRidFromResource(const String& resourceType, const String& resourceId, String& ownerRid);

    // ---- Szenensteuerung ----

    /**
     * @brief Ruft eine Hue-Szene auf der Bridge auf.
     * @param sceneRID Hue Scene Resource ID
     * @return true bei Erfolg
     */
    bool recallHueScene(const String& sceneRID);

    // ---- Geräte-Service-RIDs ----

    /**
     * @brief Liest alle Service-RIDs (button, motion, contact_sensor, relative_rotary …)
     *        eines Hue-Geräts aus der Bridge.
     * @param deviceId Device Resource ID (UUID, ohne Typ-Präfix)
     * @param out       Array der Ausgabe-Einträge
     * @param maxCount  Maximale Anzahl Einträge (Array-Größe)
     * @return Anzahl gefundener Service-RIDs (0 bei Fehler)
     */
    int getDeviceServiceRids(const String& deviceId, HueGatewayServiceRid* out, int maxCount);

    /**
     * @brief Returns the IDs of all behavior_instances associated with a device.
     * @param deviceId     Hue Device Resource ID
     * @param instanceIds  Array for output IDs
     * @param maxCount     Maximum number of entries (array size)
     * @return Number of found behavior_instance IDs (0 on error or none)
     */
    int getBehaviorInstances(const String& deviceId, String* instanceIds, int maxCount, String* debugInfo = nullptr);

    /**
     * @brief Deletes a behavior_instance from the bridge.
     * Used for "Hue Accessories" instances that don't support enabled=false.
     * @param instanceId  ID of the behavior_instance
     * @return true on success (HTTP 200)
     */
    bool deleteBehaviorInstance(const String& instanceId);

    /** Returns true if a behavior_instance add event was seen via SSE since last clear. */
    bool hasBehaviorInstanceEvent() const { return _behaviorInstanceEventPending; }
    void clearBehaviorInstanceEvent() { _behaviorInstanceEventPending = false; }

private:
    bool _initialized;
    String _bridgeIP;
    String _appKey;
    HTTPClient _http;
    WiFiClientSecure _secureClient;
    unsigned long _lastHttpActivityMs = 0; // 0 = no kept-alive connection open
    WiFiClientSecure _eventClient;
    bool _eventStreamConnected;
    bool _eventHandshakePending;
    unsigned long _eventHandshakeStartMs;
    unsigned long _eventLastDataMs;
    uint8_t _eventParseErrorStreak;
    uint32_t _eventDropCount;
    bool _eventAutoRestartEnabled;
    bool _behaviorInstanceEventPending;
    String _eventLineBuffer;
    String _eventDataBuffer;
    DiagnosticsStats _diagStats;

    // Async EventStream connect (FreeRTOS task on Core 0)
    volatile bool _eventConnectPending;
    volatile bool _eventConnectDone;
    volatile bool _eventConnectOk;
    volatile bool _eventConnectAbort;
    TaskHandle_t _eventConnectTask;
    static constexpr uint32_t kEventConnectStackSize = 12288u;
    static void sEventStreamConnectTask(void* param);

    struct LightLocation
    {
        String id;
        String room;
        String roomRid;
        String roomIdV1;
        String zone;
        String zoneRid;
        String zoneIdV1;
    };

    struct DeviceLightLink
    {
        String deviceId;
        String lightId;
    };
    
    /**
    * @brief Executes an HTTP GET request.
     * @param endpoint API Endpoint (z.B. "/clip/v2/resource/light")
    * @param doc JsonDocument for the response payload
     * @return HTTP Status Code
     */
    int httpGet(const String& endpoint, JsonDocument& doc, JsonDocument* filterDoc = nullptr, int timeoutMs = 2000, int nestingLimit = 10);
    
    /**
    * @brief Executes an HTTP PUT request.
     * @param endpoint API Endpoint
     * @param payload JSON Payload
     * @return HTTP Status Code
     */
    int httpPut(const String& endpoint, const String& payload);

    /**
    * @brief Executes an HTTP DELETE request.
     * @param endpoint API Endpoint
     * @return HTTP Status Code
     */
    int httpDelete(const String& endpoint);
    
    /**
    * @brief Builds the full request URL.
     */
    String buildUrl(const String& endpoint);
    static void xyToRgb(float x, float y, uint8_t& red, uint8_t& green, uint8_t& blue);
    int parseEventPayload(const String& payload, HueGatewayEventLightUpdate* updates, int maxUpdates);
    int parseEventPayloadFull(const String& payload,
                              HueGatewayEventLightUpdate* lightUpdates, int maxLightUpdates,
                              HueGatewayEventSensorUpdate* sensorUpdates, int maxSensorUpdates,
                              int& sensorCount);

    void appendDeviceLightLinksFromDoc(std::vector<DeviceLightLink>& links, const JsonDocument& doc);
    void appendLocationsFromDoc(std::vector<LightLocation>& locations,
                                const JsonDocument& doc,
                                bool isRoom,
                                const std::vector<DeviceLightLink>& deviceLightLinks);
    void upsertLocation(std::vector<LightLocation>& locations,
                        const String& id,
                        const String& room,
                        const String& roomRid,
                        const String& roomIdV1,
                        const String& zone,
                        const String& zoneRid,
                        const String& zoneIdV1);
    bool resolveGroupedLightForTarget(const String& endpoint, const String& targetRid, String& groupedLightRid, String& targetName);
    static bool extractGroupedLightRid(JsonObjectConst item, String& groupedLightRid);
    int getTargetsForEndpoint(const String& endpoint, HueGatewayTargetInfo* targets, int maxTargets);
};

