#include "main.h"

void debugPrint(String s);

void debugPrint(unsigned long l, int i);

void debugPrintln(String s);

void debugPrintln(unsigned long l, int i);

XWebServer server(80);

#include "modbus_ascii.h"

#include "configuration.h"


#ifdef MQTT_HOST

void notifyStateChanged();

#endif




#if defined(ESP8266)
// instantiate ModbusMaster object
SoftwareSerial modbusSerial(D4, D1, true);
#elif defined(ESP32)
HardwareSerial modbusSerial(1);
#else
#error "This hardware is not supported"
#endif



void setup() {
    pinMode(READ_ENABLE_PIN, OUTPUT);
    pinMode(DRIVER_ENABLE_PIN, OUTPUT);

    digitalWrite(READ_ENABLE_PIN, 1);
    digitalWrite(DRIVER_ENABLE_PIN, 0);

    setupModbus();
    setupLogging();

#if defined(ESP8266)
    modbusSerial.begin(9600, SWSERIAL_7E1);
#elif defined(ESP32)
    modbusSerial.begin(9600, SERIAL_7E1);
#endif

    modbusSerial.setTimeout(500);


    debugPrintln("Connecting to WiFi..");
#ifdef WIFI_SSID
    setupWifi(WIFI_SSID, WIFI_PASSWORD, WIFI_HOST);
#else
    setupWifi(kSsid, kPassword, wifiHost);
#endif
    wifiMgrExpose(&server);

    debugPrintln(WiFi.localIP().toString());

    setupOTA();
    setupHttp();

    MODBUS_SERIAL.setTimeout(5000);

    setupFancoilManager();

#ifdef MQTT_HOST
    setupMqtt();
#endif
}

// reading 601-602
// 0258 = HEX 600
//            :  0  1  0  3  0  2  5  8  0  0  0  2  A  0  \r \n
// Request:   3A 30 31 30 33 30 32 35 38 30 30 30 32 41 30 0D 0A
// Response:  3A 30 31 30 33 30 34 30 33 45 38 31 33 38 38 37 32 0D 0A
void loop() {
    loopWifi();
    server.handleClient();
    loopOTA();

    loopFancoils(&MODBUS_SERIAL);

#ifdef MQTT_HOST
    loopMqtt();
#endif
}
