const char* kSsid = "XXX";
const char* kPassword = "XXX";
const char* wifiHost = "FancoilCtrl";

#define AMBIENT_TEMPERATURE_TIMEOUT_S 10000 // if defined, the device will turn off after not receiving an ambient temperature after n seconds. if this line is commented out, the ambient temperature will never expire
#define USE_LOGGING
/*
// FOR USE WITH MQTT CONFIGURE THIS SECTION
#define MQTT_HOST ""
#define MQTT_USER ""
#define MQTT_PASS ""
*/
