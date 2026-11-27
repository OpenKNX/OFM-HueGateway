#pragma once

namespace HueGatewayNvs {

static constexpr const char* Namespace = "hue";

static constexpr const char* AppKey = "app_key";
static constexpr const char* ClientKey = "client_key";
static constexpr const char* BridgeIp = "bridge_ip";

// Reserved prefix for future HCL master persistence keys (e.g. hclm_1, hclm_2, ...)
static constexpr const char* HclMasterPrefix = "hclm_";

// Remembered switch-on brightness per channel (lb_1, lb_2, ...)
static constexpr const char* RestoreBrightnessPrefix = "lb_";

} // namespace HueGatewayNvs
