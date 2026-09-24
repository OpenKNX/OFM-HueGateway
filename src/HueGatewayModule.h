#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <vector>
#include "OpenKNX.h"
#include "knxprod.h"
#include "HueGatewayAuth.h"
#include "HCL/HCLMasterManager.h"
#include "Devices/HueGatewayDevice.h"

// Forward Declarations
class HueGatewayClient;
class HueGatewayLight;
class HueGatewayPlug;
namespace OpenKNX { namespace Network { class WebRequest; class WebResponse; struct WebSocketFrame; } }
struct HueGatewayEventLightUpdate;
struct HueGatewayEventSensorUpdate;

/**
 * @brief OpenKNX Hue Bridge Module - Philips Hue Integration
 * 
 * This module integrates Philips Hue devices into KNX systems.
 * It communicates with the Hue Bridge via Hue API v2 and exposes device
 * functions through KNX communication objects.
 * 
 * @version 0.4.0
 * @date 2026-04-16
 */

class HueGatewayModule : public OpenKNX::Module
{
public:
    HueGatewayModule();
    ~HueGatewayModule();
    
    // OpenKNX::Module Interface
    const std::string name() override;
    const std::string version() override;
    void setup() override;
    void loop() override;
    void processInputKo(GroupObject& ko) override;
    bool processCommand(const std::string cmd, bool diagnoseKo) override;
    uint16_t flashSize() override;
    void writeFlash() override;
    void readFlash(const uint8_t* data, const uint16_t size) override;
    bool processFunctionProperty(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t* data, uint8_t* resultData, uint8_t& resultLength) override;

    enum class HclLockFallbackMode : uint8_t {
        None = 0,
        Min1 = 1,
        Min2 = 2,
        Min5 = 3,
        Min10 = 4,
        Min20 = 5,
        Min30 = 6,
        Hour1 = 7,
        Hour2 = 8,
        Hour5 = 9,
        Hour8 = 10,
        Hour12 = 11,
        NextDay = 12
    };

    enum class HclLockFallbackPolicy : uint8_t {
        Legacy = 0,
        Duration = 1,
        TimeOfDay = 2,
        DurationOrTime = 3,
        ExternalOnly = 4
    };

#ifdef OPENKNX_WEBSERVER
    void markChannelDirty(int channel);
#endif

private:
    struct DiagnosticLogEntry {
        unsigned long uptimeMs;
        String level;
        String category;
        String message;
    };

    bool _initialized;
    unsigned long _lastConnectionCheckMs;
    unsigned long _bootStartMs;
    unsigned long _lastRefreshTickMs;
    unsigned long _lastDeviceSetupRetryMs;
    unsigned long _deviceSetupRetryBackoffMs;
    unsigned long _lastEventStreamRetryMs;
    unsigned long _eventStreamRetryBackoffMs;
    unsigned long _eventStreamPauseUntilMs;
    uint8_t _eventStreamFailureCount;
    HueGatewayClient* _client;
    
    
    // Device Management
    static const int MAX_CHANNELS = 32;  ///< Max configurable channels (was MAX_LIGHTS=24)
    static const int MAX_LIGHTS = MAX_CHANNELS;  ///< Backward-compat alias
    HueGatewayDevice* _devices[MAX_CHANNELS];  ///< Polymorphic device array
    /// Helper: returns the channel as HueGatewayLight* or nullptr if not a light.
    HueGatewayLight* lightAt(int ch) const;

    /// Helper: returns the channel as HueGatewayPlug* or nullptr if not a plug.
    HueGatewayPlug* plugAt(int ch) const;
    unsigned long _channelLastPollMs[MAX_CHANNELS];
    unsigned long _channelFastTrackNextMs[MAX_CHANNELS];
    unsigned long _channelFastTrackCooldownUntilMs[MAX_CHANNELS];
    uint8_t _channelFastTrackRemaining[MAX_CHANNELS];
    unsigned long _pollBackoffUntilMs;
    unsigned long _pollBackoffMs;
    uint8_t _pollFailureCount;
    unsigned long _lastBridgeHealthOkMs;
    unsigned long _lastChannelSyncOkMs;
    // Round-robin cursor for chunked polling fallback.
    uint8_t _pollCursor;
    int _lightCount;
    
