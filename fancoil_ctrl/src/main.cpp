#include "main.h"

XWebServer server(80);

#if defined(ESP8266)
// instantiate ModbusMaster object
// RX on D6/GPIO12, not D4/GPIO2: a 5V MAX485 needs its RO level-shifted down
// (2k2 series / 3k3 to GND), and that divider holds whatever pin it feeds low
// while RO is high-Z - fatal on GPIO2, which must be HIGH at reset. See main.h.
SoftwareSerial modbusSerial(D6, D1);
#elif defined(ESP32)
HardwareSerial modbusSerial(1);
#else
#error "This hardware is not supported"
#endif

void loopDuringWifi() {
    loopFancoils(&MODBUS_SERIAL);
}

void handleClientDuringModbus() {
    server.handleClient();
}

void setup() {
    pinMode(READ_ENABLE_PIN, OUTPUT);
    pinMode(DRIVER_ENABLE_PIN, OUTPUT);

    digitalWrite(READ_ENABLE_PIN, 1);
    digitalWrite(DRIVER_ENABLE_PIN, 0);

    setupLogging();
    setupModbus();
    
    // Initialize LittleFS
    if (!LittleFS.begin()) {
        debugPrintln("Failed to mount LittleFS");
    } else {
        debugPrintln("LittleFS mounted successfully");
    }

#if defined(ESP8266)
    modbusSerial.begin(9600, SWSERIAL_7E1);
#elif defined(ESP32)
    modbusSerial.begin(9600, SERIAL_7E1, -1, -1, true);
#endif

    modbusSerial.setTimeout(500);


    debugPrintln("Connecting to WiFi..");
    wifiMgrExpose(&server);

    // Recovery: the wifi manager disables the SDK auto-reconnect, and a failed
    // attempt leaves the radio in WIFI_OFF. If the ESP8266 gets stuck in the
    // scan-never-succeeds state, only a reset recovers it. ~10 tries at one
    // every 10s = reboot after ~2 minutes without WiFi.
    wifiMgrSetRebootAfterUnsuccessfullTries(10);

#if defined(ESP8266)
    // reduce TX power from the 20.5dBm default: the fleet's recurring
    // hardware-watchdog crashes resolve to the WiFi blob's TX-path interrupt
    // handlers (lmacProcessTxSuccess, wDev_ProcessFiq), and lowering PA
    // stress is the known mitigation. RSSI runs -52..-67 here, so 3.5dB of
    // margin is unused anyway.
    WiFi.setOutputPower(17.0);
#endif

#ifdef WIFI_SSID
    setupWifi(WIFI_SSID, WIFI_PASSWORD, WIFI_HOST);
#else
    wifiMgrPortalSetup(false, "FancoilCtrl-", "p0rtal123");
#endif
#ifndef MQTT_HOST
    wifiMgrPortalAddConfigEntry("MQTT Host", "MQTT_HOST", PortalConfigEntryType::STRING, false, true);
    wifiMgrPortalAddConfigEntry("MQTT Username", "MQTT_USER", PortalConfigEntryType::STRING, false, true);
    wifiMgrPortalAddConfigEntry("MQTT Password", "MQTT_PASS", PortalConfigEntryType::STRING, true, true);
    wifiMgrPortalAddConfigEntry("HA Extra Entities", "HA_XTRA", PortalConfigEntryType::BOOL, false, true);
    wifiMgrPortalAddConfigEntry("HA Manufacturer", "HA_MAN", PortalConfigEntryType::STRING, false, true);
    wifiMgrPortalAddConfigEntry("HA Model", "HA_MOD", PortalConfigEntryType::STRING, false, true);
#endif

    setupHttp();

    MODBUS_SERIAL.setTimeout(5000);

    setupFancoilManager();
    setLoopFunction(loopDuringWifi);
    setModbusYieldCallback(handleClientDuringModbus);
    setupMqtt();
}

// reading 601-602
// 0258 = HEX 600
//            :  0  1  0  3  0  2  5  8  0  0  0  2  A  0  \r \n
// Request:   3A 30 31 30 33 30 32 35 38 30 30 30 32 41 30 0D 0A
// Response:  3A 30 31 30 33 30 34 30 33 45 38 31 33 38 38 37 32 0D 0A
uint32_t heapLowWaterMark = 0xFFFFFFFF;

// Self-restart for states where the device is functionally dead but both
// hardware watchdogs stay fed because the loop keeps spinning politely:
// (a) the largest allocatable block is too small for the network stack to
//     rebuild sockets or reassociate - no recovery is possible on this heap
// (b) WiFi is associated but MQTT stays unconnectable - the "zombie" mode
//     where sockets starve while WiFi.isConnected() reports healthy, so the
//     reboot-after-failed-reconnects logic never engages
// Each condition must hold continuously for its whole window; any healthy
// sample resets its timer, so transient dips (discovery bursts, reconnects)
// never trigger it. A restart is safe for running fancoils by design
// (transparent-reboot guard + modbus watchdog nibble written as 0).
unsigned long lowHeapSince = 0;
unsigned long mqttDeadSince = 0;
#define LOW_HEAP_BLOCK_LIMIT 4096
#define LOW_HEAP_RESTART_MS (5 * 60 * 1000)
#define MQTT_DEAD_RESTART_MS (15 * 60 * 1000)

void checkSelfRestart() {
#if defined(ESP8266)
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
#elif defined(ESP32)
    uint32_t maxBlock = ESP.getMaxAllocHeap();
#endif
    if (maxBlock >= LOW_HEAP_BLOCK_LIMIT) {
        lowHeapSince = 0;
    } else if (lowHeapSince == 0) {
        lowHeapSince = millis();
    } else if ((millis() - lowHeapSince) > LOW_HEAP_RESTART_MS) {
        ESP.restart();
    }

    if (!mqttIsConfigured() || !WiFi.isConnected() || mqttIsConnected()) {
        mqttDeadSince = 0;
    } else if (mqttDeadSince == 0) {
        mqttDeadSince = millis();
    } else if ((millis() - mqttDeadSince) > MQTT_DEAD_RESTART_MS) {
        ESP.restart();
    }
}

void loop() {
    uint32_t freeHeapNow = ESP.getFreeHeap();
    if (freeHeapNow < heapLowWaterMark) heapLowWaterMark = freeHeapNow;
    checkSelfRestart();
#ifdef WIFI_SSID
    server.handleClient();
    loopWifi();
    server.handleClient();

    loopFancoils(&MODBUS_SERIAL);

#ifdef MQTT_HOST
    loopMqtt();
#endif
#else
    if (wifiMgrPortalLoop()) {
        yield();
        loopFancoils(&MODBUS_SERIAL);
        yield();
        loopMqtt();
    }
#endif
}
