#include "HueGatewayClient.h"
#include <cstring>
#include <functional>
#include <memory>
#include <math.h>
#include <esp_heap_caps.h>
#include "OpenKNX.h"

namespace
{
static constexpr size_t kMaxEventLineChars = 4096U;
static constexpr size_t kMaxEventPayloadChars = 8192U;
#if defined(DEVICE_REG1_LAN_TP_BASE) || defined(DEVICE_DEV_REG1_LAN_TP_Base_V00_11)
static constexpr uint32_t kTlsMinInternalFreeBytes = 46000U;
static constexpr uint32_t kTlsMinInternalLargestBlockBytes = 32000U;
#else
static constexpr uint32_t kTlsMinInternalFreeBytes = 70000U;
static constexpr uint32_t kTlsMinInternalLargestBlockBytes = 50000U;
#endif
static constexpr unsigned long kLightLocationCacheMs = 300000UL;

bool isHttpSuccessStatus(int statusCode)
{
    return statusCode >= 200 && statusCode < 300;
}

bool hasTlsInternalHeadroom()
{
    const size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largestInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return freeInternal >= kTlsMinInternalFreeBytes && largestInternal >= kTlsMinInternalLargestBlockBytes;
}

void logHeapStats(const char* phase)
{
#ifdef OPENKNX_DEBUG
    const uint32_t freeHeap = ESP.getFreeHeap();
    const uint32_t minFreeHeap = ESP.getMinFreeHeap();
    const size_t free8Bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const size_t largest8Bit = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largestInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const uint32_t fragPercent = (free8Bit > 0 && largest8Bit <= free8Bit)
        ? static_cast<uint32_t>(((free8Bit - largest8Bit) * 100U) / free8Bit)
        : 0U;

    logDebug("HueGatewayClient", "HEAP %s free=%lu min=%lu free8=%u largest8=%u intFree=%u intLargest=%u frag=%lu%%",
             phase,
             static_cast<unsigned long>(freeHeap),
             static_cast<unsigned long>(minFreeHeap),
             static_cast<unsigned>(free8Bit),
             static_cast<unsigned>(largest8Bit),
             static_cast<unsigned>(freeInternal),
             static_cast<unsigned>(largestInternal),
             static_cast<unsigned long>(fragPercent));
#else
    (void)phase;
#endif
}

bool extractResourceRef(JsonVariantConst refVar, String& outRid, String& outType)
{
    outRid = "";
    outType = "";

    if (refVar.is<const char*>())
    {
        const char* rid = refVar.as<const char*>();
        if (rid != nullptr && rid[0] != '\0')
        {
            outRid = String(rid);
            return true;
        }
        return false;
    }

    JsonObjectConst ref = refVar.as<JsonObjectConst>();
    if (ref.isNull())
    {
        return false;
    }

    const char* rid = ref["rid"] | "";
    if (rid[0] == '\0') rid = ref["id"] | "";
    if (rid[0] == '\0') rid = ref["resource_identifier"]["rid"] | "";
    if (rid[0] == '\0') rid = ref["service"]["rid"] | "";
    if (rid[0] == '\0') rid = ref["target"]["rid"] | "";

    const char* rtype = ref["rtype"] | "";
    if (rtype[0] == '\0') rtype = ref["type"] | "";
    if (rtype[0] == '\0') rtype = ref["resource_identifier"]["rtype"] | "";
    if (rtype[0] == '\0') rtype = ref["service"]["rtype"] | "";
    if (rtype[0] == '\0') rtype = ref["target"]["rtype"] | "";

    if (rid[0] == '\0')
    {
        return false;
    }

    outRid = String(rid);
    if (rtype[0] != '\0')
    {
        outType = String(rtype);
    }
    return true;
}

int parseHttpStatusCode(const String& statusLine)
{
    int firstSpace = statusLine.indexOf(' ');
    if (firstSpace < 0 || firstSpace + 3 >= static_cast<int>(statusLine.length()))
    {
        return -1;
    }

    String codeStr = statusLine.substring(firstSpace + 1, firstSpace + 4);
    return codeStr.toInt();
}
}

HueGatewayClient::HueGatewayClient()
    : _initialized(false)
    , _eventStreamConnected(false)
    , _eventHandshakePending(false)
    , _eventHandshakeStartMs(0)
    , _eventLastDataMs(0)
    , _eventParseErrorStreak(0)
    , _eventDropCount(0)
    , _eventAutoRestartEnabled(true)
    , _behaviorInstanceEventPending(false)
    , _diagStats{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "", ""}
    , _eventConnectPending(false)
    , _eventConnectDone(false)
    , _eventConnectOk(false)
    , _eventConnectAbort(false)
    , _eventConnectTask(nullptr)
{
}

HueGatewayClient::~HueGatewayClient()
{
    stopEventStream();
    _http.end();
}

bool HueGatewayClient::begin(const String& bridgeIP, const String& appKey)
{
    if (bridgeIP.isEmpty() || appKey.isEmpty())
    {
        logError("HueGatewayClient", "Invalid Bridge IP or App Key");
        return false;
    }
    
    _bridgeIP = bridgeIP;
    _appKey = appKey;
    _initialized = true;
    _secureClient.setInsecure();
    _eventClient.setInsecure();
    _secureClient.setTimeout(2000);
    _eventClient.setTimeout(100);
    _eventLineBuffer.reserve(512);
    _eventDataBuffer.reserve(2048);
    _eventLineBuffer = "";
    _eventDataBuffer = "";
    _eventStreamConnected = false;
    _eventParseErrorStreak = 0;
    _eventLastDataMs = millis();
    _eventAutoRestartEnabled = true;
    
    logInfo("HueGatewayClient", "Initialized - Bridge: %s", _bridgeIP.c_str());
    return true;
}

int HueGatewayClient::getLights(HueGatewayLightState* lights, int maxLights, bool includeLocations)
{
    if (!_initialized)
    {
        logError("HueGatewayClient", "Not initialized");
        return 0;
    }

    DynamicJsonDocument doc(65536);
    DynamicJsonDocument lightFilterDoc(768);
    lightFilterDoc.clear();
    JsonObject lightFilterRoot = lightFilterDoc.to<JsonObject>();
    JsonObject lightFilterData = lightFilterRoot["data"][0].to<JsonObject>();
    lightFilterData["id"] = true;
    lightFilterData["metadata"]["name"] = true;
    lightFilterData["on"]["on"] = true;
    lightFilterData["owner"]["rid"] = true;
    lightFilterData["dimming"]["brightness"] = true;
    lightFilterData["color_temperature"]["mirek"] = true;
    lightFilterData["color"]["xy"]["x"] = true;
    lightFilterData["color"]["xy"]["y"] = true;
    doc.clear();
    int statusCode = httpGet("/clip/v2/resource/light", doc, &lightFilterDoc);

    if (!isHttpSuccessStatus(statusCode))
    {
        logError("HueGatewayClient", "GET lights failed - HTTP %d", statusCode);
        return 0;
    }

    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    std::vector<LightLocation> locations;

    if (includeLocations)
    {
        static std::vector<LightLocation> sCachedLocations;
        static unsigned long sLocationCacheValidUntilMs = 0UL;

        const unsigned long now = millis();
        const bool cacheValid = !sCachedLocations.empty()
            && sLocationCacheValidUntilMs != 0UL
            && now <= sLocationCacheValidUntilMs;

        if (cacheValid)
        {
            locations = sCachedLocations;
        }
        else
        {
            std::vector<DeviceLightLink> deviceLightLinks;
            deviceLightLinks.reserve(static_cast<size_t>(data.size()));

        // Build a direct device->light map from light payload first.
        // This is robust even if /device does not expose light refs on some bridge firmwares.
        for (JsonObjectConst light : data)
        {
            const char* lightRid = light["id"] | "";
            const char* ownerRid = light["owner"]["rid"] | "";
            if (lightRid[0] == '\0' || ownerRid[0] == '\0')
            {
                continue;
            }

            DeviceLightLink link;
            link.deviceId = String(ownerRid);
            link.lightId = String(lightRid);
            deviceLightLinks.push_back(link);
        }

        DynamicJsonDocument deviceDoc(32768);
        DynamicJsonDocument deviceFilterDoc(512);
        deviceFilterDoc.clear();
        JsonObject deviceFilterRoot = deviceFilterDoc.to<JsonObject>();
        JsonObject deviceFilterData = deviceFilterRoot["data"][0].to<JsonObject>();
        deviceFilterData["id"] = true;
        deviceFilterData["services"][0]["rid"] = true;
        deviceFilterData["services"][0]["rtype"] = true;
        deviceFilterData["children"][0]["rid"] = true;
        deviceFilterData["children"][0]["rtype"] = true;
        deviceFilterData["grouped_services"][0]["rid"] = true;
        deviceFilterData["grouped_services"][0]["rtype"] = true;
        deviceDoc.clear();
        int deviceStatus = httpGet("/clip/v2/resource/device", deviceDoc, &deviceFilterDoc);
        if (isHttpSuccessStatus(deviceStatus))
        {
            appendDeviceLightLinksFromDoc(deviceLightLinks, deviceDoc);
        }

        DynamicJsonDocument roomDoc(32768);
        DynamicJsonDocument locationFilterDoc(640);
        locationFilterDoc.clear();
        JsonObject locationFilterRoot = locationFilterDoc.to<JsonObject>();
        JsonObject locationFilterData = locationFilterRoot["data"][0].to<JsonObject>();
        locationFilterData["id"] = true;
        locationFilterData["id_v1"] = true;
        locationFilterData["metadata"]["name"] = true;
        locationFilterData["name"] = true;
        locationFilterData["children"][0]["rid"] = true;
        locationFilterData["children"][0]["rtype"] = true;
        locationFilterData["services"][0]["rid"] = true;
        locationFilterData["services"][0]["rtype"] = true;
        locationFilterData["grouped_services"][0]["rid"] = true;
        locationFilterData["grouped_services"][0]["rtype"] = true;
        roomDoc.clear();
        int roomStatus = httpGet("/clip/v2/resource/room", roomDoc, &locationFilterDoc);
        if (isHttpSuccessStatus(roomStatus))
        {
            appendLocationsFromDoc(locations, roomDoc, true, deviceLightLinks);
        }

        DynamicJsonDocument zoneDoc(32768);
        zoneDoc.clear();
        int zoneStatus = httpGet("/clip/v2/resource/zone", zoneDoc, &locationFilterDoc);
        if (isHttpSuccessStatus(zoneStatus))
        {
            appendLocationsFromDoc(locations, zoneDoc, false, deviceLightLinks);
        }

        const size_t roomItems = roomDoc["data"].is<JsonArrayConst>() ? roomDoc["data"].as<JsonArrayConst>().size() : 0;
        const size_t zoneItems = zoneDoc["data"].is<JsonArrayConst>() ? zoneDoc["data"].as<JsonArrayConst>().size() : 0;
        if (locations.empty() && (roomItems > 0 || zoneItems > 0))
        {
            logError("HueGatewayClient", "No room/zone mapping created (roomItems=%u zoneItems=%u links=%u)",
                     static_cast<unsigned>(roomItems),
                     static_cast<unsigned>(zoneItems),
                     static_cast<unsigned>(deviceLightLinks.size()));
        }

            if (!locations.empty())
            {
                sCachedLocations = locations;
                sLocationCacheValidUntilMs = now + kLightLocationCacheMs;
            }
            else if (!sCachedLocations.empty())
            {
                locations = sCachedLocations;
            }
        }
    }

    int count = 0;

    for (JsonObjectConst light : data)
    {
        if (count >= maxLights)
            break;

        lights[count].id = light["id"].as<String>();
        lights[count].name = light["metadata"]["name"].as<String>();
        lights[count].on = light["on"]["on"].as<bool>();
        String ownerRid = light["owner"]["rid"].as<String>();

        float brightnessPct = light["dimming"]["brightness"].as<float>();
        lights[count].brightness = static_cast<uint8_t>(roundf(brightnessPct * 2.54f));

        lights[count].reachable = light["owner"].isNull() == false;
        lights[count].supportsColorTemp = !light["color_temperature"].isNull();
        lights[count].supportsColor = !light["color"].isNull();

        lights[count].colorTempKelvin = 0;
        lights[count].red = 255;
        lights[count].green = 255;
        lights[count].blue = 255;

        JsonVariantConst mirekVar = light["color_temperature"]["mirek"];
        if (!mirekVar.isNull())
        {
            uint16_t mirek = mirekVar.as<uint16_t>();
            lights[count].colorTempKelvin = mirekToKelvin(mirek);
        }

        JsonVariantConst xVar = light["color"]["xy"]["x"];
        JsonVariantConst yVar = light["color"]["xy"]["y"];
        if (!xVar.isNull() && !yVar.isNull())
        {
            xyToRgb(xVar.as<float>(), yVar.as<float>(), lights[count].red, lights[count].green, lights[count].blue);
        }

        lights[count].room = "-";
        lights[count].roomRid = "-";
        lights[count].roomIdV1 = "-";
        lights[count].zone = "-";
        lights[count].zoneRid = "-";
        lights[count].zoneIdV1 = "-";

        for (const auto& location : locations)
        {
            if (location.id.equalsIgnoreCase(lights[count].id)
                || (ownerRid.length() > 0 && location.id.equalsIgnoreCase(ownerRid)))
            {
                if (location.room.length() > 0)
                {
                    lights[count].room = location.room;
                }
                if (location.roomRid.length() > 0)
                {
                    lights[count].roomRid = location.roomRid;
                }
                if (location.roomIdV1.length() > 0)
                {
                    lights[count].roomIdV1 = location.roomIdV1;
                }
                if (location.zone.length() > 0)
                {
                    lights[count].zone = location.zone;
                }
                if (location.zoneRid.length() > 0)
                {
                    lights[count].zoneRid = location.zoneRid;
                }
                if (location.zoneIdV1.length() > 0)
                {
                    lights[count].zoneIdV1 = location.zoneIdV1;
                }
                break;
            }
        }

        count++;
    }

    logInfo("HueGatewayClient", "Found lights: %d", count);
    return count;
}

