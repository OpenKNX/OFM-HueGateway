#include "HueGatewayStorage.h"
#include "HueGatewayNvsKeys.h"
#include <Preferences.h>

namespace HueGatewayStorage
{
namespace
{
String loadString(const char* key)
{
    Preferences prefs;
    prefs.begin(HueGatewayNvs::Namespace, true);
    String value = prefs.getString(key, "");
    prefs.end();
    return value;
}

void saveString(const char* key, const String& value)
{
    Preferences prefs;
    prefs.begin(HueGatewayNvs::Namespace, false);
    prefs.putString(key, value);
    prefs.end();
}
}

String loadAppKey()
{
    return loadString(HueGatewayNvs::AppKey);
}

String loadClientKey()
{
    return loadString(HueGatewayNvs::ClientKey);
}

String loadBridgeIp()
{
    return loadString(HueGatewayNvs::BridgeIp);
}

void saveAppKey(const String& key)
{
    saveString(HueGatewayNvs::AppKey, key);
}

void saveClientKey(const String& key)
{
    saveString(HueGatewayNvs::ClientKey, key);
}

void saveBridgeIp(const String& ip)
{
    saveString(HueGatewayNvs::BridgeIp, ip);
}

void clearAuthKeys()
{
    Preferences prefs;
    prefs.begin(HueGatewayNvs::Namespace, false);
    prefs.remove(HueGatewayNvs::AppKey);
    prefs.remove(HueGatewayNvs::ClientKey);
    prefs.end();
}

uint32_t loadRestoreBrightness(uint8_t channel)
{
    const String key = String(HueGatewayNvs::RestoreBrightnessPrefix) + String(channel);
    Preferences prefs;
    prefs.begin(HueGatewayNvs::Namespace, true);
    const uint32_t value = prefs.getULong(key.c_str(), 0);
    prefs.end();
    return value;
}

void saveRestoreBrightness(uint8_t channel, uint32_t value)
{
    const String key = String(HueGatewayNvs::RestoreBrightnessPrefix) + String(channel);
    Preferences prefs;
    prefs.begin(HueGatewayNvs::Namespace, false);
    prefs.putULong(key.c_str(), value);
    prefs.end();
}
}
