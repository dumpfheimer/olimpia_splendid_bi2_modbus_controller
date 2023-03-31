//
// Created by chris on 14.02.23.
//

#ifndef WIFI_MGR_H
#define WIFI_MGR_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

void setupWifi(const char* SSID, const char* password, const char* hostname, unsigned long tolerateBadRSSms, unsigned long waitForConnectMs);
void loopWifi();

#endif //WIFI_MGR_H