    // Status values
    enum class BridgeStatus {
        DISCONNECTED,
        CONNECTING,
        WAIT_FOR_BUTTON,
        AUTHENTICATING,
        CONNECTED,
        CONNECTION_LOST,
        BRIDGE_UNREACHABLE,
        ERROR
    };
    
    BridgeStatus _bridgeStatus;
    unsigned long _ledBlinkTime;

    // Authentication state
    HueGatewayAuth _auth;
    String _bridgeIP;
    bool _authPending;
    unsigned long _authStartTime;
    unsigned long _authLastTry;
    unsigned long _authWindowMs;
    unsigned long _lastReconnectTryMs;
    unsigned long _reconnectBackoffMs;
    unsigned long _setupCircuitOpenUntilMs;
    uint8_t _setupCircuitTrips;
    unsigned long _lastLoopBudgetLogMs;
    bool _manualPairingRequired;
    bool _pairingTriggerLastState;
    unsigned long _lastPairingTriggerMs;
    bool _devicesInitialized;
    bool _deviceSetupNeedsRetry;
    uint8_t _consecutiveEmptyLightFetches;
    uint32_t _diagCounterEmptyLightFetches;
    unsigned long _lastValidLightFetchMs;
    bool _biAutoDeletePending;             // behavior_instance SSE event seen, delete pending
    unsigned long _biAutoDeleteTriggerMs;   // millis() when SSE event was first seen (debounce)
    bool _webScanRequested;
    bool _webScanInProgress;
    bool _networkConnectedLast;
    bool _mdnsStarted;
    unsigned long _webScanStartedMs;
    unsigned long _lastWebScanDurationMs;
    unsigned long _lastWebScanMs;
    int _lastWebScanLightCount;
    int _lastWebScanAccessoryCount;
    String _lastWebScanHtml;
    String _lastWebScanText;
    String _lastWebScanError;

    // ETS-Inbetriebnahme: kompaktes Scan-Ergebnis für HueGatewayModule.script.js
    // (Function Property 0xA3/1). Befüllt in updateWebScanCache().
    struct CommissioningLight {
        String id;
        String name;
        String roomId;
        String roomName;
        String zoneId;
        String zoneName;
    };
    struct CommissioningAccessory {
        uint8_t type;  // 1 Taster, 2 Taster mit Drehelement, 3 Bewegungsmelder, 4 Kontaktsensor, 5 Steckdose, 6 Sonstiges
        String id;
        String name;
    };
    struct CommissioningScene {
        String id;
        String name;
        String groupName;
    };
    std::vector<CommissioningLight> _commLights;
    std::vector<CommissioningAccessory> _commAccessories;
    std::vector<CommissioningScene> _commScenes;
    uint8_t _commTruncatedFlags;  // Bit 0 Lichter, Bit 1 Zubehör, Bit 2 Szenen
    bool _etsScanPending;         // von der ETS angefordert, Ergebnis noch nicht fertig
    uint8_t _etsScanError;
    void clearCommissioningResult();

    bool _hclLockActive;
    uint8_t _hclLockFallbackMode;
    uint8_t _hclFallbackPolicy;
    uint32_t _hclFallbackDurationMs;
    uint16_t _hclFallbackReleaseMinuteOfDay;
    unsigned long _hclLockActivatedMs;

