//
// Created by chris on 14.02.23.
//

#ifndef WIFI_MGR_H
#define WIFI_MGR_H

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#define XWebServer ESP8266WebServer
#elif defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#define XWebServer WebServer
#else
#error "This hardware is not supported"
#endif

void setupWifi(const char* SSID, const char* password);
void setupWifi(const char* SSID, const char* password, const char* hostname);
void setupWifi(const char* SSID, const char* password, const char* hostname, unsigned long tolerateBadRSSms, unsigned long waitForConnectMs);
void setupWifi(const char* SSID, const char* password, const char* hostname, unsigned long tolerateBadRSSms, unsigned long waitForConnectMs, unsigned long wifiMgrWaitForScanMs, unsigned long rescanInterval);
void loopWifi();
void wifiMgrExpose(XWebServer *server_);
void wifiMgrSetRebootAfterUnsuccessfullTries(uint8_t _wifiMgrRebootAfterUnsuccessfullTries);
void wifiMgrSetBadRSSI(int8_t rssi);
void wifiMgrNotifyNoWifi(void (*wifiMgrNotifyNoWifiCallbackArg)(void), unsigned long timeout);
void setLoopFunction(void (*loopFunctionPointerArg)(void));

#endif //WIFI_MGR_H
