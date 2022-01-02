#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define MODBUS_SERIAL modbusSerial
#define DEBUG_SERIAL Serial

#define DRIVER_ENABLE_PIN D3
#define READ_ENABLE_PIN   D2

void debugPrint(String s);
void debugPrint(unsigned long l, int i);
void debugPrintln(String s);
void debugPrintln(unsigned long l, int i);

bool noSwing = false;
ESP8266WebServer server(80);

#include "modbus.h"
#include "fancoil.h"


const char* kSsid = "XXX";
const char* kPassword = "XXX";
const char* wifiHost = "FancoilCtrl";

// instantiate ModbusMaster object
SoftwareSerial modbusSerial(D4, D1, true);


void setup()
{
  pinMode(READ_ENABLE_PIN, OUTPUT);
  pinMode(DRIVER_ENABLE_PIN, OUTPUT);

  digitalWrite(READ_ENABLE_PIN, 1);
  digitalWrite(DRIVER_ENABLE_PIN, 0);

  setupLogging();

  IncomingMessage m;
  
  modbusSerial.begin(9600, SWSERIAL_7E1);
  modbusSerial.setTimeout(500);
  

  debugPrintln("Connecting to WiFi..");
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(kSsid, kPassword);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  if (!MDNS.begin(wifiHost)) {
    debugPrintln("Error setting up MDNS responder!");
  }
  debugPrintln(WiFi.localIP().toString());

  setupOTA();
  setupHttp();

  MODBUS_SERIAL.setTimeout(5000);

  setupFancoilManager();

}

// reading 601-602
// 0258 = HEX 600
//            :  0  1  0  3  0  2  5  8  0  0  0  2  A  0  \r \n
// Request:   3A 30 31 30 33 30 32 35 38 30 30 30 32 41 30 0D 0A
// Response:  3A 30 31 30 33 30 34 30 33 45 38 31 33 38 38 37 32 0D 0A
void loop() {
  server.handleClient();
  loopOTA();

  loopFancoils(&MODBUS_SERIAL);
}
