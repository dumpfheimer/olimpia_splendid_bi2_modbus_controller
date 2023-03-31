#include "wifi.h"

MDNSResponder mdns;
unsigned long lastNonShitRSS = 0;
unsigned long wifiMgrTolerateBadRSSms = 60000;
unsigned long wifiMgrWaitForConnectMs = 10000;
int8_t badRSS = -60;
const char* wifiMgrSSID;
const char* wifiMgrPW;
const char* wifiMgrHN;

boolean waitForWifi(unsigned long timeout) {
    unsigned long waitForConnectStart = millis();
    while (!WiFi.isConnected() && (millis() - waitForConnectStart) < timeout) {
        delay(50);
    }
    return WiFi.isConnected();
}

void connectToWifi(String SSID, String password) {
    WiFi.scanDelete();

    int n = WiFi.scanNetworks(false, true);

    String ssid;
    uint8_t encryptionType;
    int32_t RSSI;
    uint8_t* BSSID;
    int32_t channel;
    bool isHidden;

    uint8_t* bestBSSID = nullptr;
    int32_t bestRSSI = -999;
    int32_t bestChannel = 0;

    for (int i = 0; i < n; i++)
    {
        WiFi.getNetworkInfo(i, ssid, encryptionType, RSSI, BSSID, channel, isHidden);
        if (ssid.equals(SSID) && RSSI > bestRSSI) {
            bestRSSI = RSSI;
            bestBSSID = BSSID;
            bestChannel = channel;
        }
    }

    if (bestBSSID != nullptr) {
        WiFi.begin(SSID, password, bestChannel, bestBSSID);
        if (!waitForWifi(wifiMgrWaitForConnectMs)) {
            WiFi.begin(SSID, password);
            waitForWifi(wifiMgrWaitForConnectMs);
        }
    } else {
        WiFi.begin(SSID, password);
        waitForWifi(wifiMgrWaitForConnectMs);
    }
    if (WiFi.isConnected()) {
        mdns.begin(wifiMgrHN, WiFi.localIP());
    }
}

void setupWifi(const char* SSID, const char* password, const char* hostname, unsigned long tolerateBadRSSms, unsigned long waitForConnectMs) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setAutoReconnect(false);
    ESP8266WiFiClass::persistent(false);

    wifiMgrSSID = SSID;
    wifiMgrPW = password;
    wifiMgrHN = hostname;
    wifiMgrTolerateBadRSSms = tolerateBadRSSms;
    wifiMgrWaitForConnectMs = waitForConnectMs;
}

void loopWifi() {
    if (!WiFi.isConnected()) {
        connectToWifi(wifiMgrSSID, wifiMgrPW);
    } else {
        int8_t rss = WiFi.RSSI();
        if (rss < badRSS) {
            if ((millis() - lastNonShitRSS) > wifiMgrTolerateBadRSSms) {
                connectToWifi(wifiMgrSSID, wifiMgrPW);
            }
        } else {
            lastNonShitRSS = millis();
        }
    }
}
