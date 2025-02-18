#include "main.h"

XWebServer server(80);

#ifndef WIFI_SSID
#include "wifi_mgr_portal.h"
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
    modbusSerial.begin(9600, SERIAL_7E1, -1, -1, true);
#endif

    modbusSerial.setTimeout(500);


    debugPrintln("Connecting to WiFi..");
    wifiMgrExpose(&server);
#ifdef WIFI_SSID
    setupWifi(WIFI_SSID, WIFI_PASSWORD, WIFI_HOST);
#else
    wifiMgrPortalSetup(false);
    wifiMgrPortalAddConfigEntry("MQTT Host", "MQTT_HOST", PortalConfigEntryType::STRING, false, true);
    wifiMgrPortalAddConfigEntry("MQTT Username", "MQTT_USER", PortalConfigEntryType::STRING, false, true);
    wifiMgrPortalAddConfigEntry("MQTT Password", "MQTT_PASS", PortalConfigEntryType::STRING, false, true);
#endif

    debugPrintln(WiFi.localIP().toString());

    setupOTA();
    setupHttp();

    MODBUS_SERIAL.setTimeout(5000);

    setupFancoilManager();
    setupMqtt();
}

// reading 601-602
// 0258 = HEX 600
//            :  0  1  0  3  0  2  5  8  0  0  0  2  A  0  \r \n
// Request:   3A 30 31 30 33 30 32 35 38 30 30 30 32 41 30 0D 0A
// Response:  3A 30 31 30 33 30 34 30 33 45 38 31 33 38 38 37 32 0D 0A
void loop() {
    server.handleClient();
#ifdef WIFI_SSID
    loopWifi();
    server.handleClient();
    loopOTA();

    loopFancoils(&MODBUS_SERIAL);

#ifdef MQTT_HOST
    loopMqtt();
#endif
#else
    if (wifiMgrPortalLoop()) {
        server.handleClient();
        loopOTA();
        loopFancoils(&MODBUS_SERIAL);
        loopMqtt();
    }
#endif
}