int HueGatewayClient::getGroupedLights(HueGatewayLightState* groupedLights, int maxLights)
{
    if (!_initialized || groupedLights == nullptr || maxLights <= 0)
    {
        return 0;
    }

    DynamicJsonDocument doc(32768);
    DynamicJsonDocument groupedLightFilterDoc(640);
    groupedLightFilterDoc.clear();
    JsonObject groupedFilterRoot = groupedLightFilterDoc.to<JsonObject>();
    JsonObject groupedFilterData = groupedFilterRoot["data"][0].to<JsonObject>();
    groupedFilterData["id"] = true;
    groupedFilterData["metadata"]["name"] = true;
    groupedFilterData["on"]["on"] = true;
    groupedFilterData["dimming"]["brightness"] = true;
    groupedFilterData["color_temperature"]["mirek"] = true;
    groupedFilterData["color"]["xy"]["x"] = true;
    groupedFilterData["color"]["xy"]["y"] = true;
    doc.clear();
    int statusCode = httpGet("/clip/v2/resource/grouped_light", doc, &groupedLightFilterDoc);
    if (!isHttpSuccessStatus(statusCode))
    {
        logError("HueGatewayClient", "GET grouped_light failed - HTTP %d", statusCode);
        return 0;
    }

    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    int count = 0;
    for (JsonObjectConst item : data)
    {
        if (count >= maxLights)
        {
            break;
        }

        const char* id = item["id"] | "";
        if (id[0] == '\0')
        {
            continue;
        }

        groupedLights[count].id = String(id);
        groupedLights[count].name = item["metadata"]["name"].as<String>();
        groupedLights[count].on = item["on"]["on"].as<bool>();
        groupedLights[count].reachable = true;

        float brightnessPct = item["dimming"]["brightness"].as<float>();
        if (brightnessPct < 0.0f) brightnessPct = 0.0f;
        if (brightnessPct > 100.0f) brightnessPct = 100.0f;
        groupedLights[count].brightness = static_cast<uint8_t>(roundf(brightnessPct * 2.54f));

        groupedLights[count].supportsColorTemp = !item["color_temperature"].isNull();
        groupedLights[count].supportsColor = !item["color"].isNull();

        groupedLights[count].colorTempKelvin = 0;
        JsonVariantConst mirekVar = item["color_temperature"]["mirek"];
        if (!mirekVar.isNull())
        {
            groupedLights[count].colorTempKelvin = mirekToKelvin(mirekVar.as<uint16_t>());
        }

        groupedLights[count].red = 255;
        groupedLights[count].green = 255;
        groupedLights[count].blue = 255;
        JsonVariantConst xVar = item["color"]["xy"]["x"];
        JsonVariantConst yVar = item["color"]["xy"]["y"];
        if (!xVar.isNull() && !yVar.isNull())
        {
            xyToRgb(xVar.as<float>(), yVar.as<float>(), groupedLights[count].red, groupedLights[count].green, groupedLights[count].blue);
        }

        groupedLights[count].room = "-";
        groupedLights[count].roomRid = "-";
        groupedLights[count].roomIdV1 = "-";
        groupedLights[count].zone = "-";
        groupedLights[count].zoneRid = "-";
        groupedLights[count].zoneIdV1 = "-";

        count++;
    }

    logInfo("HueGatewayClient", "Found grouped_light targets: %d", count);
    return count;
}

int HueGatewayClient::getAccessoryDevices(HueGatewayAccessoryDevice* devices, int maxDevices)
{
    if (!_initialized || devices == nullptr || maxDevices <= 0)
        return 0;

    DynamicJsonDocument doc(32768);
    DynamicJsonDocument filterDoc(640);
    filterDoc.clear();
    JsonObject filterRoot = filterDoc.to<JsonObject>();
    JsonObject filterData = filterRoot["data"][0].to<JsonObject>();
    filterData["id"] = true;
    filterData["metadata"]["name"] = true;
    filterData["metadata"]["archetype"] = true;
    filterData["services"][0]["rid"] = true;
    filterData["services"][0]["rtype"] = true;
    doc.clear();
    int statusCode = httpGet("/clip/v2/resource/device", doc, &filterDoc);
    if (!isHttpSuccessStatus(statusCode))
        return 0;

    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    int count = 0;

    for (JsonObjectConst device : data)
    {
        if (count >= maxDevices)
            break;

        bool hasLight = false;
        String lightRid;
        String typeStr;
        const char* archetype = device["metadata"]["archetype"] | "";

        JsonArrayConst services = device["services"].as<JsonArrayConst>();
        for (JsonObjectConst svc : services)
        {
            const char* rtype = svc["rtype"] | "";
            if (strcmp(rtype, "light") == 0)
            {
                hasLight = true;
                const char* rid = svc["rid"] | "";
                if (rid[0] != '\0') lightRid = String(rid);
                continue;
            }
            if (strcmp(rtype, "zigbee_connectivity") == 0)  continue;
            if (strcmp(rtype, "device_software_update") == 0) continue;
            if (strcmp(rtype, "device_power") == 0)         continue;
            if (strcmp(rtype, "tamper") == 0)               continue;
            if (strcmp(rtype, "entertainment") == 0)        continue;

            const char* label = rtype;
            if      (strcmp(rtype, "button") == 0)          label = "Taster";
            else if (strcmp(rtype, "relative_rotary") == 0) label = "Drehelement";
            else if (strcmp(rtype, "motion") == 0)          label = "Bewegungsmelder";
            else if (strcmp(rtype, "contact_sensor") == 0)  label = "Kontaktsensor";
            else if (strcmp(rtype, "temperature") == 0)     label = "Temperatursensor";
            else if (strcmp(rtype, "light_level") == 0)     label = "Helligkeitssensor";

            if (typeStr.length() > 0) typeStr += ", ";
            typeStr += label;
        }

        // Smart Plugs: appear as light resources in the Hue API, detected via archetype
        if (hasLight)
        {
            const bool isPlug = (strstr(archetype, "plug") != nullptr
                                 || strstr(archetype, "socket") != nullptr
                                 || strstr(archetype, "outlet") != nullptr);
            if (!isPlug)
                continue;  // regular light, skip

            // Use the light resource ID so it can be copied directly into ETS config
            const char* id   = lightRid.length() > 0 ? lightRid.c_str() : (device["id"] | "");
            const char* name = device["metadata"]["name"] | "";
            if (id[0] == '\0') continue;
            devices[count].id   = String(id);
            devices[count].name = String(name);
            devices[count].type = "Steckdose";
            count++;
            continue;
        }

        if (typeStr.length() == 0) continue;

        const char* id   = device["id"] | "";
        const char* name = device["metadata"]["name"] | "";
        if (id[0] == '\0') continue;

        devices[count].id   = String(id);
        devices[count].name = String(name);
        devices[count].type = typeStr;
        count++;
    }

    logInfo("HueGatewayClient", "Found accessory devices: %d", count);
    return count;
}

int HueGatewayClient::getScenes(HueGatewayScene* scenes, int maxScenes)
{
    if (!_initialized || scenes == nullptr || maxScenes <= 0)
        return 0;

    // Build grouped_light RID → room/zone name mapping
    static const int kMaxGroups = 32;
    String groupRids[kMaxGroups];
    String groupNames[kMaxGroups];
    int groupCount = 0;

    // Helper: fetch a room or zone endpoint and collect room/zone ID → name pairs
    // Scene group.rid references the room/zone id directly (not a grouped_light service)
    auto fetchGroupNames = [&](const char* endpoint, const char* suffix) {
        DynamicJsonDocument gDoc(32768);
        DynamicJsonDocument gFilter(128);
        JsonObject gFRoot = gFilter.to<JsonObject>();
        JsonObject gFData = gFRoot["data"][0].to<JsonObject>();
        gFData["id"] = true;
        gFData["metadata"]["name"] = true;
        if (isHttpSuccessStatus(httpGet(endpoint, gDoc, &gFilter)))
        {
            for (JsonObjectConst group : gDoc["data"].as<JsonArrayConst>())
            {
                if (groupCount >= kMaxGroups) break;
                const char* gId   = group["id"] | "";
                const char* gName = group["metadata"]["name"] | "";
                if (gId[0] == '\0' || gName[0] == '\0') continue;
                groupRids[groupCount] = String(gId);
                groupNames[groupCount] = String(gName) + suffix;
                groupCount++;
            }
        }
    };

    fetchGroupNames("/clip/v2/resource/room", "");
    fetchGroupNames("/clip/v2/resource/zone", " (Zone)");
    logDebug("HueGatewayClient", "Scene group mapping: %d room/zone entries", groupCount);
    for (int g = 0; g < groupCount; g++)
    {
        logDebug("HueGatewayClient", "  room/zone %s -> %s", groupRids[g].c_str(), groupNames[g].c_str());
    }

    // Fetch scenes
    DynamicJsonDocument doc(32768);
    DynamicJsonDocument filterDoc(256);
    filterDoc.clear();
    JsonObject filterRoot = filterDoc.to<JsonObject>();
    JsonObject filterData = filterRoot["data"][0].to<JsonObject>();
    filterData["id"] = true;
    filterData["metadata"]["name"] = true;
    filterData["group"]["rid"] = true;
    filterData["group"]["rtype"] = true;
    doc.clear();
    const int statusCode = httpGet("/clip/v2/resource/scene", doc, &filterDoc);
    if (!isHttpSuccessStatus(statusCode))
        return 0;

    int count = 0;
    for (JsonObjectConst scene : doc["data"].as<JsonArrayConst>())
    {
        if (count >= maxScenes) break;
        const char* id   = scene["id"] | "";
        const char* name = scene["metadata"]["name"] | "";
        const char* gRid = scene["group"]["rid"] | "";
        if (id[0] == '\0') continue;

        String resolvedName = "-";
        for (int g = 0; g < groupCount; g++)
        {
            if (groupRids[g] == gRid)
            {
                resolvedName = groupNames[g];
                break;
            }
        }

        scenes[count].id        = String(id);
        scenes[count].name      = String(name);
        scenes[count].groupRid  = String(gRid);
        scenes[count].groupName = resolvedName;
        count++;
    }

    logInfo("HueGatewayClient", "Found scenes: %d", count);
    return count;
}

