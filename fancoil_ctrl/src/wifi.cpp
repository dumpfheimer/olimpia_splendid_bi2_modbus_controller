#include "wifi.h"

MDNSResponder wifiMgrMdns;
unsigned long wifiMgrLastNonShitRSS = 0;
unsigned long wifiMgrlastConnected = 0;
unsigned long wifiMgrNotifyNoWifiTimeout = 600 * 1000; // 10m
unsigned long wifiMgrTolerateBadRSSms = 300 * 1000; // 5m
unsigned long wifiMgrRescanInterval = 3600 * 1000; // 1h
unsigned long wifiMgrLastScan = 0;
unsigned long wifiMgrWaitForConnectMs = 15000; // 15s
unsigned long wifiMgrWaitForScanMs = 15000; // 15s
unsigned long wifiMgrScanCount = 0;
unsigned long wifiMgrConnectCount = 0;

int8_t badRSS = -70;
const char* wifiMgrSSID;
const char* wifiMgrPW;
const char* wifiMgrHN;

void (*loopFunctionPointer)(void) = nullptr;
void (*wifiMgrNotifyNoWifiCallback)(void) = nullptr;

ESP8266WebServer *wifiMgrServer;

boolean waitForWifi(unsigned long timeout) {
    unsigned long waitForConnectStart = millis();
    while (!WiFi.isConnected() && (millis() - waitForConnectStart) < timeout) {
        if (loopFunctionPointer != nullptr) loopFunctionPointer();
        delay(10);
    }
    return WiFi.isConnected();
}

void connectToWifi() {
    if (wifiMgrMdns.isRunning()) wifiMgrMdns.end();
    //if (wifiMgrServer != nullptr) wifiMgrServer->stop();
    WiFi.scanDelete();
    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);

    wifiMgrScanCount++;

    int n = WiFi.scanNetworks(true, false);

    unsigned long waitForScanStart = millis();

    while (WiFi.scanComplete() == -1 && (millis() - waitForScanStart) < wifiMgrWaitForScanMs) {
        if (loopFunctionPointer != nullptr) loopFunctionPointer();
        delay(10);
    }
    n = WiFi.scanComplete();

    if (n != -1) {
        String ssid;
        uint8_t encryptionType;
        int32_t RSSI;
        uint8_t *BSSID;
        int32_t channel;
        bool isHidden;

        uint8_t *bestBSSID = nullptr;
        int32_t bestRSSI = -999;
        int32_t bestChannel = 0;

        for (int i = 0; i < n; i++) {
            WiFi.getNetworkInfo(i, ssid, encryptionType, RSSI, BSSID, channel, isHidden);
            if (ssid.equals(wifiMgrSSID) && RSSI > bestRSSI) {
                bestRSSI = RSSI;
                bestBSSID = BSSID;
                bestChannel = channel;
            }
        }

        wifiMgrLastScan = millis();

        if (bestBSSID != nullptr) {
            WiFi.begin(wifiMgrSSID, wifiMgrPW, bestChannel, bestBSSID);
            wifiMgrConnectCount++;
            if (!waitForWifi(wifiMgrWaitForConnectMs)) {
                WiFi.disconnect();
                WiFi.mode(WIFI_OFF);
                WiFi.mode(WIFI_STA);

                WiFi.begin(wifiMgrSSID, wifiMgrPW);
                wifiMgrConnectCount++;
                waitForWifi(wifiMgrWaitForConnectMs);
            }
        } else {
            WiFi.begin(wifiMgrSSID, wifiMgrPW);
            wifiMgrConnectCount++;
            waitForWifi(wifiMgrWaitForConnectMs);
        }
        if (WiFi.isConnected() && wifiMgrHN != nullptr) {
            if (wifiMgrMdns.isRunning()) wifiMgrMdns.end();
            wifiMgrMdns.begin(wifiMgrHN, WiFi.localIP());
        }
        if (WiFi.isConnected()) {
            wifiMgrLastNonShitRSS = millis();
            //if (wifiMgrServer != nullptr) wifiMgrServer->begin();
        }
    }
}

void setupWifi(const char* SSID, const char* password) {
    setupWifi(SSID, password, nullptr);
}

void setupWifi(const char* SSID, const char* password, const char* hostname) {
    setupWifi(SSID, password, hostname, wifiMgrTolerateBadRSSms, wifiMgrWaitForConnectMs);
}

