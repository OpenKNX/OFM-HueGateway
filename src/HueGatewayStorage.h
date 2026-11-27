#pragma once

#include <Arduino.h>

namespace HueGatewayStorage
{
String loadAppKey();
String loadClientKey();
String loadBridgeIp();

void saveAppKey(const String& key);
void saveClientKey(const String& key);
void saveBridgeIp(const String& ip);

void clearAuthKeys();

// Remembered switch-on brightness of a channel (1-based), 0 = none stored
uint32_t loadRestoreBrightness(uint8_t channel);
void saveRestoreBrightness(uint8_t channel, uint32_t value);
}
