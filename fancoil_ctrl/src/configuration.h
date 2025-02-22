#ifndef CONFIGURATION
#define CONFIGURATION

#define WIFI_MGR_EEPROM_START_ADDR 512
#define WIFI_MGR_EEPROM_SIZE 1024
#define AMBIENT_TEMPERATURE_TIMEOUT_S 1200 // if defined, the device will turn off after not receiving an ambient temperature after n seconds
#define LOAD_WATER_TEMP

#if __has_include("my_config.h")
#include "my_config.h"
#endif


#endif