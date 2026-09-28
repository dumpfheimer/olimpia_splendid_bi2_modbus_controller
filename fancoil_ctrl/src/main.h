//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_MAIN_H
#define FANCOIL_CTRL_MAIN_H

#include "configuration.h"
#include "board_config.h"
#include "wifi_mgr.h"
#include "wifi_mgr_portal.h"

#define MODBUS_SERIAL modbusSerial

#if defined(MODBUS_SOFTWARE_SERIAL)
#include <SoftwareSerial.h>
extern SoftwareSerial modbusSerial;
#elif defined(ESP32)
#include <HardwareSerial.h>
extern HardwareSerial modbusSerial;
#else
// BOARD_V2: modbus owns UART0 (GPIO1/GPIO3); the console moves to Serial1
#define modbusSerial Serial
#endif

#include <LittleFS.h>

#include "httpHandlers.h"
#include "mqtt.h"
#include "fancoil.h"
#include "modbus_ascii.h"
#include "logging.h"


// RS485 pinout and serial selection live in board_config.h, chosen by the
// build environment. Never hardcode pins here again: three different wirings
// are in service and the wrong one bricks a board until it is power cycled.

extern XWebServer server;

// lowest free heap ever observed (sampled every main loop pass); exposed via
// /uptime to distinguish a slow leak from transient dips
extern uint32_t heapLowWaterMark;

#if defined(ESP8266)
// 3V3 rail in mV, sampled once a second. vccMinAcrossResets is kept in RTC
// memory so it survives a watchdog reset - see main.cpp.
extern uint16_t vccNow;
extern uint16_t vccMinThisBoot;
extern uint16_t vccMinAcrossResets;
#endif


#endif //FANCOIL_CTRL_MAIN_H
