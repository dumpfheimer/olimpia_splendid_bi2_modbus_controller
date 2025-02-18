#ifndef CONFIGURATION
#define CONFIGURATION

// you can force wifi credentials in the firmware, but you can also flash without fixed config and configure it over the web portal
// the device will open up an access point. you can connect and configure it over http://192.168.4.1/wifiMgr/configure

//#define WIFI_SSID "XXX"
//#define WIFI_PASSWORD "XXX"
//#define WIFI_HOST "FancoilCtrl"

#define AMBIENT_TEMPERATURE_TIMEOUT_S 1200 // if defined, the device will turn off after not receiving an ambient temperature after n seconds
//#define USE_LOGGING
#define LOAD_WATER_TEMP

// you can also use the http configuration instead of these params
//#define MQTT_HOST ""
// MQTT user (note that you might need to restart MQTT server after adding a user)
//#define MQTT_USER ""
//#define MQTT_PASS ""

#endif