void setupWifi(const char* SSID, const char* password, const char* hostname, unsigned long tolerateBadRSSms, unsigned long waitForConnectMs) {
    setupWifi(SSID, password, hostname, tolerateBadRSSms, waitForConnectMs, wifiMgrRescanInterval);
}

#ifdef ElegantOTA_h
void onOTAEnd(bool success) {
  if (success) {
    ESP.restart();
  } else {
    Serial.println("There was an error during OTA update!");
  }
}
#endif

void setupWifi(const char* SSID, const char* password, const char* hostname, unsigned long tolerateBadRSSms, unsigned long waitForConnectMs, unsigned long rescanInterval) {
    WiFi.mode(WIFI_STA);
    if (hostname != nullptr) WiFi.hostname(hostname);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    ESP8266WiFiClass::persistent(false);

    wifiMgrSSID = SSID;
    wifiMgrPW = password;
    wifiMgrHN = hostname;
    wifiMgrTolerateBadRSSms = tolerateBadRSSms;
    wifiMgrWaitForConnectMs = waitForConnectMs;
    wifiMgrRescanInterval = rescanInterval;

    connectToWifi();


#ifdef ElegantOTA_h
    ElegantOTA.onEnd(onOTAEnd);
#endif
}

void loopWifi() {
    if (!WiFi.isConnected()) {
        connectToWifi();
        if (wifiMgrNotifyNoWifiCallback != nullptr && millis() - wifiMgrlastConnected > wifiMgrNotifyNoWifiTimeout) {
            wifiMgrNotifyNoWifiCallback();
        }
    } else {
        if (millis() - wifiMgrlastConnected > 1000) {
            wifiMgrlastConnected = millis();

            int8_t rss = WiFi.RSSI();

            if (rss < badRSS) {
                if ((millis() - wifiMgrLastNonShitRSS) > wifiMgrTolerateBadRSSms) {
                    connectToWifi();
                }
            } else {
                wifiMgrLastNonShitRSS = millis();
            }

            if (wifiMgrRescanInterval > 0 && (millis() - wifiMgrLastScan) > wifiMgrRescanInterval) {
                connectToWifi();
            }
        }
    }
}

void sendRSSI() {
    wifiMgrServer->send(200, "text/plain", String(WiFi.RSSI()));
}

void isConnected() {
    wifiMgrServer->send(200, "text/plain", String(WiFi.isConnected()));
}

void ssid() {
    wifiMgrServer->send(200, "text/plain", WiFi.SSID());
}

void bssid() {
    wifiMgrServer->send(200, "text/plain", WiFi.BSSIDstr());
}

void status() {
    String s = "ssid: " + WiFi.SSID() + "\nconnected: " + String(WiFi.isConnected()) + "\nbssid: " + WiFi.BSSIDstr() + "\nrssi: " + String(WiFi.RSSI()) + "\nuptime: " + String(millis()/1000) + "s";
    wifiMgrServer->send(200, "text/plain", s);
}

void restart() {
    wifiMgrServer->send(200, "text/plain", "restarting");
    delay(500);
    ESP.restart();
}

void wifiMgrExpose(ESP8266WebServer *wifiMgrServer_) {
    wifiMgrServer = wifiMgrServer_;
    wifiMgrServer->on("/wifiMgr/rssi", sendRSSI);
    wifiMgrServer->on("/wifiMgr/isConnected", isConnected);
    wifiMgrServer->on("/wifiMgr/ssid", ssid);
    wifiMgrServer->on("/wifiMgr/bssid", bssid);
    wifiMgrServer->on("/wifiMgr/status", status);
    wifiMgrServer->on("/wifiMgr/restart", restart);
}

void wifiMgrSetBadRSSI(int8_t rssi) {
    badRSS = rssi;
}

void wifiMgrNotifyNoWifi(void (*wifiMgrNotifyNoWifiCallbackArg)(void), unsigned long timeout) {
    wifiMgrNotifyNoWifiCallback = wifiMgrNotifyNoWifiCallbackArg;
    wifiMgrNotifyNoWifiTimeout = timeout;
}

void setLoopFunction(void (*loopFunctionPointerArg)(void)) {
    loopFunctionPointer = loopFunctionPointerArg;
}