    // Scene Store: per-channel stored scene data (persisted to flash)
    struct SceneStoreData {
        uint8_t valid;       // 0x01 = valid, 0xFF = empty
        uint8_t onOff;       // 0 or 1
        uint8_t brightness;  // 0-100 %
        uint16_t colorTemp;  // Kelvin (2000-6500)
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };
    static const uint8_t SCENE_STORE_VALID = 0x01;
    static const uint8_t SCENE_STORE_EMPTY = 0xFF;
    static const uint8_t SCENE_STORE_VERSION = 2;
    static const int SCENE_SLOTS = 8;
    SceneStoreData _sceneStore[MAX_CHANNELS][SCENE_SLOTS];
    unsigned long _hclLockAutoReleaseMs;
    int16_t _hclLockActivationDayOfYear;
    int16_t _hclLockActivationMinuteOfDay;
    bool _hclManagerLockActive[HCL::MasterManager::MAX_MASTERS];
    uint8_t _hclManagerLockFallbackMode[HCL::MasterManager::MAX_MASTERS];
    uint8_t _hclManagerFallbackPolicy[HCL::MasterManager::MAX_MASTERS];
    uint32_t _hclManagerFallbackDurationMs[HCL::MasterManager::MAX_MASTERS];
    uint16_t _hclManagerFallbackReleaseMinuteOfDay[HCL::MasterManager::MAX_MASTERS];
    unsigned long _hclManagerLockActivatedMs[HCL::MasterManager::MAX_MASTERS];
    unsigned long _hclManagerLockAutoReleaseMs[HCL::MasterManager::MAX_MASTERS];
    int16_t _hclManagerLockActivationDayOfYear[HCL::MasterManager::MAX_MASTERS];
    int16_t _hclManagerLockActivationMinuteOfDay[HCL::MasterManager::MAX_MASTERS];
    bool _hclChannelLockActive[MAX_CHANNELS];
    uint8_t _hclChannelLockFallbackMode[MAX_CHANNELS];
    uint8_t _hclChannelAutoStandby[MAX_CHANNELS];
    uint8_t _hclChannelFallbackPolicy[MAX_CHANNELS];
    uint32_t _hclChannelFallbackDurationMs[MAX_CHANNELS];
    uint16_t _hclChannelFallbackReleaseMinuteOfDay[MAX_CHANNELS];
    unsigned long _hclChannelLockActivatedMs[MAX_CHANNELS];
    unsigned long _hclChannelLockAutoReleaseMs[MAX_CHANNELS];
    int16_t _hclChannelLockActivationDayOfYear[MAX_CHANNELS];
    int16_t _hclChannelLockActivationMinuteOfDay[MAX_CHANNELS];
    uint16_t _hclLastPublishedKelvin[HCL::MasterManager::MAX_MASTERS];
    uint8_t _hclLastPublishedBrightness[HCL::MasterManager::MAX_MASTERS];
    bool _hclMasterValuesPublished[HCL::MasterManager::MAX_MASTERS];
    unsigned long _hclLastPublishMs[HCL::MasterManager::MAX_MASTERS];

    // In-memory diagnostics (for WebUI support package export).
    std::vector<DiagnosticLogEntry> _diagLogRing;
    size_t _diagLogRingHead;
    size_t _diagLogRingCount;
    uint32_t _diagLogDropped;
    uint32_t _diagCounterSetupRuns;
    uint32_t _diagCounterSetupIncomplete;
    uint32_t _diagCounterUnresolvedTargets;
    uint32_t _diagCounterKoCommands;
    uint32_t _diagCounterKoBlockedSyncDir;
    uint32_t _diagCounterKoBlockedChannelMissing;
    uint32_t _diagCounterWebScanRuns;
    uint32_t _diagCounterWebScanTimeouts;
    unsigned long _diagLastKoCommandMs[MAX_CHANNELS];
    uint8_t _diagLastKoType[MAX_CHANNELS];
    String _diagLastKoValue[MAX_CHANNELS];
    String _diagLastKoBlockReason[MAX_CHANNELS];
    unsigned long _diagLastWriteTraceMs[MAX_CHANNELS];
    unsigned long _diagLastWriteTraceDurationMs[MAX_CHANNELS];
    int _diagLastWriteTraceHttpStatus[MAX_CHANNELS];
    String _diagLastWriteTraceResult[MAX_CHANNELS];
    String _diagLastWriteTraceMethod[MAX_CHANNELS];
    String _diagLastWriteTraceEndpoint[MAX_CHANNELS];
    bool _diagTargetUnresolved[MAX_CHANNELS];

    // Last setup snapshot for diagnostics.
    uint8_t _diagLastEnabledChannels;
    int _diagLastBridgeLightCount;
    int _diagLastRoomTargetCount;
    int _diagLastZoneTargetCount;
    bool _diagLastHasUnresolvedTarget;
    
