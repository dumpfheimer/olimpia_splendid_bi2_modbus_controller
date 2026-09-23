#ifndef CONFIGURATION
#define CONFIGURATION

#define AMBIENT_TEMPERATURE_TIMEOUT_S 1200 // if defined, the device will turn off after not receiving an ambient temperature after n seconds
#define LOAD_WATER_TEMP

// swing (flap) control is enabled by default; define DISABLE_SWING to
// compile it out (register 224 is in the units' wear-limited EEPROM region,
// see fancoil.cpp).
//#define DISABLE_SWING

#define USE_LOGGING

#if __has_include("my_config.h")
#include "my_config.h"
#endif

// value written into register 101 bits 11-8, the "X communication timer".
// Per the official spec (docs/RDE109 rev7 rete MODBUS *.pdf): "do not
// modify" - no semantics are documented. History: for years the firmware
// ECHOED the last-read value here (doc-compliant, but the echo path once
// adopted garbage from misattributed responses); constant 0 has only been
// written since 2026-08-16 and one clean pre-change read showed the device
// holding 0 itself. If recData ever shows a non-zero nibble under constant-0
// writes, the device uses these bits and the (now validation-protected) echo
// should be restored instead. Value 15 is dead, see 2026-08-20 incident.
#ifndef FANCOIL_COMM_WATCHDOG_MINUTES
#define FANCOIL_COMM_WATCHDOG_MINUTES 0
#endif

#define WIFI_MGR_DISABLE_MDNS

// remote-mode guard: on the 10-minute probe cadence, read register 201 and
// if it is not 128 (unit fell back to local/autonomous control after a comm
// gap or power cycle) light the LOCAL MODE badge and write 128 to neutralize
// the local thermostat (stops autonomous fan/valve action). NOTE: this does
// NOT restore remote command execution - only the unit's panel (rE) has been
// observed to do that. Register semantics inferred+field-tested on Bi2 Wall
// TR only - comment out if your units differ.
#define FANCOIL_REMOTE_GUARD

#endif
