const char* kSsid = "XXX";
const char* kPassword = "XXX";
const char* wifiHost = "FancoilCtrl";

#define AMBIENT_TEMPERATURE_TIMEOUT_S 3600 // 1h if defined, the device will turn off after not receiving an ambient temperature after n seconds. if this line is commented out, the ambient temperature will never expire
#define USE_LOGGING

// if you want to track the water temperature uncomment:
// #define LOAD_WATER_TEMP

// if you want to track the water temperature uncomment:
// #define LOAD_AMBIENT_TEMP

/*
// FOR USE WITH MQTT CONFIGURE THIS SECTION
#define MQTT_HOST ""
#define MQTT_USER ""
#define MQTT_PASS ""
*/

// if you want the controller to read the state of the fan coil too, you can enable it with this option:
// otherwise the only way to change the state is by http/mqtt calls
// #define ENABLE_READ_STATE