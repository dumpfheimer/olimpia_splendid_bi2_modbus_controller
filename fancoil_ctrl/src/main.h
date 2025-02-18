//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_MAIN_H
#define FANCOIL_CTRL_MAIN_H

#define MODBUS_SERIAL modbusSerial

#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include "ota.h"
#include "httpHandlers.h"
#include "mqtt.h"
#include "fancoil.h"
#include "wifi_mgr.h"


#define DRIVER_ENABLE_PIN D3
#define READ_ENABLE_PIN   D2

extern ESP8266WebServer server;
extern SoftwareSerial modbusSerial;


#endif //FANCOIL_CTRL_MAIN_H