void HueGatewayClient::appendDeviceLightLinksFromDoc(std::vector<DeviceLightLink>& links, const JsonDocument& doc)
{
    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    if (data.isNull())
    {
        data = doc.as<JsonArrayConst>();
    }
    if (data.isNull())
    {
        return;
    }

    for (JsonObjectConst item : data)
    {
        const char* deviceRid = item["id"] | "";
        if (deviceRid[0] == '\0')
        {
            continue;
        }

        auto collectFromRefs = [&](JsonArrayConst refs) {
            for (JsonVariantConst refVar : refs)
            {
                String rid;
                String rtype;
                if (!extractResourceRef(refVar, rid, rtype))
                {
                    continue;
                }

                if (!(rtype.equalsIgnoreCase("light") || rtype.indexOf("light") >= 0))
                {
                    continue;
                }

                DeviceLightLink link;
                link.deviceId = String(deviceRid);
                link.lightId = rid;
                links.push_back(link);
            }
        };

        collectFromRefs(item["services"].as<JsonArrayConst>());
        collectFromRefs(item["children"].as<JsonArrayConst>());
        collectFromRefs(item["grouped_services"].as<JsonArrayConst>());
    }
}

void HueGatewayClient::appendLocationsFromDoc(std::vector<LightLocation>& locations,
                                              const JsonDocument& doc,
                                              bool isRoom,
                                              const std::vector<DeviceLightLink>& deviceLightLinks)
{
    auto mapDeviceToLights = [&](const String& deviceRid,
                                 const String& locationName,
                                 const String& locationRid,
                                 const String& locationIdV1,
                                 bool roomLocation) {
        bool resolved = false;
        for (const auto& link : deviceLightLinks)
        {
            if (!link.deviceId.equalsIgnoreCase(deviceRid))
            {
                continue;
            }

            resolved = true;
            if (roomLocation)
            {
                upsertLocation(locations, link.lightId, locationName, locationRid, locationIdV1, "", "", "");
            }
            else
            {
                upsertLocation(locations, link.lightId, "", "", "", locationName, locationRid, locationIdV1);
            }
        }

        if (!resolved)
        {
            if (roomLocation)
            {
                upsertLocation(locations, deviceRid, locationName, locationRid, locationIdV1, "", "", "");
            }
            else
            {
                upsertLocation(locations, deviceRid, "", "", "", locationName, locationRid, locationIdV1);
            }
        }
    };

    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    if (data.isNull())
    {
        data = doc.as<JsonArrayConst>();
    }
    if (data.isNull())
    {
        return;
    }

    for (JsonObjectConst item : data)
    {
        String locationName = item["metadata"]["name"].as<String>();
        if (locationName.length() == 0)
        {
            locationName = item["name"].as<String>();
        }
        if (locationName.length() == 0)
        {
            continue;
        }

        String locationRid = item["id"].as<String>();
        String locationIdV1 = item["id_v1"].as<String>();

        auto handleRefs = [&](JsonArrayConst refs) {
            for (JsonVariantConst refVar : refs)
            {
                String rid;
                String rawType;
                if (!extractResourceRef(refVar, rid, rawType))
                {
                    continue;
                }

                String rtype = rawType;
                rtype.toLowerCase();

                if (rtype == "light")
                {
                    if (isRoom)
                    {
                        upsertLocation(locations, rid, locationName, locationRid, locationIdV1, "", "", "");
                    }
                    else
                    {
                        upsertLocation(locations, rid, "", "", "", locationName, locationRid, locationIdV1);
                    }
                    continue;
                }

                if (rtype == "device")
                {
                    mapDeviceToLights(rid, locationName, locationRid, locationIdV1, isRoom);
                    continue;
                }

                bool matchedAsLight = false;
                for (const auto& link : deviceLightLinks)
                {
                    if (link.lightId.equalsIgnoreCase(rid))
                    {
                        if (isRoom)
                        {
                            upsertLocation(locations, link.lightId, locationName, locationRid, locationIdV1, "", "", "");
                        }
                        else
                        {
                            upsertLocation(locations, link.lightId, "", "", "", locationName, locationRid, locationIdV1);
                        }
                        matchedAsLight = true;
                        break;
                    }
                }

                if (!matchedAsLight)
                {
                    mapDeviceToLights(rid, locationName, locationRid, locationIdV1, isRoom);
                }
            }
        };

        handleRefs(item["children"].as<JsonArrayConst>());
        handleRefs(item["services"].as<JsonArrayConst>());
        handleRefs(item["grouped_services"].as<JsonArrayConst>());
    }

}

void HueGatewayClient::upsertLocation(std::vector<LightLocation>& locations,
                                      const String& id,
                                      const String& room,
                                      const String& roomRid,
                                      const String& roomIdV1,
                                      const String& zone,
                                      const String& zoneRid,
                                      const String& zoneIdV1)
{
    for (auto& loc : locations)
    {
        if (loc.id == id)
        {
            if (room.length() > 0)
            {
                loc.room = room;
            }
            if (roomRid.length() > 0)
            {
                loc.roomRid = roomRid;
            }
            if (roomIdV1.length() > 0)
            {
                loc.roomIdV1 = roomIdV1;
            }
            if (zone.length() > 0)
            {
                loc.zone = zone;
            }
            if (zoneRid.length() > 0)
            {
                loc.zoneRid = zoneRid;
            }
            if (zoneIdV1.length() > 0)
            {
                loc.zoneIdV1 = zoneIdV1;
            }
            return;
        }
    }

    LightLocation loc;
    loc.id = id;
    loc.room = room;
    loc.roomRid = roomRid;
    loc.roomIdV1 = roomIdV1;
    loc.zone = zone;
    loc.zoneRid = zoneRid;
    loc.zoneIdV1 = zoneIdV1;
    locations.push_back(loc);
}

bool HueGatewayClient::setLightOnOff(const String& lightId, bool on)
{
    if (!_initialized)
        return false;

    String endpoint = "/clip/v2/resource/light/" + lightId;

    DynamicJsonDocument doc(256);
    doc["on"]["on"] = on;

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "Light %s -> %s", lightId.c_str(), on ? "ON" : "OFF");
        return true;
    }

    logError("HueGatewayClient", "PUT light on/off failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::setGroupedLightOnOff(const String& groupedLightId, bool on)
{
    if (!_initialized)
        return false;

    String endpoint = "/clip/v2/resource/grouped_light/" + groupedLightId;

    DynamicJsonDocument doc(256);
    doc["on"]["on"] = on;

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "GroupedLight %s -> %s", groupedLightId.c_str(), on ? "ON" : "OFF");
        return true;
    }

    logError("HueGatewayClient", "PUT grouped_light on/off failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::setLightBrightness(const String& lightId, uint8_t brightness)
{
    if (!_initialized)
        return false;
    
    // Brightness: 0-254 -> 0.0-100.0%
    float brightnessPct = (brightness / 254.0f) * 100.0f;

    String endpoint = "/clip/v2/resource/light/" + lightId;

    DynamicJsonDocument doc(256);
    doc["dimming"]["brightness"] = brightnessPct;

    String payload;
    serializeJson(doc, payload);
    
    int statusCode = httpPut(endpoint, payload);
    
    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "Light %s -> Brightness: %d", lightId.c_str(), brightness);
        return true;
    }
    else
    {
        logError("HueGatewayClient", "PUT brightness failed - HTTP %d", statusCode);
        return false;
    }
}

bool HueGatewayClient::setGroupedLightBrightness(const String& groupedLightId, uint8_t brightness)
{
    if (!_initialized)
        return false;

    float brightnessPct = (brightness / 254.0f) * 100.0f;

    String endpoint = "/clip/v2/resource/grouped_light/" + groupedLightId;

    DynamicJsonDocument doc(256);
    doc["dimming"]["brightness"] = brightnessPct;

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "GroupedLight %s -> Brightness: %d", groupedLightId.c_str(), brightness);
        return true;
    }

    logError("HueGatewayClient", "PUT grouped_light brightness failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::setLightState(const String& lightId, bool on, uint8_t brightness, uint8_t fadeDurationSec)
{
    if (!_initialized)
        return false;
    
    float brightnessPct = (brightness / 254.0f) * 100.0f;
    
    String endpoint = "/clip/v2/resource/light/" + lightId;
    
    DynamicJsonDocument doc(512);
    doc["on"]["on"] = on;
    // Only send a dimming value when we actually have one (>0). Sending
    // brightness=0 would make the bridge clamp to its minimum (~1%) instead
    // of keeping its last-known brightness. Omitting the field = keep last value.
    if (on && brightness > 0)
    {
        doc["dimming"]["brightness"] = brightnessPct;
    }
    if (fadeDurationSec > 0)
    {
        doc["dynamics"]["duration"] = static_cast<uint32_t>(fadeDurationSec) * 1000UL;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    int statusCode = httpPut(endpoint, payload);
    
    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "Light %s -> On:%d Bri:%d fade:%us",
                 lightId.c_str(), on, brightness, static_cast<unsigned>(fadeDurationSec));
        return true;
    }
    else
    {
        logError("HueGatewayClient", "PUT state failed - HTTP %d", statusCode);
        return false;
    }
}

