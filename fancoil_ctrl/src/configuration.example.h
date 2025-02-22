#ifndef CONFIGURATION
#define CONFIGURATION

#define WIFI_SSID "XXX"
#define WIFI_PASSWORD "XXX"
#define WIFI_HOST "FancoilCtrl"


#define AMBIENT_TEMPERATURE_TIMEOUT_S 1200 // if defined, the device will turn off after not receiving an ambient temperature after n seconds
#define USE_LOGGING

#define MODEL "Bi2 SL Smart 400"
#define MANUFACTURER "Olimpia Splendid"

// if you want to track the water temperat ure uncomment:
// #define LOAD_WATER_TEMP

/*
// FOR USE WITH MQTT CONFIGURE THIS SECTION
// IP of the host running MQTT (without port, e.g. 192.168.0.99)
#define MQTT_HOST ""
// MQTT user (note that you might need to restart MQTT server after adding a user)
#define MQTT_USER ""
#define MQTT_PASS ""
*/

#endif
