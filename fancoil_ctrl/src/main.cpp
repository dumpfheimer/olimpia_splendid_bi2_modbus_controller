#include "main.h"

XWebServer server(80);

// pins come from board_config.h, selected by the build environment
#if defined(MODBUS_SOFTWARE_SERIAL)
SoftwareSerial modbusSerial(MODBUS_RX_PIN, MODBUS_TX_PIN, MODBUS_INVERT);
#elif defined(ESP32)
HardwareSerial modbusSerial(1);
#endif

#if defined(ESP8266)
// Supply-rail instrumentation.
//
// A freeze that needs a physical power cycle looks the same from outside
// whatever caused it, and a sagging 3V3 rail is invisible to every other
// counter we expose. The ESP can measure its own supply instead of A0, which
// is unconnected on every board revision here, so this costs nothing.
//
// The minimum is kept in RTC memory as well as RAM: RTC survives a watchdog
// reset, so after a crash we can still see how low the rail got BEFORE it -
// which is the difference between "browned out" and "the WiFi stack died on
// a healthy rail". It does not survive removing power, so a freeze that you
// recover by power cycling still loses the number; poll /uptime while the
// device is alive to catch a downward trend.
ADC_MODE(ADC_VCC);

struct RtcDiag {
    uint32_t magic;
    uint16_t vccMin;
    uint16_t reserved;
};
#define RTC_DIAG_MAGIC  0x56434331UL   // "VCC1"
#define RTC_DIAG_OFFSET 64             // 4-byte-word offset, well clear of anything else

uint16_t vccNow = 0;
uint16_t vccMinThisBoot = 0xFFFF;
uint16_t vccMinAcrossResets = 0xFFFF;
static unsigned long lastVccSample = 0;

static void loadVccMin() {
    RtcDiag d;
    if (ESP.rtcUserMemoryRead(RTC_DIAG_OFFSET, (uint32_t *) &d, sizeof(d)) && d.magic == RTC_DIAG_MAGIC) {
        vccMinAcrossResets = d.vccMin;
    }
}

static void sampleVcc() {
    // once a second: the reading briefly borrows the ADC, and the rail does
    // not need millisecond resolution to show a sag
    if ((millis() - lastVccSample) < 1000) return;
    lastVccSample = millis();

    vccNow = ESP.getVcc();
    if (vccNow < vccMinThisBoot) vccMinThisBoot = vccNow;
    if (vccNow < vccMinAcrossResets) {
        vccMinAcrossResets = vccNow;
        // only on a new minimum, so this is rare rather than once a second
        RtcDiag d = {RTC_DIAG_MAGIC, vccMinAcrossResets, 0};
        ESP.rtcUserMemoryWrite(RTC_DIAG_OFFSET, (uint32_t *) &d, sizeof(d));
    }
}
#endif

// Called by the wifi manager from inside its connect-wait loop. Modbus
// transactions are blocking, so a registered-but-absent unit (wrong address,
// powered-off fancoil, stale EEPROM entry) burns the whole connect window in
// read timeouts. The attempt then fails, wifiNotifyUnsuccessfullTry() counts
// it, and after wifiMgrSetRebootAfterUnsuccessfullTries(10) the board reboots
// - into the same trap. That reboot loop looks exactly like "the device does
// not boot with the bus attached".
// Nothing is lost by staying silent until the first association: the ambient
// temperature that gates every write arrives over the network anyway.
bool wifiEverConnected = false;

void loopDuringWifi() {
    if (!wifiEverConnected) {
        if (!WiFi.isConnected()) {
            server.handleClient();
            return;
        }
        wifiEverConnected = true;
    }
    loopFancoils(&MODBUS_SERIAL);
}

void handleClientDuringModbus() {
    server.handleClient();
}

#if defined(ESP8266)
// Earliest user code the core offers: called from user_init() before the WiFi
// stack is brought up, and well before setup().
//
// From reset until a pin is configured as an output it is an input, so the
// transceiver module's 10k pull-up on DE keys the RS485 driver: the bus is
// jammed and the board draws an extra ~40-80mA - exactly while RF calibration
// is pulling its own peak current, which is when the rail is weakest.
// This does not close the window (the boot ROM runs first, and only a hardware
// pull-down on DE can cover that - BOARD_V2 has one), but it shortens it from
// the whole SDK startup to a few milliseconds.
extern "C" void preinit() {
    pinMode(DRIVER_ENABLE_PIN, OUTPUT);
    digitalWrite(DRIVER_ENABLE_PIN, 0);   // driver off
    pinMode(READ_ENABLE_PIN, OUTPUT);
    digitalWrite(READ_ENABLE_PIN, 1);     // receiver off
}
#endif

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

#if defined(MODBUS_SOFTWARE_SERIAL)
    modbusSerial.begin(9600, SWSERIAL_7E1);
    // begin() arms the RX pin-change interrupt. Leave it detached until a
    // read actually wants it: between transactions the transceiver's RO is
    // high-Z and the pin level can sit in the undefined band, which would
    // make that interrupt fire continuously. See modbus_ascii.cpp.
    modbusSerial.enableRx(false);
#elif defined(ESP32)
    modbusSerial.begin(9600, SERIAL_7E1, -1, -1, true);
#else
    // BOARD_V2: hardware UART0, native 7E1. setupLogging() has already moved
    // the console to Serial1, so nothing else writes to this port.
    modbusSerial.begin(9600, SERIAL_7E1);
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
#if defined(ESP8266)
    loadVccMin();   // carry the rail minimum across a watchdog reset
#endif
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

    // The zombie case this targets is "our sockets are broken while WiFi still
    // reports healthy", and only a reboot clears it. But an ordinary broker
    // outage is indistinguishable from the outside, and rebooting then is
    // actively harmful: every controller in the fleet reboots every 15 minutes
    // for as long as the broker is down, losing its desired state each time
    // (observed 2026-09-21, mosquitto stopped). So require evidence that it is
    // US and not the broker: if a plain TCP connection to the gateway also
    // fails, our networking really is dead. If the gateway answers, the stack
    // works and the broker is simply gone - keep running, keep the fancoils
    // serviced, keep the web UI up, and wait for it to come back.
    if (!mqttIsConfigured() || !WiFi.isConnected() || mqttIsConnected()) {
        mqttDeadSince = 0;
    } else if (mqttDeadSince == 0) {
        mqttDeadSince = millis();
    } else if ((millis() - mqttDeadSince) > MQTT_DEAD_RESTART_MS) {
        // Caveat: a gateway that refuses both ports is indistinguishable from
        // a broken stack, and we reboot - which is exactly the old behaviour,
        // so this can only reduce spurious restarts, never add one. Two ports
        // because some routers expose their UI on 443 only.
        bool stackAlive = false;
        for (uint16_t port : {80, 443}) {
            WiFiClient probe;
            probe.setTimeout(1000);
            if (probe.connect(WiFi.gatewayIP(), port)) stackAlive = true;
            probe.stop();
            if (stackAlive) break;
        }
        if (!stackAlive) {
            ESP.restart();
        } else {
            // broker's fault, not ours - re-arm and re-check in another window
            mqttDeadSince = millis();
        }
    }
}

void loop() {
    uint32_t freeHeapNow = ESP.getFreeHeap();
    if (freeHeapNow < heapLowWaterMark) heapLowWaterMark = freeHeapNow;
#if defined(ESP8266)
    sampleVcc();
#endif
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
