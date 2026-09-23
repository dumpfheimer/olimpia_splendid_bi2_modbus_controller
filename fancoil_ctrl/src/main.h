//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_MAIN_H
#define FANCOIL_CTRL_MAIN_H

#include "configuration.h"
#include "wifi_mgr.h"
#include "wifi_mgr_portal.h"

#define MODBUS_SERIAL modbusSerial

#if defined(ESP8266)
#include <SoftwareSerial.h>
extern SoftwareSerial modbusSerial;
#elif defined(ESP32)
#include <HardwareSerial.h>
extern HardwareSerial modbusSerial;
#else
#error "This hardware is not supported"
#endif

#include <LittleFS.h>

#include "httpHandlers.h"
#include "mqtt.h"
#include "fancoil.h"
#include "modbus_ascii.h"
#include "logging.h"


// RS485 pinout. DE and the modbus RX line must stay OFF the boot-strap pins:
// on the D1 mini v3.0.0 GPIO0 (D3) and GPIO2 (D4) carry 12k pull-ups (R10/R11)
// and must both be HIGH at reset, so anything that holds them low - a transceiver
// RO output, or the pull-down DE needs to fail safe - puts the board into
// flash-download mode instead of booting. That is a dead controller that no
// watchdog recovers: black, no WiFi, until it is power cycled.
// D2/GPIO4, D6/GPIO12 and D7/GPIO13 are bare pins with no strap function.
// DE is pulled down (10k) and RE pulled up (10k to 3V3) at the transceiver, so a
// broken signal wire disables the driver instead of leaving it jamming the bus.
#define DRIVER_ENABLE_PIN D2
#define READ_ENABLE_PIN   D7

extern XWebServer server;

// lowest free heap ever observed (sampled every main loop pass); exposed via
// /uptime to distinguish a slow leak from transient dips
extern uint32_t heapLowWaterMark;


#endif //FANCOIL_CTRL_MAIN_H