bool HueGatewayClient::setGroupedLightState(const String& groupedLightId, bool on, uint8_t brightness, uint8_t fadeDurationSec)
{
    if (!_initialized)
        return false;

    float brightnessPct = (brightness / 254.0f) * 100.0f;

    String endpoint = "/clip/v2/resource/grouped_light/" + groupedLightId;

    DynamicJsonDocument doc(512);
    doc["on"]["on"] = on;
    // See setLightState: omit dimming at brightness=0 to keep last bridge value.
    if (on && brightness > 0)
    {
        doc["dimming"]["brightness"] = brightnessPct;
    }
    if (fadeDurationSec > 0)
    {
        doc["dynamics"]["duration"] = static_cast<uint32_t>(fadeDurationSec) * 1000UL;
    }

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "GroupedLight %s -> On:%d Bri:%d fade:%us",
                 groupedLightId.c_str(), on, brightness, static_cast<unsigned>(fadeDurationSec));
        return true;
    }

    logError("HueGatewayClient", "PUT grouped_light state failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::setLightDimmingDelta(const String& lightId, bool brighter, uint8_t steps)
{
    if (!_initialized)
        return false;

    if (steps == 0)
    {
        return stopLightDimming(lightId);
    }

    if (steps > 7)
    {
        steps = 7;
    }

    float brightnessDeltaPct = relativeDimmingDeltaPercent(steps);

    String endpoint = "/clip/v2/resource/light/" + lightId;

    DynamicJsonDocument doc(256);
    doc["dimming_delta"]["action"] = brighter ? "up" : "down";
    doc["dimming_delta"]["brightness_delta"] = brightnessDeltaPct;

    // Ensure "dim up" from OFF can activate the light similar to KNX behavior.
    if (brighter)
    {
        doc["on"]["on"] = true;
    }

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "Light %s -> Dimming delta: %s %u step(s) (%.1f%%)",
                 lightId.c_str(), brighter ? "up" : "down", static_cast<unsigned>(steps), brightnessDeltaPct);
        return true;
    }

    logError("HueGatewayClient", "PUT dimming_delta failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::setGroupedLightDimmingDelta(const String& groupedLightId, bool brighter, uint8_t steps)
{
    if (!_initialized)
        return false;

    if (steps == 0)
    {
        return stopGroupedLightDimming(groupedLightId);
    }

    if (steps > 7)
    {
        steps = 7;
    }

    float brightnessDeltaPct = relativeDimmingDeltaPercent(steps);

    String endpoint = "/clip/v2/resource/grouped_light/" + groupedLightId;

    DynamicJsonDocument doc(256);
    doc["dimming_delta"]["action"] = brighter ? "up" : "down";
    doc["dimming_delta"]["brightness_delta"] = brightnessDeltaPct;

    if (brighter)
    {
        doc["on"]["on"] = true;
    }

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "GroupedLight %s -> Dimming delta: %s %u step(s) (%.1f%%)",
                 groupedLightId.c_str(), brighter ? "up" : "down", static_cast<unsigned>(steps), brightnessDeltaPct);
        return true;
    }

    logError("HueGatewayClient", "PUT grouped_light dimming delta failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::stopLightDimming(const String& lightId)
{
    if (!_initialized)
        return false;

    String endpoint = "/clip/v2/resource/light/" + lightId;

    DynamicJsonDocument doc(160);
    doc["dimming_delta"]["action"] = "stop";

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "Light %s -> Dimming stop", lightId.c_str());
        return true;
    }

    logError("HueGatewayClient", "PUT dimming stop failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::stopGroupedLightDimming(const String& groupedLightId)
{
    if (!_initialized)
        return false;

    String endpoint = "/clip/v2/resource/grouped_light/" + groupedLightId;

    DynamicJsonDocument doc(128);
    doc["dimming_delta"]["action"] = "stop";

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "GroupedLight %s -> Dimming STOP", groupedLightId.c_str());
        return true;
    }

    logError("HueGatewayClient", "PUT grouped_light dimming stop failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::extractGroupedLightRid(JsonObjectConst item, String& groupedLightRid)
{
    JsonArrayConst services = item["services"].as<JsonArrayConst>();
    for (JsonObjectConst service : services)
    {
        const char* rtype = service["rtype"] | "";
        if (strcmp(rtype, "grouped_light") != 0)
        {
            continue;
        }

        const char* rid = service["rid"] | "";
        if (rid[0] == '\0')
        {
            continue;
        }

        groupedLightRid = String(rid);
        return true;
    }

    return false;
}

bool HueGatewayClient::resolveGroupedLightForTarget(const String& endpoint, const String& targetRid, String& groupedLightRid, String& targetName)
{
    groupedLightRid = "";
    targetName = "";

    if (!_initialized || targetRid.length() == 0)
    {
        return false;
    }

    DynamicJsonDocument doc(32768);
    DynamicJsonDocument targetFilterDoc(512);
    targetFilterDoc.clear();
    JsonObject targetFilterRoot = targetFilterDoc.to<JsonObject>();
    JsonObject targetFilterData = targetFilterRoot["data"][0].to<JsonObject>();
    targetFilterData["id"] = true;
    targetFilterData["metadata"]["name"] = true;
    targetFilterData["services"][0]["rid"] = true;
    targetFilterData["services"][0]["rtype"] = true;
    targetFilterData["children"][0]["rid"] = true;
    targetFilterData["children"][0]["rtype"] = true;
    targetFilterData["grouped_services"][0]["rid"] = true;
    targetFilterData["grouped_services"][0]["rtype"] = true;
    doc.clear();

    int statusCode = httpGet(endpoint, doc, &targetFilterDoc);
    if (!isHttpSuccessStatus(statusCode))
    {
        logError("HueGatewayClient", "GET %s failed - HTTP %d", endpoint.c_str(), statusCode);
        return false;
    }

    String lookup = targetRid;
    lookup.trim();
    String lookupLower = lookup;
    lookupLower.toLowerCase();

    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    for (JsonObjectConst item : data)
    {
        String itemRid = item["id"].as<String>();
        String itemName = item["metadata"]["name"].as<String>();
        String itemNameLower = itemName;
        itemNameLower.toLowerCase();

        bool matchesId = itemRid.equalsIgnoreCase(lookup);
        bool matchesName = itemNameLower == lookupLower;
        if (!matchesId && !matchesName)
        {
            continue;
        }

        targetName = itemName;
        return extractGroupedLightRid(item, groupedLightRid);
    }

    return false;
}

bool HueGatewayClient::resolveGroupedLightForRoom(const String& roomRid, String& groupedLightRid, String& roomName)
{
    return resolveGroupedLightForTarget("/clip/v2/resource/room", roomRid, groupedLightRid, roomName);
}

bool HueGatewayClient::resolveGroupedLightForZone(const String& zoneRid, String& groupedLightRid, String& zoneName)
{
    return resolveGroupedLightForTarget("/clip/v2/resource/zone", zoneRid, groupedLightRid, zoneName);
}

int HueGatewayClient::getTargetsForEndpoint(const String& endpoint, HueGatewayTargetInfo* targets, int maxTargets)
{
    if (!_initialized || targets == nullptr || maxTargets <= 0)
    {
        return 0;
    }

    DynamicJsonDocument doc(32768);
    DynamicJsonDocument targetFilterDoc(512);
    targetFilterDoc.clear();
    JsonObject targetFilterRoot = targetFilterDoc.to<JsonObject>();
    JsonObject targetFilterData = targetFilterRoot["data"][0].to<JsonObject>();
    targetFilterData["id"] = true;
    targetFilterData["metadata"]["name"] = true;
    targetFilterData["services"][0]["rid"] = true;
    targetFilterData["services"][0]["rtype"] = true;
    targetFilterData["children"][0]["rid"] = true;
    targetFilterData["children"][0]["rtype"] = true;
    targetFilterData["grouped_services"][0]["rid"] = true;
    targetFilterData["grouped_services"][0]["rtype"] = true;
    doc.clear();

    int statusCode = httpGet(endpoint, doc, &targetFilterDoc);
    if (!isHttpSuccessStatus(statusCode))
    {
        logError("HueGatewayClient", "GET %s failed - HTTP %d", endpoint.c_str(), statusCode);
        return 0;
    }

    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    int count = 0;

    for (JsonObjectConst item : data)
    {
        if (count >= maxTargets)
        {
            break;
        }

        String groupedRid;
        if (!extractGroupedLightRid(item, groupedRid) || groupedRid.length() == 0)
        {
            continue;
        }

        targets[count].id = item["id"].as<String>();
        targets[count].name = item["metadata"]["name"].as<String>();
        targets[count].groupedLightId = groupedRid;
        count++;
    }

    return count;
}

int HueGatewayClient::getRoomTargets(HueGatewayTargetInfo* targets, int maxTargets)
{
    return getTargetsForEndpoint("/clip/v2/resource/room", targets, maxTargets);
}

int HueGatewayClient::getZoneTargets(HueGatewayTargetInfo* targets, int maxTargets)
{
    return getTargetsForEndpoint("/clip/v2/resource/zone", targets, maxTargets);
}

bool HueGatewayClient::setLightStateWithColorTemp(const String& lightId, bool on, uint8_t brightness, 
                                                   uint16_t mirek, uint8_t fadeDurationSec)
{
    if (!_initialized)
        return false;
    
    // Clamp mirek to valid range (153-500)
    // 153 = cold/6536K, 500 = warm/2000K
    uint16_t clampedMirek = mirek;
    if (clampedMirek < 153) clampedMirek = 153;
    if (clampedMirek > 500) clampedMirek = 500;
    
    // Brightness 0-254 -> 0-100%
    float brightnessPct = (brightness / 254.0f) * 100.0f;
    
    // Fade duration in milliseconds (Hue API v2 dynamics.duration).
    uint32_t fadeDurationMs = fadeDurationSec * 1000;
    
    String endpoint = "/clip/v2/resource/light/" + lightId;
    
    // API v2 structure with on, dimming, color_temperature, and dynamics
    DynamicJsonDocument doc(512);
    doc["on"]["on"] = on;
    if (on)
    {
        // Omit dimming at brightness=0 so the bridge keeps its last-known
        // brightness; still apply the requested color temperature.
        if (brightness > 0)
            doc["dimming"]["brightness"] = brightnessPct;
        doc["color_temperature"]["mirek"] = clampedMirek;
    }
    if (fadeDurationSec > 0) {
        doc["dynamics"]["duration"] = fadeDurationMs;
    }
    
    String payload;
    serializeJson(doc, payload);
    
    int statusCode = httpPut(endpoint, payload);
    
    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "Light %s -> On:%d Bri:%d CT:%d fade:%ds",
                 lightId.c_str(), on, brightness, clampedMirek, fadeDurationSec);
        return true;
    }
    else
    {
        logError("HueGatewayClient", "PUT state+CT failed - HTTP %d", statusCode);
        return false;
    }
}

bool HueGatewayClient::setGroupedLightStateWithColorTemp(const String& groupedLightId,
                                                          bool on,
                                                          uint8_t brightness,
                                                          uint16_t mirek,
                                                          uint8_t fadeDurationSec)
{
    if (!_initialized)
        return false;

    uint16_t clampedMirek = mirek;
    if (clampedMirek < 153) clampedMirek = 153;
    if (clampedMirek > 500) clampedMirek = 500;

    float brightnessPct = (brightness / 254.0f) * 100.0f;
    uint32_t fadeDurationMs = fadeDurationSec * 1000UL;

    String endpoint = "/clip/v2/resource/grouped_light/" + groupedLightId;

    DynamicJsonDocument doc(512);
    doc["on"]["on"] = on;
    if (on)
    {
        // Omit dimming at brightness=0 so the bridge keeps its last-known
        // brightness; still apply the requested color temperature.
        if (brightness > 0)
            doc["dimming"]["brightness"] = brightnessPct;
        doc["color_temperature"]["mirek"] = clampedMirek;
    }
    if (fadeDurationSec > 0)
    {
        doc["dynamics"]["duration"] = fadeDurationMs;
    }

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "GroupedLight %s -> On:%d Bri:%d CT:%d fade:%ds",
                 groupedLightId.c_str(), on, brightness, clampedMirek, fadeDurationSec);
        return true;
    }

    logError("HueGatewayClient", "PUT grouped_light state+CT failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::setLightColorTemperature(const String& lightId, uint16_t mirek)
{
    if (!_initialized)
        return false;
    
    // Clamp mirek to valid range (153-500)
    // 153 = cold/6536K, 500 = warm/2000K
    uint16_t clampedMirek = mirek;
    if (clampedMirek < 153) clampedMirek = 153;
    if (clampedMirek > 500) clampedMirek = 500;
    
    String endpoint = "/clip/v2/resource/light/" + lightId;
    
    // API v2 structure: {"color_temperature": {"mirek": value}}
    DynamicJsonDocument doc(256);
    doc["color_temperature"]["mirek"] = clampedMirek;
    
    String payload;
    serializeJson(doc, payload);
    
    int statusCode = httpPut(endpoint, payload);
    
    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "Light %s -> ColorTemp: %d mirek (%d K)",
                 lightId.c_str(), clampedMirek, mirekToKelvin(clampedMirek));
        return true;
    }
    else
    {
        logError("HueGatewayClient", "PUT color_temperature failed - HTTP %d", statusCode);
        return false;
    }
}

