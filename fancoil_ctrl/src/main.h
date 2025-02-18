//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_MAIN_H
#define FANCOIL_CTRL_MAIN_H

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

#include "ota.h"
#include "httpHandlers.h"
#include "mqtt.h"
#include "fancoil.h"
#include "wifi_mgr.h"


#define DRIVER_ENABLE_PIN D3
#define READ_ENABLE_PIN   D2

extern XWebServer server;


#endif //FANCOIL_CTRL_MAIN_H