    void setupBridge();
    void setupDevices();
    void setupHCL();
    void processBehaviorInstanceAutoDelete();
    void checkConnection();
    void refreshLightStatus(bool fastTrackOnly = false);
    void applyEventStreamUpdates(const HueGatewayEventLightUpdate* updates, int updateCount);
    void applyEventStreamDeviceUpdates(const HueGatewayEventSensorUpdate* updates, int updateCount);
    void setupMDNS();
    void refreshNetworkServices();
    void pollAuthentication();
    bool initClientWithAppKey();
    bool startPairing();

#ifdef OPENKNX_WEBSERVER
    void setupWebUI();
    void handleWebRoot(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebScan(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebScanText(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebStatus(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebPair(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebResetAuth(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebDiagnose(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebDiagnoseText(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebRetrySetup(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebFadeTest(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebLive(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleWebLichtmanager(OpenKNX::Network::WebRequest& req, OpenKNX::Network::WebResponse& res);
    void handleLiveWsConnect(int clientId, bool connected);
    void handleLiveWsMessage(int clientId, OpenKNX::Network::WebSocketFrame* frame);
    void flushLiveUpdates();
    void buildChannelStateJson(int channel, String& out);
    void sendLiveSnapshot(int clientFd);
    bool _liveDirty[MAX_CHANNELS];
    unsigned long _liveLastSentMs[MAX_CHANNELS];
    unsigned long _livePeriodicRefreshMs = 0;
#endif // OPENKNX_WEBSERVER

    // Scan functions
    void performBridgeScan();
    void updateWebScanCache();
    String getBridgeScanHTML();
    String getBridgeScanText();
    String buildDiagnosticReport(size_t requestedDepth, const String& testerNote, bool includeNetworkDetails);
    void appendDiagnosticLog(const char* level, const char* category, const String& message);
    void resetDevices();

    // Fade test: drives one channel through a fixed sequence of Hue payloads and
    // samples the resulting brightness, to determine which payload the bridge
    // actually ramps and whether it clobbers its stored brightness.
    enum class FadeTestPhase : uint8_t { Put, Sample, Settle };
    bool startFadeTest(int channelIndex, String& error);
    void loopFadeTest();
    void finishFadeTest();
    int8_t _fadeTestChannel = -1;
    uint8_t _fadeTestStep = 0;
    uint8_t _fadeTestSampleIndex = 0;
    FadeTestPhase _fadeTestPhase = FadeTestPhase::Put;
    unsigned long _fadeTestNextMs = 0;
    bool _fadeTestLockTaken = false;
    String _fadeTestReport;
    
    // Helper Methods
    bool hasNetworkConnectivity() const;
    String getBridgeIP();
    uint8_t countEnabledChannels() const;
    // Kanaltyp aus der ETS; Deaktiviert auch für Kanäle außerhalb des gültigen Bereichs.
    PT_HUEChannelType channelType(uint8_t channelIndex) const;
    // Kanal ist gewählt und nicht suspendiert.
    bool channelConfigured(uint8_t channelIndex) const;
    void setHclLock(bool active, const char* reason);
    void publishHclLockStatus();
    void setHclManagerLock(uint8_t managerNumber, bool active, const char* reason);
    void publishHclManagerLockStatus(uint8_t managerNumber);
    void setHclChannelLock(uint8_t channelIndex, bool active, const char* reason);
    void publishHclChannelLockStatus(uint8_t channelIndex);
    void publishHclMasterValues();
    void evaluateHclLockFallback(const tm* timeinfo, bool hasTime);
    void evaluateHclManagerLockFallback(const tm* timeinfo, bool hasTime);
    void evaluateHclChannelLockFallback(const tm* timeinfo, bool hasTime);
    uint32_t getHclFallbackDurationMs(HclLockFallbackMode mode) const;
    bool shouldReleaseByPolicyTime(int16_t activationDayOfYear, int16_t activationMinuteOfDay, uint16_t releaseMinuteOfDay, const tm* timeinfo, bool hasTime) const;
    static const char* hclFallbackModeToText(HclLockFallbackMode mode);
    static const char* hclFallbackPolicyToText(HclLockFallbackPolicy policy);
    
    // Status Methods
    void updateStatus(BridgeStatus status);
    void sendStatusKO(bool connected);
    void updateInfoLED();
};

// Global module instance used by OpenKNX module registration.
extern HueGatewayModule openknxHueGatewayModule;