bool HueGatewayClient::setGroupedLightColorTemperature(const String& groupedLightId, uint16_t mirek)
{
    if (!_initialized)
        return false;

    uint16_t clampedMirek = mirek;
    if (clampedMirek < 153) clampedMirek = 153;
    if (clampedMirek > 500) clampedMirek = 500;

    String endpoint = "/clip/v2/resource/grouped_light/" + groupedLightId;

    DynamicJsonDocument doc(256);
    doc["color_temperature"]["mirek"] = clampedMirek;

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "GroupedLight %s -> ColorTemp: %d mirek (%d K)",
                 groupedLightId.c_str(), clampedMirek, mirekToKelvin(clampedMirek));
        return true;
    }

    logError("HueGatewayClient", "PUT grouped_light color_temperature failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::setLightColor(const String& lightId, float x, float y)
{
    if (!_initialized)
        return false;
    
    // Clamp x and y to valid range (0.0-1.0)
    // CIE 1931 color space coordinates
    float clampedX = x;
    float clampedY = y;
    if (clampedX < 0.0f) clampedX = 0.0f;
    if (clampedX > 1.0f) clampedX = 1.0f;
    if (clampedY < 0.0f) clampedY = 0.0f;
    if (clampedY > 1.0f) clampedY = 1.0f;
    
    String endpoint = "/clip/v2/resource/light/" + lightId;
    
    // API v2 structure: {"color": {"xy": {"x": x, "y": y}}}
    DynamicJsonDocument doc(256);
    doc["color"]["xy"]["x"] = clampedX;
    doc["color"]["xy"]["y"] = clampedY;
    
    String payload;
    serializeJson(doc, payload);
    
    int statusCode = httpPut(endpoint, payload);
    
    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "Light %s -> Color XY: (%.3f, %.3f)",
                 lightId.c_str(), clampedX, clampedY);
        return true;
    }
    else
    {
        logError("HueGatewayClient", "PUT color failed - HTTP %d", statusCode);
        return false;
    }
}

