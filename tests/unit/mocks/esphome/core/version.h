#pragma once

#define VERSION_CODE(major, minor, patch) ((major) << 16 | (minor) << 8 | (patch))
#ifndef ESPHOME_VERSION_CODE
#define ESPHOME_VERSION_CODE VERSION_CODE(2026, 3, 0)
#endif
