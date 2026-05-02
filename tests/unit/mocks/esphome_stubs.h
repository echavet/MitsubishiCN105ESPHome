/// esphome_stubs.h — Stubs minimaux pour compiler les fonctions CN105 hors ESPHome.
/// Deps: <cstdio>, <cstdint>, <cstring>, <cmath>, <string>, <functional>
#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <functional>

// Stub des macros de log ESPHome → no-op en mode test
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)
