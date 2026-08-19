#ifndef CONFIGURATION
#define CONFIGURATION

#define AMBIENT_TEMPERATURE_TIMEOUT_S 1200 // if defined, the device will turn off after not receiving an ambient temperature after n seconds
#define LOAD_WATER_TEMP

#define USE_LOGGING

#if __has_include("my_config.h")
#include "my_config.h"
#endif

// minutes without a master write before a fancoil switches itself off
// (register 101 comm-watchdog nibble; 0 = disabled, max 15). Protects against
// a dead controller and against units that lose their address (fallback to 1)
// and silently stop receiving writes.
#ifndef FANCOIL_COMM_WATCHDOG_MINUTES
#define FANCOIL_COMM_WATCHDOG_MINUTES 15
#endif


#endif