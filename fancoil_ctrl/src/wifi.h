//
// Created by chris on 14.02.23.
//

#ifndef WIFI_MGR_H
#define WIFI_MGR_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

void setupWifi(const char* SSID, const char* password);
void setupWifi(const char* SSID, const char* password, const char* hostname);
void setupWifi(const char* SSID, const char* password, const char* hostname, unsigned long tolerateBadRSSms, unsigned long waitForConnectMs);
void setupWifi(const char* SSID, const char* password, const char* hostname, unsigned long tolerateBadRSSms, unsigned long waitForConnectMs, unsigned long rescanInterval);
void loopWifi();
void wifiMgrExpose(ESP8266WebServer *server_);
void wifiMgrSetBadRSSI(int8_t rssi);
void setLoopFunction(void (*loopFunctionPointerArg)(void));

#endif //WIFI_MGR_H