bool HueGatewayClient::setGroupedLightColor(const String& groupedLightId, float x, float y)
{
    if (!_initialized)
        return false;

    float clampedX = x;
    float clampedY = y;
    if (clampedX < 0.0f) clampedX = 0.0f;
    if (clampedX > 1.0f) clampedX = 1.0f;
    if (clampedY < 0.0f) clampedY = 0.0f;
    if (clampedY > 1.0f) clampedY = 1.0f;

    String endpoint = "/clip/v2/resource/grouped_light/" + groupedLightId;

    DynamicJsonDocument doc(256);
    doc["color"]["xy"]["x"] = clampedX;
    doc["color"]["xy"]["y"] = clampedY;

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logDebug("HueGatewayClient", "GroupedLight %s -> Color XY: (%.3f, %.3f)",
                 groupedLightId.c_str(), clampedX, clampedY);
        return true;
    }

    logError("HueGatewayClient", "PUT grouped_light color failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::recallHueScene(const String& sceneRID)
{
    if (!_initialized || sceneRID.length() == 0)
        return false;

    String endpoint = "/clip/v2/resource/scene/" + sceneRID;

    DynamicJsonDocument doc(128);
    doc["recall"]["action"] = "active";

    String payload;
    serializeJson(doc, payload);

    int statusCode = httpPut(endpoint, payload);

    if (isHttpSuccessStatus(statusCode))
    {
        logInfo("HueGatewayClient", "Scene %s recalled", sceneRID.c_str());
        return true;
    }

    logError("HueGatewayClient", "Scene recall failed - HTTP %d", statusCode);
    return false;
}

int HueGatewayClient::getDeviceServiceRids(const String& deviceId, HueGatewayServiceRid* out, int maxCount)
{
    if (!_initialized || deviceId.length() == 0 || out == nullptr || maxCount <= 0)
        return 0;

    String endpoint = "/clip/v2/resource/device/" + deviceId;
    DynamicJsonDocument doc(2048);
    int statusCode = httpGet(endpoint, doc);
    if (!isHttpSuccessStatus(statusCode))
    {
        logError("HueGatewayClient", "getDeviceServiceRids: HTTP %d for device %s",
                 statusCode, deviceId.c_str());
        return 0;
    }

    int count = 0;
    JsonArrayConst dataArr = doc["data"].as<JsonArrayConst>();
    for (JsonObjectConst device : dataArr)
    {
        JsonArrayConst services = device["services"].as<JsonArrayConst>();
        for (JsonObjectConst svc : services)
        {
            if (count >= maxCount)
                break;
            const char* rtype = svc["rtype"] | "";
            const char* rid   = svc["rid"]   | "";
            if (rtype[0] != '\0' && rid[0] != '\0')
            {
                out[count].rtype = String(rtype);
                out[count].rid   = String(rid);
                count++;
            }
        }
        if (count >= maxCount)
            break;
    }

    logDebug("HueGatewayClient", "getDeviceServiceRids: device %s -> %d services",
             deviceId.c_str(), count);
    return count;
}

int HueGatewayClient::getBehaviorInstances(const String& deviceId, String* instanceIds, int maxCount, String* debugInfo)
{
    if (!_initialized || deviceId.length() == 0 || instanceIds == nullptr || maxCount <= 0)
        return 0;

    // First resolve all service RIDs for the device so we can match via owner/dependees
    static constexpr int kMaxSvc = 16;
    HueGatewayServiceRid deviceSvcs[kMaxSvc];
    const int svcCount = getDeviceServiceRids(deviceId, deviceSvcs, kMaxSvc);

    // Use a filter document to only parse the fields we need for matching.
    // The full behavior_instance response can be very large (>32KB) due to
    // complex configuration data, so we MUST filter to avoid parse failures.
    StaticJsonDocument<512> filterDoc;
    JsonObject filterData = filterDoc["data"][0].to<JsonObject>();
    filterData["id"] = true;
    filterData["enabled"] = true;
    filterData["script_id"] = true;
    filterData["dependees"][0]["target"]["rid"] = true;
    filterData["dependees"][0]["target"]["rtype"] = true;
    filterData["owner"]["rid"] = true;
    filterData["owner"]["rtype"] = true;

    DynamicJsonDocument doc(16384);
    // Use 3s timeout — behavior_instance response is filtered, bridge should respond quickly
    int statusCode = httpGet("/clip/v2/resource/behavior_instance", doc, &filterDoc, 3000, 50);
    if (!isHttpSuccessStatus(statusCode))
    {
        logError("HueGatewayClient", "getBehaviorInstances: HTTP %d (filtered)", statusCode);
        if (debugInfo)
            *debugInfo = "httpFail=" + String(statusCode)
                + " jsonErr=" + _diagStats.lastJsonError
                + " contentLen=" + String(_diagStats.lastContentLength)
                + " svc=" + String(svcCount);
        return 0;
    }

    logDebug("HueGatewayClient", "getBehaviorInstances: HTTP %d, docOverflow=%d, memUsed=%u/%u",
             statusCode, doc.overflowed() ? 1 : 0,
             (unsigned)doc.memoryUsage(), 16384u);

    int count = 0;
    int totalInstances = 0;
    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    for (JsonObjectConst inst : data)
    {
        totalInstances++;
        if (count >= maxCount)
            break;

        bool matchesDevice = false;

        // Method 1: dependees[].target.rtype=="device" && target.rid==deviceId
        JsonArrayConst dependees = inst["dependees"].as<JsonArrayConst>();
        for (JsonObjectConst dep : dependees)
        {
            const char* rtype = dep["target"]["rtype"] | "";
            const char* rid   = dep["target"]["rid"]   | "";
            if (strcmp(rtype, "device") == 0 && strcmp(rid, deviceId.c_str()) == 0)
            {
                matchesDevice = true;
                break;
            }
            // Also check if dependees reference a service RID of the device
            for (int s = 0; s < svcCount && !matchesDevice; s++)
            {
                if (deviceSvcs[s].rid.equalsIgnoreCase(String(rid)))
                {
                    matchesDevice = true;
                    break;
                }
            }
            if (matchesDevice) break;
        }

        // Method 2: owner.rid == deviceId or owner.rid matches a service of the device
        if (!matchesDevice)
        {
            const char* ownerRid = inst["owner"]["rid"] | "";
            if (ownerRid[0] != '\0')
            {
                if (strcmp(ownerRid, deviceId.c_str()) == 0)
                {
                    matchesDevice = true;
                }
                else
                {
                    for (int s = 0; s < svcCount; s++)
                    {
                        if (deviceSvcs[s].rid.equalsIgnoreCase(String(ownerRid)))
                        {
                            matchesDevice = true;
                            break;
                        }
                    }
                }
            }
        }

        if (!matchesDevice)
            continue;

        const char* id = inst["id"] | "";
        if (id[0] != '\0')
        {
            instanceIds[count++] = String(id);
            logDebug("HueGatewayClient", "behavior_instance match: %s for device %s",
                     id, deviceId.c_str());
        }
    }

    // --- Debug: log summary ---
    logDebug("HueGatewayClient", "getBehaviorInstances: device %s -> %d/%d instances (svc=%d)",
             deviceId.c_str(), count, totalInstances, svcCount);

    // Build debugInfo string for caller to put into RingLog
    if (debugInfo != nullptr)
    {
        *debugInfo = "total=" + String(totalInstances) + " matched=" + String(count)
            + " svc=" + String(svcCount) + " overflow=" + String(doc.overflowed() ? 1 : 0)
            + " mem=" + String((unsigned)doc.memoryUsage());
        // Append matched instances
        for (int i = 0; i < count && i < 3; i++)
        {
            *debugInfo += " |" + instanceIds[i].substring(0, 8);
        }
    }
    return count;
}

bool HueGatewayClient::deleteBehaviorInstance(const String& instanceId)
{
    if (!_initialized || instanceId.length() == 0)
        return false;

    String endpoint = "/clip/v2/resource/behavior_instance/" + instanceId;
    int statusCode = httpDelete(endpoint);

    if (isHttpSuccessStatus(statusCode))
    {
        logInfo("HueGatewayClient", "behavior_instance %s deleted", instanceId.c_str());
        return true;
    }

    logError("HueGatewayClient", "behavior_instance delete failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::pingBridgeApiV2()
{
    if (!_initialized)
    {
        return false;
    }

    DynamicJsonDocument doc(1024);
    int statusCode = httpGet("/clip/v2/resource/bridge", doc);
    if (isHttpSuccessStatus(statusCode))
    {
        return true;
    }

    logWarning("HueGatewayClient", "Bridge API v2 ping failed - HTTP %d", statusCode);
    return false;
}

bool HueGatewayClient::startEventStream()
{
    if (!_initialized)
    {
        return false;
    }

    if (_eventStreamConnected && _eventClient.connected())
    {
        return true;
    }

    if (_eventHandshakePending && _eventClient.connected())
    {
        return true;
    }

    if (_eventConnectPending)
    {
        return true;
    }

    if (!hasTlsInternalHeadroom())
    {
        logWarning("HueGatewayClient", "EventStream deferred: insufficient internal TLS headroom (need free>=%lu, largest>=%lu)",
                   static_cast<unsigned long>(kTlsMinInternalFreeBytes),
                   static_cast<unsigned long>(kTlsMinInternalLargestBlockBytes));
        logHeapStats("event-connect-deferred");
        return false;
    }

    stopEventStream();

    _eventClient.setTimeout(100);
    logInfo("HueGatewayClient", "EventStream async connecting to %s:443", _bridgeIP.c_str());
    logHeapStats("before-event-connect");

    _eventConnectPending = true;
    _eventConnectDone    = false;
    _eventConnectOk      = false;
    _eventConnectAbort   = false;

    BaseType_t result = xTaskCreatePinnedToCore(
        sEventStreamConnectTask,
        "hue-esc",
        kEventConnectStackSize,
        this,
        1,
        &_eventConnectTask,
        0
    );

    if (result != pdPASS)
    {
        _eventConnectPending = false;
        _eventConnectTask    = nullptr;
        _diagStats.eventConnectFail++;
        logError("HueGatewayClient", "EventStream connect task creation failed");
        logHeapStats("event-connect-task-failed");
        return false;
    }

    return true;
}

void HueGatewayClient::stopEventStream()
{
    if (_eventConnectPending)
    {
        _eventConnectAbort = true;
        const unsigned long waitStart = millis();
        while (_eventConnectPending && (millis() - waitStart < 2000UL))
        {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        if (_eventConnectPending)
        {
            logWarning("HueGatewayClient", "EventStream connect task abort timed out");
            _eventConnectPending = false;
        }
    }
    else
    {
        if (_eventClient.connected())
        {
            _diagStats.eventStopCount++;
            logInfo("HueGatewayClient", "EventStream stopping");
            _eventClient.stop();
        }
    }

    _eventConnectDone      = false;
    _eventStreamConnected  = false;
    _eventHandshakePending = false;
    _eventHandshakeStartMs = 0;
    _eventParseErrorStreak = 0;
    _eventLineBuffer       = "";
    _eventDataBuffer       = "";
}

// Async EventStream connect — FreeRTOS task (runs on Core 0)

void HueGatewayClient::sEventStreamConnectTask(void* param)
{
    HueGatewayClient* self = static_cast<HueGatewayClient*>(param);

    bool ok = self->_eventClient.connect(self->_bridgeIP.c_str(), 443);

    if (self->_eventConnectAbort)
    {
        if (ok)
        {
            self->_eventClient.stop();
        }
        ok = false;
    }

    self->_eventConnectOk      = ok;
    self->_eventConnectDone    = true;
    self->_eventConnectPending = false;
    self->_eventConnectTask    = nullptr;
    vTaskDelete(nullptr);
}

void HueGatewayClient::checkEventStreamConnect()
{
    if (!_eventConnectDone)
    {
        return;
    }

    _eventConnectDone = false;

    if (_eventConnectAbort)
    {
        return;
    }

    if (!_eventConnectOk)
    {
        _diagStats.eventConnectFail++;
        logError("HueGatewayClient", "Async EventStream connect failed");
        logHeapStats("event-connect-failed");
        return;
    }

    logInfo("HueGatewayClient", "EventStream handshake complete");

    _eventClient.printf("GET /eventstream/clip/v2 HTTP/1.1\r\n");
    _eventClient.printf("Host: %s\r\n", _bridgeIP.c_str());
    _eventClient.printf("hue-application-key: %s\r\n", _appKey.c_str());
    _eventClient.print("Accept: text/event-stream\r\n");
    _eventClient.print("Connection: keep-alive\r\n\r\n");

    _eventHandshakePending = true;
    _eventHandshakeStartMs = millis();
    _eventLastDataMs       = _eventHandshakeStartMs;
    _eventParseErrorStreak = 0;
    _eventStreamConnected  = false;
    _eventLineBuffer       = "";
    _eventDataBuffer       = "";
}

int HueGatewayClient::pollEventStream(HueGatewayEventLightUpdate* updates, int maxUpdates)
{
    if (_eventHandshakePending)
    {
        if (!_eventClient.connected())
        {
            logError("HueGatewayClient", "EventStream handshake failed: disconnected");
            stopEventStream();
            return 0;
        }

        if ((millis() - _eventHandshakeStartMs) > 3000UL && !_eventClient.available())
        {
            _diagStats.eventHandshakeTimeoutCount++;
            logError("HueGatewayClient", "EventStream header timeout");
            stopEventStream();
            return 0;
        }

        if (!_eventClient.available())
        {
            return 0;
        }

        String statusLine = _eventClient.readStringUntil('\n');
        statusLine.trim();
        logDebug("HueGatewayClient", "EventStream status: %s", statusLine.c_str());
        int statusCode = parseHttpStatusCode(statusLine);
        if (!isHttpSuccessStatus(statusCode))
        {
            _diagStats.eventHttpErrorCount++;
            logError("HueGatewayClient", "EventStream HTTP error: %s", statusLine.c_str());
            stopEventStream();
            return 0;
        }

        while (_eventClient.connected())
        {
            if (!_eventClient.available())
            {
                return 0;
            }

            String headerLine = _eventClient.readStringUntil('\n');
            headerLine.trim();
            if (headerLine.length() == 0)
            {
                break;
            }
        }

        _eventHandshakePending = false;
        _eventHandshakeStartMs = 0;
        _eventLineBuffer = "";
        _eventDataBuffer = "";
        _eventStreamConnected = true;
        _diagStats.eventConnectOk++;
        logInfo("HueGatewayClient", "EventStream connected");
    }

    if (!_eventStreamConnected || !_eventClient.connected() || updates == nullptr || maxUpdates <= 0)
    {
        return 0;
    }

    int updateCount = 0;
    while (_eventClient.available() && updateCount < maxUpdates)
    {
        _eventLastDataMs = millis();
        char ch = static_cast<char>(_eventClient.read());
        if (ch == '\r')
        {
            continue;
        }

        if (ch != '\n')
        {
            _eventLineBuffer += ch;
            if (_eventLineBuffer.length() > kMaxEventLineChars)
            {
                logWarning("HueGatewayClient", "EventStream line exceeded limit, dropping line");
                _eventDropCount++;
                _eventParseErrorStreak = min<uint8_t>(static_cast<uint8_t>(_eventParseErrorStreak + 1), static_cast<uint8_t>(10));
                _eventLineBuffer = "";
                if (_eventParseErrorStreak >= 3)
                {
                    logError("HueGatewayClient", "EventStream parser unstable, forcing reconnect");
                    stopEventStream();
                    return updateCount;
                }
            }
            continue;
        }

        if (_eventLineBuffer.length() == 0)
        {
            if (_eventDataBuffer.length() > 0)
            {
                int parsedCount = parseEventPayload(_eventDataBuffer, updates + updateCount, maxUpdates - updateCount);
                if (parsedCount < 0)
                {
                    _eventDropCount++;
                    _eventParseErrorStreak = min<uint8_t>(static_cast<uint8_t>(_eventParseErrorStreak + 1), static_cast<uint8_t>(10));
                    if (_eventParseErrorStreak >= 3)
                    {
                        logError("HueGatewayClient", "EventStream parse failures repeated, forcing reconnect");
                        stopEventStream();
                        return updateCount;
                    }
                }
                else
                {
                    updateCount += parsedCount;
                    _eventParseErrorStreak = 0;
                }
                _eventDataBuffer = "";
            }
        }
        else if (_eventLineBuffer.startsWith("data:"))
        {
            String dataPart = _eventLineBuffer.substring(5);
            dataPart.trim();
            _eventDataBuffer += dataPart;
            if (_eventDataBuffer.length() > kMaxEventPayloadChars)
            {
                logWarning("HueGatewayClient", "Event payload exceeded limit, dropping payload");
                _eventDropCount++;
                _eventParseErrorStreak = min<uint8_t>(static_cast<uint8_t>(_eventParseErrorStreak + 1), static_cast<uint8_t>(10));
                _eventDataBuffer = "";
                if (_eventParseErrorStreak >= 3)
                {
                    logError("HueGatewayClient", "EventStream payload drops repeated, forcing reconnect");
                    stopEventStream();
                    return updateCount;
                }
            }
        }

        _eventLineBuffer = "";
    }

    if (!_eventClient.connected())
    {
        _diagStats.eventDisconnectCount++;
        logInfo("HueGatewayClient", "EventStream disconnected");
        stopEventStream();
    }
    else if (_eventStreamConnected && _eventLastDataMs != 0 && (millis() - _eventLastDataMs) > 180000UL)
    {
        logWarning("HueGatewayClient", "EventStream stale for >180s, reconnecting");
        stopEventStream();
    }

    if (updateCount > 0)
    {
        logDebug("HueGatewayClient", "EventStream parsed %d update(s)", updateCount);
    }

    return updateCount;
}

int HueGatewayClient::parseEventPayload(const String& payload, HueGatewayEventLightUpdate* updates, int maxUpdates)
{
    if (payload.length() == 0 || updates == nullptr || maxUpdates <= 0)
    {
        return 0;
    }

    if (payload.length() > kMaxEventPayloadChars)
    {
        logError("HueGatewayClient", "Event payload too large: %u bytes", static_cast<unsigned>(payload.length()));
        return -1;
    }

    static DynamicJsonDocument doc(12288);
    doc.clear();
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        logError("HueGatewayClient", "Event payload JSON parse error: %s", error.c_str());
        return -1;
    }

    JsonArrayConst events = doc.as<JsonArrayConst>();
    int updateCount = 0;

    for (JsonObjectConst eventObj : events)
    {
        JsonArrayConst data = eventObj["data"].as<JsonArrayConst>();
        for (JsonObjectConst item : data)
        {
            const char* type = item["type"] | "";
            if (strcmp(type, "light") != 0 && strcmp(type, "grouped_light") != 0)
            {
                continue;
            }
            const bool isGroupedResource = (strcmp(type, "grouped_light") == 0);

            const char* id = item["id"] | "";
            if (id[0] == '\0' || updateCount >= maxUpdates)
            {
                continue;
            }

            HueGatewayEventLightUpdate& update = updates[updateCount];
            update.lightId = String(id);
            update.isGroupedResource = isGroupedResource;
            update.hasOn = false;
            update.on = false;
            update.hasBrightness = false;
            update.brightness = 0;
            update.hasColorTemp = false;
            update.colorTempKelvin = 0;
            update.hasColorRgb = false;
            update.red = 255;
            update.green = 255;
            update.blue = 255;

            JsonVariantConst onVar = item["on"]["on"];
            if (!onVar.isNull())
            {
                update.hasOn = true;
                update.on = onVar.as<bool>();
            }

            JsonVariantConst brightnessVar = item["dimming"]["brightness"];
            if (!brightnessVar.isNull())
            {
                float brightnessPct = brightnessVar.as<float>();
                if (brightnessPct < 0.0f) brightnessPct = 0.0f;
                if (brightnessPct > 100.0f) brightnessPct = 100.0f;
                update.hasBrightness = true;
                update.brightness = static_cast<uint8_t>(brightnessPct * 2.54f);
            }

            JsonVariantConst mirekVar = item["color_temperature"]["mirek"];
            if (!mirekVar.isNull())
            {
                update.hasColorTemp = true;
                update.colorTempKelvin = mirekToKelvin(mirekVar.as<uint16_t>());
            }

            JsonVariantConst xVar = item["color"]["xy"]["x"];
            JsonVariantConst yVar = item["color"]["xy"]["y"];
            if (!xVar.isNull() && !yVar.isNull())
            {
                update.hasColorRgb = true;
                xyToRgb(xVar.as<float>(), yVar.as<float>(), update.red, update.green, update.blue);
            }

            if (update.hasOn || update.hasBrightness || update.hasColorTemp || update.hasColorRgb)
            {
                updateCount++;
            }
        }
    }

    return updateCount;
}

uint16_t HueGatewayClient::kelvinToMirek(uint16_t kelvin)
{
    // Mirek = 1,000,000 / Kelvin
    // Clamp Kelvin to valid range first (2000-6536)
    if (kelvin < 2000) kelvin = 2000;
    if (kelvin > 6536) kelvin = 6536;
    
    uint16_t mirek = 1000000 / kelvin;
    
    // Ensure result is in valid mirek range (153-500)
    if (mirek < 153) mirek = 153;
    if (mirek > 500) mirek = 500;
    
    return mirek;
}

uint16_t HueGatewayClient::mirekToKelvin(uint16_t mirek)
{
    // Kelvin = 1,000,000 / Mirek
    // Clamp mirek to valid range first (153-500)
    if (mirek < 153) mirek = 153;
    if (mirek > 500) mirek = 500;
    
    uint16_t kelvin = 1000000 / mirek;
    
    return kelvin;
}

float HueGatewayClient::relativeDimmingDeltaPercent(uint8_t steps)
{
    if (steps == 0)
    {
        return 0.0f;
    }

    if (steps > 7)
    {
        steps = 7;
    }

    float deltaPercent = 0.0f;
    switch (steps)
    {
        case 1: deltaPercent = 6.0f; break;
        case 2: deltaPercent = 5.2f; break;
        case 3: deltaPercent = 4.6f; break;
        case 4: deltaPercent = 4.0f; break;
        case 5: deltaPercent = 3.4f; break;
        case 6: deltaPercent = 2.8f; break;
        case 7: deltaPercent = 2.2f; break;
        default: deltaPercent = 0.0f; break;
    }

    if (deltaPercent < 1.0f)
    {
        deltaPercent = 1.0f;
    }
    if (deltaPercent > 6.0f)
    {
        deltaPercent = 6.0f;
    }

    return deltaPercent;
}

// ===== Private Methods =====

int HueGatewayClient::httpGet(const String& endpoint, JsonDocument& doc, JsonDocument* filterDoc, int timeoutMs, int nestingLimit)
{
    String url = buildUrl(endpoint);
    logDebug("HueGatewayClient", "HTTP GET %s", url.c_str());
    _diagStats.httpGetCount++;
    _diagStats.lastHttpMethod = "GET";
    _diagStats.lastHttpEndpoint = endpoint;
    _diagStats.lastJsonError = "";
    _diagStats.lastContentLength = -1;
    _http.setTimeout(timeoutMs);
    const bool eventWasActive = (_eventHandshakePending || _eventStreamConnected || _eventConnectPending);
    bool eventPausedForGet = false;

    if (eventWasActive && _eventAutoRestartEnabled && !hasTlsInternalHeadroom())
    {
        logWarning("HueGatewayClient", "Pausing EventStream for HTTPS GET (internal TLS headroom low)");
        _http.end();
        stopEventStream();
        _secureClient.stop();
        delay(25);
        eventPausedForGet = true;
    }
    
    _http.useHTTP10(true);
    _http.begin(_secureClient, url);
    _http.addHeader("hue-application-key", _appKey);
    
    int statusCode = _http.GET();
    _diagStats.lastHttpStatusCode = statusCode;

    if (statusCode < 0)
    {
        _diagStats.httpGetErrorCount++;
        if (statusCode == HTTPC_ERROR_READ_TIMEOUT)
        {
            _diagStats.httpTimeoutCount++;
        }
        logHeapStats("http-get-failed");
    }

    if (isHttpSuccessStatus(statusCode))
    {
        doc.clear();
        Stream& responseStream = _http.getStream();
        const int contentLength = _http.getSize();
        _diagStats.lastContentLength = contentLength;

        DeserializationError error;
        if (contentLength >= 0)
        {
            if (filterDoc != nullptr)
            {
                error = deserializeJson(doc,
                                        responseStream,
                                        DeserializationOption::Filter(filterDoc->as<JsonVariantConst>()),
                                        DeserializationOption::NestingLimit(nestingLimit));
            }
            else
            {
                error = deserializeJson(doc, responseStream,
                                        DeserializationOption::NestingLimit(nestingLimit));
            }
        }
        else
        {
            String response = _http.getString();
            if (filterDoc != nullptr)
            {
                error = deserializeJson(doc,
                                        response,
                                        DeserializationOption::Filter(filterDoc->as<JsonVariantConst>()),
                                        DeserializationOption::NestingLimit(nestingLimit));
            }
            else
            {
                error = deserializeJson(doc, response,
                                        DeserializationOption::NestingLimit(nestingLimit));
            }
        }

        if (error)
        {
            _diagStats.lastJsonError = error.c_str();
            logError("HueGatewayClient", "JSON parse error: %s contentLen=%d docMem=%u",
                     error.c_str(), contentLength, (unsigned)doc.memoryUsage());
            statusCode = -1;
        }
    }
    else
    {
        logError("HueGatewayClient", "HTTP GET failed (%d)", statusCode);
        if (statusCode >= 0)
        {
            _diagStats.httpGetErrorCount++;
        }
    }
    
    _http.end();

    // EventStream restart is handled by the module loop via startEventStream()/checkEventStreamConnect().
    (void)eventPausedForGet;

    return statusCode;
}

int HueGatewayClient::httpPut(const String& endpoint, const String& payload)
{
    String url = buildUrl(endpoint);
    logDebug("HueGatewayClient", "HTTP PUT %s payload=%s", url.c_str(), payload.c_str());
    _diagStats.httpPutCount++;
    _diagStats.lastHttpMethod = "PUT";
    _diagStats.lastHttpEndpoint = endpoint;
    _http.setTimeout(2000);
    const bool eventWasActive = (_eventHandshakePending || _eventStreamConnected || _eventConnectPending);
    bool eventPausedForPut = false;

    if (eventWasActive && _eventAutoRestartEnabled && !hasTlsInternalHeadroom())
    {
        logWarning("HueGatewayClient", "Pausing EventStream for HTTPS PUT (internal TLS headroom low)");
        _http.end();
        stopEventStream();
        _secureClient.stop();
        delay(25);
        eventPausedForPut = true;
    }
    
    _http.begin(_secureClient, url);
    _http.addHeader("Content-Type", "application/json");
    _http.addHeader("hue-application-key", _appKey);
    
    int statusCode = _http.PUT(payload);
    _diagStats.lastHttpStatusCode = statusCode;

    if (statusCode < 0)
    {
        _diagStats.httpPutErrorCount++;
        if (statusCode == HTTPC_ERROR_READ_TIMEOUT)
        {
            _diagStats.httpTimeoutCount++;
        }
        logHeapStats("http-put-failed");
    }

    if (!isHttpSuccessStatus(statusCode))
    {
        if (statusCode >= 0)
        {
            _diagStats.httpPutErrorCount++;
        }
        String response;
        int bodyLen = _http.getSize();
        if (bodyLen >= 0 && bodyLen <= 512)
        {
            response = _http.getString();
        }
        else
        {
            Stream& s = _http.getStream();
            char buf[513];
            int n = s.readBytes(buf, 512);
            buf[n] = '\0';
            response = buf;
            if (bodyLen > 512) response += "...";
        }
        logError("HueGatewayClient", "HTTP PUT failed (%d): %s", statusCode, response.c_str());
    }
    
    _http.end();

    // EventStream restart is handled by the module loop via startEventStream()/checkEventStreamConnect().
    (void)eventPausedForPut;

    return statusCode;
}

int HueGatewayClient::httpDelete(const String& endpoint)
{
    String url = buildUrl(endpoint);
    logDebug("HueGatewayClient", "HTTP DELETE %s", url.c_str());
    _diagStats.lastHttpMethod = "DELETE";
    _diagStats.lastHttpEndpoint = endpoint;
    _http.setTimeout(2000);
    const bool eventWasActive = (_eventHandshakePending || _eventStreamConnected || _eventConnectPending);
    bool eventPausedForDelete = false;

    if (eventWasActive && _eventAutoRestartEnabled && !hasTlsInternalHeadroom())
    {
        logWarning("HueGatewayClient", "Pausing EventStream for HTTPS DELETE (internal TLS headroom low)");
        _http.end();
        stopEventStream();
        _secureClient.stop();
        delay(25);
        eventPausedForDelete = true;
    }

    _http.begin(_secureClient, url);
    _http.addHeader("hue-application-key", _appKey);

    int statusCode = _http.sendRequest("DELETE");
    _diagStats.lastHttpStatusCode = statusCode;

    if (statusCode < 0)
    {
        if (statusCode == HTTPC_ERROR_READ_TIMEOUT)
        {
            _diagStats.httpTimeoutCount++;
        }
        logHeapStats("http-delete-failed");
    }

    if (!isHttpSuccessStatus(statusCode))
    {
        String response = _http.getString();
        logError("HueGatewayClient", "HTTP DELETE failed (%d): %s", statusCode, response.c_str());
    }

    _http.end();

    // EventStream restart is handled by the module loop via startEventStream()/checkEventStreamConnect().
    (void)eventPausedForDelete;

    return statusCode;
}

String HueGatewayClient::buildUrl(const String& endpoint)
{
    // Hue API access should use HTTPS (HTTP deprecated by Signify).
    return "https://" + _bridgeIP + endpoint;
}

void HueGatewayClient::xyToRgb(float x, float y, uint8_t& red, uint8_t& green, uint8_t& blue)
{
    if (y <= 0.00001f)
    {
        red = 255;
        green = 255;
        blue = 255;
        return;
    }

    float z = 1.0f - x - y;
    float Y = 1.0f;
    float X = (Y / y) * x;
    float Z = (Y / y) * z;

    float r = X * 1.656492f - Y * 0.354851f - Z * 0.255038f;
    float g = -X * 0.707196f + Y * 1.655397f + Z * 0.036152f;
    float b = X * 0.051713f - Y * 0.121364f + Z * 1.011530f;

    if (r < 0.0f) r = 0.0f;
    if (g < 0.0f) g = 0.0f;
    if (b < 0.0f) b = 0.0f;

    float maxValue = r;
    if (g > maxValue) maxValue = g;
    if (b > maxValue) maxValue = b;
    if (maxValue > 1.0f)
    {
        r /= maxValue;
        g /= maxValue;
        b /= maxValue;
    }

    auto gamma = [](float c) -> float {
        return (c <= 0.0031308f) ? (12.92f * c) : ((1.0f + 0.055f) * powf(c, 1.0f / 2.4f) - 0.055f);
    };

    r = gamma(r);
    g = gamma(g);
    b = gamma(b);

    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    if (g < 0.0f) g = 0.0f;
    if (g > 1.0f) g = 1.0f;
    if (b < 0.0f) b = 0.0f;
    if (b > 1.0f) b = 1.0f;

    red = static_cast<uint8_t>(r * 255.0f);
    green = static_cast<uint8_t>(g * 255.0f);
    blue = static_cast<uint8_t>(b * 255.0f);
}

// ===== Sensoren: Bewegungsmelder, Kontakt, Taster =====

bool HueGatewayClient::getMotionState(const String& motionRid, HueGatewayMotionState& state)
{
    if (!_initialized)
        return false;

    DynamicJsonDocument doc(1024);
    const String endpoint = "/clip/v2/resource/motion/" + motionRid;
    const int statusCode = httpGet(endpoint, doc);
    if (!isHttpSuccessStatus(statusCode))
    {
        logError("HueGatewayClient", "getMotionState failed HTTP %d", statusCode);
        return false;
    }

    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    if (data.size() == 0)
        return false;

    JsonObjectConst item = data[0].as<JsonObjectConst>();
    state.id = motionRid;
    state.motionDetected = item["motion"]["motion"] | false;

    // Besitzer-RID für optionale Zusatzdaten ermitteln
    state.reachable = true;
    String ownerRid;
    if (item["owner"]["rtype"].as<String>() == "device")
        ownerRid = item["owner"]["rid"].as<String>();

    if (ownerRid.length() > 0)
    {
        fetchZigbeeReachableByOwner(ownerRid, state.reachable);
        fetchTemperatureByOwner(ownerRid, state.temperature);
        fetchLightLevelByOwner(ownerRid, state.lightLevelLux);
        fetchBatteryByOwner(ownerRid, state.batteryPercent);
    }
    return true;
}

bool HueGatewayClient::getContactState(const String& contactRid, HueGatewayContactState& state)
{
    if (!_initialized)
        return false;

    DynamicJsonDocument doc(1024);
    const String endpoint = "/clip/v2/resource/contact_sensor/" + contactRid;
    const int statusCode = httpGet(endpoint, doc);
    if (!isHttpSuccessStatus(statusCode))
    {
        logError("HueGatewayClient", "getContactState failed HTTP %d", statusCode);
        return false;
    }

    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    if (data.size() == 0)
        return false;

    JsonObjectConst item = data[0].as<JsonObjectConst>();
    state.id = contactRid;
    // "contact" = geschlossen, "no_contact" = geöffnet
    const char* reportState = item["contact_report"]["state"] | "contact";
    state.contactOpen = (strcmp(reportState, "no_contact") == 0);

    // Besitzer-RID für optionale Zusatzdaten
    state.reachable = true;
    String ownerRid;
    if (item["owner"]["rtype"].as<String>() == "device")
        ownerRid = item["owner"]["rid"].as<String>();

    if (ownerRid.length() > 0)
    {
        fetchZigbeeReachableByOwner(ownerRid, state.reachable);
        fetchTamperByOwner(ownerRid, state.tampered);
        fetchTemperatureByOwner(ownerRid, state.temperature);
        fetchBatteryByOwner(ownerRid, state.batteryPercent);
    }
    return true;
}

// ===== Optionale Sensor-Zusatzdaten =====

bool HueGatewayClient::getOwnerRidFromResource(const String& resourceType, const String& resourceId, String& ownerRid)
{
    if (!_initialized) return false;
    DynamicJsonDocument doc(512);
    const int sc = httpGet("/clip/v2/resource/" + resourceType + "/" + resourceId, doc);
    if (!isHttpSuccessStatus(sc)) return false;
    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    if (data.size() == 0) return false;
    const String rtype = data[0]["owner"]["rtype"].as<String>();
    if (rtype != "device") return false;
    ownerRid = data[0]["owner"]["rid"].as<String>();
    return ownerRid.length() > 0;
}

bool HueGatewayClient::fetchTemperatureByOwner(const String& ownerRid, float& celsius)
{
    if (!_initialized) return false;
    DynamicJsonDocument doc(2048);
    const int sc = httpGet("/clip/v2/resource/temperature", doc);
    if (!isHttpSuccessStatus(sc)) return false;
    for (JsonObjectConst item : doc["data"].as<JsonArrayConst>())
    {
        if (item["owner"]["rid"].as<String>() == ownerRid)
        {
            celsius = item["temperature"]["temperature"] | NAN;
            return !isnan(celsius);
        }
    }
    return false;
}

bool HueGatewayClient::fetchLightLevelByOwner(const String& ownerRid, float& lux)
{
    if (!_initialized) return false;
    DynamicJsonDocument doc(2048);
    const int sc = httpGet("/clip/v2/resource/light_level", doc);
    if (!isHttpSuccessStatus(sc)) return false;
    for (JsonObjectConst item : doc["data"].as<JsonArrayConst>())
    {
        if (item["owner"]["rid"].as<String>() == ownerRid)
        {
            // Hue light_level: 10000 * log10(lux) + 1 → Rückrechnung
            const int32_t hueLux = item["light"]["light_level"] | 0;
            lux = (hueLux > 1) ? powf(10.0f, (hueLux - 1) / 10000.0f) : 0.0f;
            return true;
        }
    }
    return false;
}

bool HueGatewayClient::fetchBatteryByOwner(const String& ownerRid, uint8_t& percent)
{
    if (!_initialized) return false;
    DynamicJsonDocument doc(2048);
    const int sc = httpGet("/clip/v2/resource/device_power", doc);
    if (!isHttpSuccessStatus(sc)) return false;
    for (JsonObjectConst item : doc["data"].as<JsonArrayConst>())
    {
        if (item["owner"]["rid"].as<String>() == ownerRid)
        {
            const int pct = item["power_state"]["battery_level"] | -1;
            if (pct >= 0)
            {
                percent = static_cast<uint8_t>(min(pct, 100));
                return true;
            }
        }
    }
    return false;
}

bool HueGatewayClient::fetchTamperByOwner(const String& ownerRid, bool& tampered)
{
    if (!_initialized) return false;
    DynamicJsonDocument doc(2048);
    const int sc = httpGet("/clip/v2/resource/tamper", doc);
    if (!isHttpSuccessStatus(sc)) return false;
    for (JsonObjectConst item : doc["data"].as<JsonArrayConst>())
    {
        if (item["owner"]["rid"].as<String>() == ownerRid)
        {
            for (JsonObjectConst report : item["tamper_reports"].as<JsonArrayConst>())
            {
                tampered = (report["state"].as<String>() == "tampered");
                return true;
            }
        }
    }
    return false;
}

bool HueGatewayClient::fetchZigbeeReachableByOwner(const String& ownerRid, bool& reachable)
{
    if (!_initialized) return false;
    DynamicJsonDocument doc(2048);
    const int sc = httpGet("/clip/v2/resource/zigbee_connectivity", doc);
    if (!isHttpSuccessStatus(sc)) return false;
    for (JsonObjectConst item : doc["data"].as<JsonArrayConst>())
    {
        if (item["owner"]["rid"].as<String>() == ownerRid)
        {
            const String status = item["status"].as<String>();
            reachable = (status == "connected");
            return true;
        }
    }
    return false;
}

bool HueGatewayClient::getButtonState(const String& buttonRid, HueGatewayButtonState& state)
{
    if (!_initialized)
        return false;

    DynamicJsonDocument doc(1024);
    const String endpoint = "/clip/v2/resource/button/" + buttonRid;
    const int statusCode = httpGet(endpoint, doc);
    if (!isHttpSuccessStatus(statusCode))
    {
        logError("HueGatewayClient", "getButtonState failed HTTP %d", statusCode);
        return false;
    }

    JsonArrayConst data = doc["data"].as<JsonArrayConst>();
    if (data.size() == 0)
        return false;

    JsonObjectConst item = data[0].as<JsonObjectConst>();
    state.id = buttonRid;
    state.buttonIndex = item["metadata"]["control_id"] | 0;
    state.lastEventType = item["button"]["last_event"] | "none";
    return true;
}

// ===== Erweiterter Eventstream mit Sensorereignissen =====

int HueGatewayClient::parseEventPayloadFull(const String& payload,
                                             HueGatewayEventLightUpdate* lightUpdates, int maxLightUpdates,
                                             HueGatewayEventSensorUpdate* sensorUpdates, int maxSensorUpdates,
                                             int& sensorCount)
{
    sensorCount = 0;

    // Licht-Events mit der existierenden Methode auslesen
    int lightCount = 0;
    if (lightUpdates != nullptr && maxLightUpdates > 0)
        lightCount = parseEventPayload(payload, lightUpdates, maxLightUpdates);

    if (sensorUpdates == nullptr || maxSensorUpdates <= 0 || payload.length() == 0)
        return lightCount;

    static DynamicJsonDocument sensorDoc(4096);
    sensorDoc.clear();
    if (deserializeJson(sensorDoc, payload) != DeserializationError::Ok)
        return lightCount;

    JsonArrayConst events = sensorDoc.as<JsonArrayConst>();
    for (JsonObjectConst eventObj : events)
    {
        JsonArrayConst data = eventObj["data"].as<JsonArrayConst>();
        for (JsonObjectConst item : data)
        {
            if (sensorCount >= maxSensorUpdates)
                break;

            const char* type = item["type"] | "";
            const char* id = item["id"] | "";
            if (id[0] == '\0')
                continue;

            if (strcmp(type, "motion") == 0)
            {
                JsonVariantConst motionVar = item["motion"]["motion"];
                if (!motionVar.isNull())
                {
                    HueGatewayEventSensorUpdate& su = sensorUpdates[sensorCount++];
                    su.type = HueGatewayEventSensorUpdate::Type::Motion;
                    su.resourceId = String(id);
                    su.motionDetected = motionVar.as<bool>();
                }
            }
            else if (strcmp(type, "contact_sensor") == 0)
            {
                const char* s = item["contact_report"]["state"] | "";
                if (s[0] != '\0')
                {
                    HueGatewayEventSensorUpdate& su = sensorUpdates[sensorCount++];
                    su.type = HueGatewayEventSensorUpdate::Type::Contact;
                    su.resourceId = String(id);
                    su.contactOpen = (strcmp(s, "no_contact") == 0);
                }
            }
            else if (strcmp(type, "button") == 0)
            {
                const char* lastEvent = item["button"]["last_event"] | "";
                if (lastEvent[0] != '\0')
                {
                    HueGatewayEventSensorUpdate& su = sensorUpdates[sensorCount++];
                    su.type = HueGatewayEventSensorUpdate::Type::Button;
                    su.resourceId = String(id);
                    su.buttonIndex = item["metadata"]["control_id"] | 0;
                    su.buttonEventType = String(lastEvent);
                    logDebug("HueGatewayClient", "SSE btn: rid=%s ctrl=%d event=%s",
                             id, su.buttonIndex, lastEvent);
                }
            }
            else if (strcmp(type, "behavior_instance") == 0)
            {
                _behaviorInstanceEventPending = true;
                logDebug("HueGatewayClient", "SSE behavior_instance event: id=%s", id);
            }
            else if (strcmp(type, "relative_rotary") == 0)
            {
                JsonObjectConst rotation = item["relative_rotary"]["last_event"]["rotation"];
                if (!rotation.isNull())
                {
                    const char* direction = rotation["direction"] | "";
                    int steps    = rotation["steps"]    | 0;
                    int duration = rotation["duration"] | 0;
                    if (direction[0] != '\0' && steps != 0)
                    {
                        HueGatewayEventSensorUpdate& su = sensorUpdates[sensorCount++];
                        su.type = HueGatewayEventSensorUpdate::Type::Rotary;
                        su.resourceId = String(id);
                        su.rotaryClockwise  = (strcmp(direction, "clock_wise") == 0);
                        su.rotarySteps      = steps;
                        su.rotaryDurationMs = duration;
                    }
                }
            }
        }
    }

    return lightCount;
}

int HueGatewayClient::pollEventStreamFull(HueGatewayEventLightUpdate* lightUpdates, int maxLightUpdates,
                                           HueGatewayEventSensorUpdate* sensorUpdates, int maxSensorUpdates,
                                           int& sensorCount)
{
    // Eventstream-Handshake und Verbindungsaufbau über die bestehende Infrastruktur
    // Die Sensor-Ereignisse werden zusätzlich zum normalen Light-Polling ausgelesen.
    // pollEventStreamFull nutzt denselben _eventDataBuffer-Mechanismus wie pollEventStream,
    // ruft aber parseEventPayloadFull auf, um beide Ereignistypen zu verarbeiten.

    sensorCount = 0;
    int lightCount = 0;

    if (!_eventStreamConnected && !_eventHandshakePending)
        return 0;

    // Handshake-Timeout prüfen
    if (_eventHandshakePending && (millis() - _eventHandshakeStartMs) > 10000UL)
    {
        logError("HueGatewayClient", "EventStream handshake timeout");
        _diagStats.eventHandshakeTimeoutCount++;
        stopEventStream();
        return 0;
    }

    while (_eventClient.available())
    {
        char c = static_cast<char>(_eventClient.read());
        _eventLastDataMs = millis();

        // Handshake-Zeilenende erkennen
        if (_eventHandshakePending)
        {
            if (c == '\n')
            {
                _eventLineBuffer.trim();
                if (_eventLineBuffer.length() == 0 && !_eventStreamConnected)
                {
                    int statusLine = _eventLineBuffer.toInt();
                    (void)statusLine;
                    _eventStreamConnected = true;
                    _eventHandshakePending = false;
                    _diagStats.eventConnectOk++;
                    logInfo("HueGatewayClient", "EventStream handshake complete");
                }
                _eventLineBuffer = "";
            }
            else if (c != '\r')
            {
                if (_eventLineBuffer.length() < kMaxEventLineChars)
                    _eventLineBuffer += c;
            }
            continue;
        }

        if (c == '\n')
        {
            if (_eventLineBuffer.length() == 0)
            {
                if (_eventDataBuffer.length() > 0)
                {
                    int sc = 0;
                    int lc = parseEventPayloadFull(_eventDataBuffer,
                                                    lightUpdates + lightCount,
                                                    maxLightUpdates - lightCount,
                                                    sensorUpdates != nullptr ? sensorUpdates + sensorCount : nullptr,
                                                    maxSensorUpdates - sensorCount,
                                                    sc);
                    if (lc < 0)
                    {
                        _eventDropCount++;
                    }
                    else
                    {
                        lightCount += lc;
                        sensorCount += sc;
                    }
                    _eventDataBuffer = "";
                }
            }
            else if (_eventLineBuffer.startsWith("data:"))
            {
                String dataPart = _eventLineBuffer.substring(5);
                dataPart.trim();
                _eventDataBuffer += dataPart;
                if (_eventDataBuffer.length() > kMaxEventPayloadChars)
                {
                    _eventDropCount++;
                    _eventDataBuffer = "";
                }
            }
            _eventLineBuffer = "";
        }
        else if (c != '\r')
        {
            if (_eventLineBuffer.length() < kMaxEventLineChars)
                _eventLineBuffer += c;
        }
    }

    if (!_eventClient.connected())
    {
        _diagStats.eventDisconnectCount++;
        logInfo("HueGatewayClient", "EventStream disconnected (full poll)");
        stopEventStream();
    }

    return lightCount;
}

