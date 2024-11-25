#ifndef CONFIGURATION
#define CONFIGURATION

static const char* kSsid = "XXX";
static const char* kPassword = "XXX";
static const char* wifiHost = "FancoilCtrl";

#define AMBIENT_TEMPERATURE_TIMEOUT_S 1200 // if defined, the device will turn off after not receiving an ambient temperature after n seconds
#define USE_LOGGING

// if you want to track the water temperature uncomment:
// #define LOAD_WATER_TEMP

/*
// FOR USE WITH MQTT CONFIGURE THIS SECTION
#define MQTT_HOST ""
#define MQTT_USER ""
#define MQTT_PASS ""
*/

#endif