#include "httpHandlers.h"

// case-insensitive: an on_off command with an unrecognized payload fails
// toward OFF, so being strict about capitalization ("On", "TRUE", "YES"
// used to be rejected) silently powered units down
bool isTrue(String str) {
    str.toLowerCase();
    return str == "true" ||
           str == "yes" ||
           str == "1" ||
           str == "on";
}

// String(x, BIN) drops leading zeros, which makes register bytes ambiguous to
// read (bit 6 masquerades as bit 7); always show all 8 bits
String toBin8(uint8_t b) {
    String s = String(b, BIN);
    while (s.length() < 8) s = "0" + s;
    return s;
}

uint8_t getAddress() {
    for (uint8_t i = 0; i < server.args(); i++) {
        if (server.argName(i) == "addr") return strtoul(server.arg(i).c_str(), nullptr, 10);
        if (server.argName(i) == "address") return strtoul(server.arg(i).c_str(), nullptr, 10);
    }
    return 0;
}

void handleScript() {
    if (LittleFS.exists("/scripts.js")) {
        File file = LittleFS.open("/scripts.js", "r");
        server.streamFile(file, "application/javascript");
        file.close();
    } else {
        server.send(404, "text/plain", "File not found");
    }
}

void handleStyles() {
    if (LittleFS.exists("/styles.css")) {
        File file = LittleFS.open("/styles.css", "r");
        server.streamFile(file, "text/css");
        file.close();
    } else {
        server.send(404, "text/plain", "File not found");
    }
}

// streams page fragments to the client without ever holding the page (or a
// section of it) in a heap String: building the full page used to peak at
// ~2x the page size of contiguous heap, which intermittently failed and left
// too little heap for the following asset requests
// (ERR_CONTENT_LENGTH_MISMATCH on scripts.js). Fragments are batched in a
// small fixed buffer and flushed as one chunk when it fills, so the TCP
// stream still gets well-filled packets. Call sendPartialFlush() at the end
// of the handler, then sendContent("") to terminate the chunked response.
static char partialBuf[1064];
static size_t partialLen = 0;

void sendPartialFlush() {
    if (partialLen > 0) {
        server.sendContent(partialBuf, partialLen);
        partialLen = 0;
    }
}

void sendPartial(const char *s) {
    size_t l = strlen(s);
    if (partialLen + l > sizeof(partialBuf)) sendPartialFlush();
    if (l >= sizeof(partialBuf)) {
        // fragment larger than the buffer goes out directly
        server.sendContent(s, l);
        return;
    }
    memcpy(partialBuf + partialLen, s, l);
    partialLen += l;
}

void sendPartial(const String &s) {
    size_t l = s.length();
    if (partialLen + l > sizeof(partialBuf)) sendPartialFlush();
    if (l >= sizeof(partialBuf)) {
        server.sendContent(s.c_str(), l);
        return;
    }
    memcpy(partialBuf + partialLen, s.c_str(), l);
    partialLen += l;
}

// starts a chunked streaming response; append with sendPartial(), terminate
// with sendPartialEnd()
void sendPartialBegin(int code, const char *contentType) {
    partialLen = 0;
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(code, contentType, "");
}

void sendPartialEnd() {
    sendPartialFlush();
    // zero-length chunk terminates the chunked response
    server.sendContent("");
}

void handleRoot() {
    sendPartialBegin(200, "text/html");
    
    // HTML header
    sendPartial("<!DOCTYPE html>");
    sendPartial("<html lang=\"en\">");
    sendPartial("<head>");
    sendPartial("    <meta charset=\"UTF-8\">");
    sendPartial("    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
    sendPartial("    <title>Fancoil Controller</title>");
    sendPartial("    <link rel=\"stylesheet\" href=\"/styles.css\">");
    sendPartial("</head>");
    sendPartial("<body>");
    
    // Header
    sendPartial("    <div class=\"header\">");
    sendPartial("        <div class=\"container header-content\">");
    sendPartial("            <h1>Fancoil Controller</h1>");
    sendPartial("            <div>");
    sendPartial("                <a href=\"https://github.com/dumpfheimer/olimpia_splendid_bi2_modbus_controller\" class=\"btn\">GitHub</a>");
    sendPartial("                <a href=\"/wifiMgr/configure\" class=\"btn\">WiFi & MQTT</a>");
    sendPartial("            </div>");
    sendPartial("        </div>");
    sendPartial("    </div>");
    

    // Navigation
    sendPartial("    <div class=\"nav\">");
    sendPartial("        <div class=\"container\">");
    sendPartial("            <ul class=\"nav-list\">");
    sendPartial("                <li class=\"nav-item\"><button class=\"nav-link tab-link active\" data-tab=\"dashboard\">Dashboard</button></li>");
    sendPartial("                <li class=\"nav-item\"><button class=\"nav-link tab-link\" data-tab=\"settings\">Settings</button></li>");
    sendPartial("                <li class=\"nav-item\"><button class=\"nav-link tab-link\" data-tab=\"statistics\">Statistics</button></li>");
    sendPartial("                <li class=\"nav-item\"><button class=\"nav-link tab-link\" data-tab=\"debug\">Debug</button></li>");
    sendPartial("            </ul>");
    sendPartial("        </div>");
    sendPartial("    </div>");
    
    // Main container
    sendPartial("    <div class=\"container mt-2\">");
    
    // Messages
    sendPartial("        <div id=\"error-message\" style=\"display: none;\" class=\"card\">");
    sendPartial("            <div class=\"card-content\" style=\"background-color: var(--error-color); color: white;\"></div>");
    sendPartial("        </div>");
    sendPartial("        <div id=\"success-message\" style=\"display: none;\" class=\"card\">");
    sendPartial("            <div class=\"card-content\" style=\"background-color: var(--success-color); color: white;\"></div>");
    sendPartial("        </div>");
    
    // Dashboard Tab
    sendPartial("        <div id=\"dashboard-content\" class=\"tab-content active\">");
    sendPartial("            <div class=\"loading\"></div>");
    sendPartial("        </div>");
    

    // Settings Tab
    sendPartial("        <div id=\"settings-content\" class=\"tab-content\">");
    
    // Register Fancoil
    sendPartial("            <div class=\"card mb-2\">");
    sendPartial("                <div class=\"card-header\">Register Fancoil</div>");
    sendPartial("                <div class=\"card-content\">");
    sendPartial("                    <div class=\"form-group\">");
    sendPartial("                        <label class=\"form-label\">Fancoil Address (1-32)</label>");
    sendPartial("                        <input type=\"number\" min=\"1\" max=\"32\" id=\"register-address\" class=\"form-control\">");
    sendPartial("                    </div>");
    sendPartial("                    <button class=\"btn\" onclick=\"registerFancoil()\">Register</button>");
    sendPartial("                </div>");
    sendPartial("            </div>");
    

    // Unregister Fancoil
    sendPartial("            <div class=\"card mb-2\">");
    sendPartial("                <div class=\"card-header\">Unregister Fancoil</div>");
    sendPartial("                <div class=\"card-content\">");
    sendPartial("                    <div class=\"form-group\">");
    sendPartial("                        <label class=\"form-label\">Fancoil Address (1-32)</label>");
    sendPartial("                        <input type=\"number\" min=\"1\" max=\"32\" id=\"unregister-address\" class=\"form-control\">");
    sendPartial("                    </div>");
    sendPartial("                    <button class=\"btn\" onclick=\"unregisterFancoil()\">Unregister</button>");
    sendPartial("                </div>");
    sendPartial("            </div>");
    

    // Change Address
    sendPartial("            <div class=\"card mb-2\">");
    sendPartial("                <div class=\"card-header\">Change Fancoil Address</div>");
    sendPartial("                <div class=\"card-content\">");
    // options are populated client-side by scripts.js (populateAddressSelects):
    // generating 64 <option> elements here pushed the page String past what
    // the heap can reliably serve
    sendPartial("                    <div class=\"form-group\">");
    sendPartial("                        <label class=\"form-label\">Source Address</label>");
    sendPartial("                        <select id=\"source-address\" class=\"form-select\">");
    sendPartial("                            <option value=\"\" selected disabled>select...</option>");
    sendPartial("                        </select>");
    sendPartial("                    </div>");
    sendPartial("                    <div class=\"form-group\">");
    sendPartial("                        <label class=\"form-label\">Target Address</label>");
    sendPartial("                        <select id=\"target-address\" class=\"form-select\">");
    sendPartial("                            <option value=\"\" selected disabled>select...</option>");
    sendPartial("                        </select>");
    sendPartial("                    </div>");
    sendPartial("                    <button class=\"btn\" onclick=\"changeFancoilAddress()\">Change Address</button>");
    sendPartial("                </div>");
    sendPartial("            </div>");
    sendPartial("");

    // Remote Control (rE)
    sendPartial("            <div class=\"card mb-2\">");
    sendPartial("                <div class=\"card-header\">Remote Control (rE)</div>");
    sendPartial("                <div class=\"card-content\">");
    sendPartial("                    <p class=\"mb-1\">Sets the unit's remote-enable flag (register 224 bit 2). A unit without it runs its own thermostat and ignores this controller.</p>");
    sendPartial("                    <div class=\"form-group\">");
    sendPartial("                        <label class=\"form-label\">Fancoil Address</label>");
    sendPartial("                        <select id=\"remote-address\" class=\"form-select\">");
    sendPartial("                            <option value=\"\" selected disabled>select...</option>");
    sendPartial("                        </select>");
    sendPartial("                    </div>");
    sendPartial("                    <div class=\"btn-group\">");
    sendPartial("                        <button class=\"btn\" onclick=\"setRemoteEnable(true)\">Enable Remote</button>");
    sendPartial("                        <button class=\"btn btn-warning\" onclick=\"setRemoteEnable(false)\">Disable Remote</button>");
    sendPartial("                    </div>");
    sendPartial("                </div>");
    sendPartial("            </div>");
    

    // Refresh Rate
    sendPartial("            <div class=\"card mb-2\">");
    sendPartial("                <div class=\"card-header\">Auto Refresh</div>");
    sendPartial("                <div class=\"card-content\">");
    sendPartial("                    <div class=\"form-group\">");
    sendPartial("                        <label class=\"form-label\">Refresh Rate</label>");
    sendPartial("                        <select id=\"refresh-rate\" class=\"form-select\">");
    sendPartial("                            <option value=\"5000\">5 seconds</option>");
    sendPartial("                            <option value=\"10000\" selected>10 seconds</option>");
    sendPartial("                            <option value=\"30000\">30 seconds</option>");
    sendPartial("                            <option value=\"60000\">1 minute</option>");
    sendPartial("                        </select>");
    sendPartial("                    </div>");
    sendPartial("                </div>");
    sendPartial("            </div>");
    
    // Factory Reset
    sendPartial("            <div class=\"card\">");
    sendPartial("                <div class=\"card-header\">Factory Reset</div>");
    sendPartial("                <div class=\"card-content\">");
    sendPartial("                    <p class=\"mb-1\">This will remove all registered fancoils.</p>");
    sendPartial("                    <button class=\"btn btn-danger\" onclick=\"factoryReset()\">Factory Reset</button>");
    sendPartial("                </div>");
    sendPartial("            </div>");
    sendPartial("        </div>");
    

    // Statistics Tab
    sendPartial("        <div id=\"statistics-content\" class=\"tab-content\">");
    sendPartial("            <div class=\"loading\"></div>");
    sendPartial("        </div>");
    
    // Debug Tab
    sendPartial("        <div id=\"debug-content\" class=\"tab-content\">");
    sendPartial("            <div class=\"card\">");
    sendPartial("                <div class=\"card-header\">Debug</div>");
    sendPartial("                <div class=\"card-content\">");
    sendPartial("                    <div class=\"form-group\">");
    sendPartial("                        <label class=\"form-label\">Fancoil Address</label>");
    sendPartial("                        <input type=\"number\" min=\"0\" max=\"32\" id=\"debugAddress\" class=\"form-control\">");
    sendPartial("                    </div>");
    sendPartial("                    <div class=\"btn-group\">");
    sendPartial("                        <button class=\"btn\" onclick=\"debug()\">Debug</button>");
    sendPartial("                        <button class=\"btn\" onclick=\"debug(quickDebugRegs)\">Quick Debug</button>");
    sendPartial("                    </div>");
    sendPartial("                    <div id=\"debugOut\" class=\"mt-2\"></div>");
    sendPartial("                </div>");
    sendPartial("            </div>");
    sendPartial("        </div>");
    sendPartial("    </div>");
    
    // Footer
    sendPartial("    <script src=\"/scripts.js\"></script>");
    sendPartial("</body>");
    sendPartial("</html>");

    sendPartialEnd();
}

void handleUptime() {
    String ret = String(millis() / 1000);
#if defined(ESP8266)
    ret += "\nreset reason: " + ESP.getResetReason();
    ret += "\nreset info: " + ESP.getResetInfo();
    ret += "\nfree heap: " + String(ESP.getFreeHeap());
    ret += "\nheap low-water mark: " + String(heapLowWaterMark);
    ret += "\nmax free block: " + String(ESP.getMaxFreeBlockSize());
    ret += "\nheap fragmentation: " + String(ESP.getHeapFragmentation());
#elif defined(ESP32)
    ret += "\nreset reason: " + String(esp_reset_reason());
    ret += "\nfree heap: " + String(ESP.getFreeHeap());
    ret += "\nheap low-water mark: " + String(heapLowWaterMark);
    ret += "\nmax free block: " + String(ESP.getMaxAllocHeap());
#endif
    server.send(200, "text/html", ret);
}

void handleGet() {
    uint8_t addr = getAddress();

    if (!(addr > 0 && addr <= 32)) {
        server.send(500, "text/plain", "address must be between 1 and 32");
        return;
    }

    Fancoil *fancoil = getFancoilByAddress(addr);

    if (fancoil == nullptr) {
        server.send(404, "text/plain", "address not registered");
    } else {
        sendPartialBegin(200, "application/json");
        sendPartial("{");
        // decimal! this used to print HEX (address 17 displayed as "11"),
        // which repeatedly confused address diagnostics
        sendPartial("\"address\": " + String(fancoil->getAddress()) + ",");
        sendPartial("\"setpoint\": " + String(fancoil->getSetpoint()) + ",");
        sendPartial("\"ambient\": " + String(fancoil->getAmbient()) + ",");

        if (fancoil->hasValidDesiredState) {
            sendPartial("\"hasValidDesiredState\": true, ");
        } else {
            sendPartial("\"hasValidDesiredState\": false, ");
        }

        if (fancoil->wantsToRead()) {
            sendPartial("\"wantsToRead\": true, ");
        } else {
            sendPartial("\"wantsToRead\": false, ");
        }

        if (fancoil->wantsToWrite()) {
            sendPartial("\"wantsToWrite\": true, ");
        } else {
            sendPartial("\"wantsToWrite\": false, ");
        }

        if (fancoil->isOn()) {
            sendPartial("\"on\": true, ");
        } else {
            sendPartial("\"on\": false, ");
        }

        switch (fancoil->getSpeed()) {
            case FanSpeed::MAX:
                sendPartial("\"speed\": \"MAX\", ");
                break;
            case FanSpeed::NIGHT:
                sendPartial("\"speed\": \"NIGHT\", ");
                break;
            case FanSpeed::MIN:
                sendPartial("\"speed\": \"MIN\", ");
                break;
            case FanSpeed::AUTOMATIC:
                sendPartial("\"speed\": \"AUTOMATIC\", ");
                break;
        }

        if (fancoil->getMode() == Mode::FAN_ONLY) {
            sendPartial("\"mode\": \"FAN_ONLY\", ");
        } else if (fancoil->getMode() == Mode::COOLING) {
            sendPartial("\"mode\": \"COOLING\", ");
        } else if (fancoil->getMode() == Mode::HEATING) {
            sendPartial("\"mode\": \"HEATING\", ");
        } else {
            sendPartial("\"mode\": \"AUTO\", ");
        }

        if (fancoil->ambientTemperatureIsValid()) {
            sendPartial("\"ambientTemperatureIsValid\": true, ");
        } else {
            sendPartial("\"ambientTemperatureIsValid\": false, ");
        }

        if (fancoil->readTimeout()) {
            sendPartial("\"readTimeout\": true, ");
        } else {
            sendPartial("\"readTimeout\": false, ");
        }

        if (fancoil->isCollisionSuspected()) {
            sendPartial("\"collisionSuspected\": true, ");
        } else {
            sendPartial("\"collisionSuspected\": false, ");
        }
        sendPartial("\"collisionSuspicionCount\": " + String(fancoil->getCollisionSuspicionCount()) + ", ");
        sendPartial("\"consecutiveFailures\": " + String(fancoil->getConsecutiveFailures()) + ", ");

        if (fancoil->isLocalModeDetected()) {
            sendPartial("\"localMode\": true, ");
        } else {
            sendPartial("\"localMode\": false, ");
        }
        sendPartial("\"localModeRepairs\": " + String(fancoil->getLocalModeRepairs()) + ", ");

        if (fancoil->isSwingOn()) {
            sendPartial("\"swing\": true, ");
        } else {
            sendPartial("\"swing\": false, ");
        }

        if (fancoil->ev1On()) {
            sendPartial("\"ev1\": true, ");
        } else {
            sendPartial("\"ev1\": false, ");
        }

        if (fancoil->ev2On()) {
            sendPartial("\"ev2\": true, ");
        } else {
            sendPartial("\"ev2\": false, ");
        }

        if (fancoil->boilerOn()) {
            sendPartial("\"boiler\": true, ");
        } else {
            sendPartial("\"boiler\": false, ");
        }

        if (fancoil->chillerOn()) {
            sendPartial("\"chiller\": true, ");
        } else {
            sendPartial("\"chiller\": false, ");
        }

        if (fancoil->hasWaterFault()) {
            sendPartial("\"waterFault\": true, ");
        } else {
            sendPartial("\"waterFault\": false, ");
        }

#ifdef LOAD_WATER_TEMP
        sendPartial("\"waterTemp\": " + String(fancoil->getWaterTemp()) + ", ");
#endif

#ifdef LOAD_AMBIENT_TEMP
        sendPartial("\"ambientTemp\": " + String(fancoil->getAmbientTemp()) + ", ");
#endif

        switch (fancoil->getSyncState()) {
            case SyncState::HAPPY:
                sendPartial("\"syncState\": \"HAPPY\",");
                break;
            case SyncState::WRITING:
                sendPartial("\"syncState\": \"WRITING\",");
                break;
            default:
                sendPartial("\"syncState\": \"INVALID\",");
                break;
        }
	sendPartial("\"data\":[");
	sendPartial("\"");
	sendPartial(toBin8(fancoil->getData1()));
	sendPartial("\",");
	sendPartial("\"");
	sendPartial(toBin8(fancoil->getData2()));
	sendPartial("\"");
	sendPartial("],");
	sendPartial("\"recData\":[");
	sendPartial("\"");
	sendPartial(toBin8(fancoil->getRecData1()));
	sendPartial("\",");
	sendPartial("\"");
	sendPartial(toBin8(fancoil->getRecData2()));
	sendPartial("\"");
	sendPartial("]");

        sendPartial("}");
        sendPartialEnd();
    }
}

void handleRead() {
    uint8_t addr = getAddress();

    if (!(addr > 0 && addr <= 32)) {
        server.send(500, "text/plain", "address must be between 1 and 32");
        return;
    }

    //Fancoil *fancoil = getFancoilByAddress(addr);

    if (modbusBusy) {
        server.send(503, "text/plain", "modbus busy, retry");
        return;
    }

    if (!server.hasArg("reg")) {
        server.send(500, "text/plain", "reg must be specified");
        return;
    }

    uint16_t reg = server.arg("reg").toDouble();
    uint16_t len = 1;
    if (server.hasArg("len")) {
        uint16_t len = server.arg("len").toDouble();
    }

    IncomingMessage *i = modbusReadRegister(&MODBUS_SERIAL, addr, reg, len);

    if (!i->valid) {
        server.send(500, "text/plain", "invalid or no response");
    } else if (i->isError) {
        server.send(500, "text/plain", "fan coil returned error");
    } else {
        server.send(200, "text/plain",
                    String(i->data[1], HEX) + " " + String(i->data[2], HEX) + " bin: " + String(i->data[1], BIN) + " " +
                    String(i->data[2], BIN) + " dec: " + String((i->data[1] << 8) | i->data[2], DEC));
    }
}

void handleWrite() {
    uint8_t addr = getAddress();

    if (!(addr > 0 && addr <= 32)) {
        server.send(500, "text/plain", "address must be between 1 and 32");
        return;
    }

    //Fancoil *fancoil = getFancoilByAddress(addr);

    if (modbusBusy) {
        server.send(503, "text/plain", "modbus busy, retry");
        return;
    }

    uint16_t reg = server.arg("reg").toDouble();
    uint16_t val = server.arg("val").toDouble();

    IncomingMessage *i = modbusWriteRegister(&MODBUS_SERIAL, addr, reg, val);

    if (!i->valid) {
        server.send(500, "text/plain", "invalid or no response");
    } else if (i->isError) {
        server.send(500, "text/plain", "fan coil returned error");
    } else {
        server.send(200, "text/plain", "ok");
    }
}

void handleResetWaterTemperatureFault() {
    uint8_t addr = getAddress();

    if (!(addr > 0 && addr <= 32)) {
        server.send(500, "text/plain", "address must be between 1 and 32");
        return;
    }

    Fancoil *fancoil = getFancoilByAddress(addr);

    if (fancoil == nullptr) {
        server.send(404, "text/plain", "address not registered");
        return;
    }

    if (modbusBusy) {
        server.send(503, "text/plain", "modbus busy, retry");
        return;
    }

    if (fancoil->resetWaterTemperatureFault(&MODBUS_SERIAL)) {
        server.send(200, "text/plain", "ok");
    } else {
        server.send(500, "text/plain", "failed");
    }
}

void handleRegister() {
    // this handler frees and rebuilds the fancoil list; when reached
    // re-entrantly (web server serviced from the modbus yield callback), the
    // suspended Fancoil::loop() above us would resume on freed memory
    if (modbusBusy) {
        server.send(503, "text/plain", "modbus busy, retry");
        return;
    }
    uint8_t addr = getAddress();

    if (!(addr > 0 && addr <= 32)) {
        server.send(500, "text/plain", "address must be between 1 and 32");
        return;
    }

    Fancoil *fancoil = getFancoilByAddress(addr);

    if (fancoil != nullptr) {
        server.send(304, "text/plain", "already registered");
        return;
    }

    if (registerFancoil(addr)) {
        // the new unit needs its command-topic subscriptions and discovery
        // configs; handled by the next loopMqtt pass
        mqttRequestDiscovery();
        server.send(200, "text/plain", "ok");
    } else {
        server.send(500, "text/plain", "register failed");
    }
}

void handleFactoryReset() {
    // frees the fancoil list - must not run re-entrantly, see handleRegister
    if (modbusBusy) {
        server.send(503, "text/plain", "modbus busy, retry");
        return;
    }
    EEPROM.write(FANCOIL_EEPROM_START_ADDRESS, 0);
    loadFancoils();
    server.send(200, "text/plain", "ok");
}

void handleUnregister() {
    // frees the fancoil list - must not run re-entrantly, see handleRegister
    if (modbusBusy) {
        server.send(503, "text/plain", "modbus busy, retry");
        return;
    }
    uint8_t addr = getAddress();

    Fancoil *fancoil = getFancoilByAddress(addr);

    if (fancoil == nullptr) {
        server.send(500, "text/plain", "address not registered");
        return;
    }

    if (unregisterFancoil(addr)) {
        server.send(200, "text/plain", "ok");
    } else {
        server.send(500, "text/plain", "unregister failed");
    }
    unconfigureHomeAssistantDevice(String(addr), false);
}

void handleList() {
    LinkedFancoilListElement *fancoilLinkedList = getFirstFancoilListElement();

    debugPrintln("creating list");
    String ret = "[";

    uint8_t walked = 0;
    while (fancoilLinkedList != nullptr && fancoilLinkedList->fancoil != nullptr && ++walked <= MAX_FANCOIL_LIST_WALK) {
        ret += String(fancoilLinkedList->fancoil->getAddress());
        if (fancoilLinkedList->next != nullptr) ret += ",";
        fancoilLinkedList = fancoilLinkedList->next;
    }
    debugPrintln("creating list finished");

    ret += "]";

    server.send(200, "application/json", ret);
}

String getValue() {
    if (server.hasArg("val")) return server.arg("val");
    if (server.hasArg("value")) return server.arg("value");

    return String("");
}

uint16_t getLength() {
    if (server.hasArg("len")) return strtoul(server.arg("len").c_str(), nullptr, 10);
    if (server.hasArg("length")) return strtoul(server.arg("length").c_str(), nullptr, 10);

    return 0;
}

double getTemperature() {
    if (server.hasArg("temp")) return server.arg("temp").toDouble();
    if (server.hasArg("temperature")) return server.arg("temperature").toDouble();

    return 0;
}


void handleSet() {
    uint8_t addr = getAddress();

    Fancoil *fancoil = getFancoilByAddress(addr);
    if (fancoil == nullptr) {
        server.send(404, "application/json", "{\"error\": \"address not registered\"}");
        return;
    }

    //fancoil->readState(&MODBUS_SERIAL);
    /*if (fancoil->lastReadChangedValues) {
      server.send(500, "application/json", "{\"error\": \"out of sync\"}");
      return;
    }*/

    if (server.hasArg("on")) {
        fancoil->setOn(isTrue(server.arg("on")));
    }

    if (server.hasArg("ambient")) {
        // range is enforced centrally in Fancoil::setAmbient (1-45)
        fancoil->setAmbient(server.arg("ambient").toDouble());
    }

    if (server.hasArg("setpoint")) {
        // range is enforced centrally in Fancoil::setSetpoint (15-40)
        fancoil->setSetpoint(server.arg("setpoint").toDouble());
    }

    if (server.hasArg("speed")) {
        String speed = server.arg("speed");

        if (speed == "MIN") {
            fancoil->setSpeed(FanSpeed::MIN);
        } else if (speed == "MAX") {
            fancoil->setSpeed(FanSpeed::MAX);
        } else if (speed == "NIGHT") {
            fancoil->setSpeed(FanSpeed::NIGHT);
        } else if (speed == "AUTOMATIC") {
            fancoil->setSpeed(FanSpeed::AUTOMATIC);
        } else {
            server.send(500, "application/json", "{\"error\": \"invalid fan speed provided\"}");
            return;
        }
    }

    if (server.hasArg("mode")) {
        String mode = server.arg("mode");

        if (mode == "FAN_ONLY") {
            fancoil->setMode(Mode::FAN_ONLY);
        } else if (mode == "COOLING" || mode == "COOL") {
            fancoil->setMode(Mode::COOLING);
        } else if (mode == "HEATING" || mode == "HEAT") {
            fancoil->setMode(Mode::HEATING);
        } else if (mode == "AUTO") {
            fancoil->setMode(Mode::AUTO);
        } else {
            server.send(500, "application/json", "{\"error\": \"invalid mode provided\"}");
            return;
        }
    }

    if (server.hasArg("swing")) {
        fancoil->setSwing(isTrue(server.arg("swing")));
    }
    fancoil->notifyHasValidState();

    if (fancoil->writeTo(&MODBUS_SERIAL)) {
        handleGet();
    } else if (modbusBusy) {
        // bounced because another transaction owns the bus right now; the
        // desired state was already stored above and the periodic loop will
        // flush it shortly, so this is not a real failure.
        server.send(503, "application/json", "{\"error\": \"modbus busy, will be applied shortly\"}");
    } else {
        server.send(500, "application/json", "{\"error\": \"write failed\"}");
    }
}

void handleChangeAddress() {
    bool hasSource = false;
    bool hasTarget = false;
    uint8_t sourceAddress = 0;
    uint8_t targetAddress = 0;

    for (uint8_t i = 0; i < server.args(); i++) {
        if (server.argName(i) == "sourceAddress") {
            sourceAddress = server.arg(i).toDouble();
            hasSource = true;
        }
        if (server.argName(i) == "targetAddress") {
            targetAddress = server.arg(i).toDouble();
            hasTarget = true;
        }
    }

    if (!hasSource) {
        server.send(500, "text/plain", "source address musst be passed ?sourceAddress=X");
        return;
    }
    if (!hasTarget) {
        server.send(500, "text/plain", "target address musst be passed ?targetAddress=X");
        return;
    }
    if (targetAddress < 1 || targetAddress > 32) {
        server.send(500, "text/plain", "target address musst be between 1 and 32");
        return;
    }
    if (sourceAddress > 32) {
        server.send(500, "text/plain", "source address musst be between 0 (broadcast) and 32");
        return;
    }
    if (sourceAddress == targetAddress) {
        server.send(500, "text/plain", "source and target address are identical");
        return;
    }

    if (modbusBusy) {
        server.send(503, "text/plain", "modbus busy, retry");
        return;
    }

    // refuse if something already answers on the target address - a second
    // unit on the same address answers every request in duplicate, which
    // corrupts frames and cannot be untangled in software afterwards
    if (modbusReadRegister(&MODBUS_SERIAL, targetAddress, 101)->success()) {
        server.send(409, "text/plain", "target address is already in use by a unit on the bus");
        return;
    }

    if (sourceAddress == 0) {
        // address 0 is the modbus broadcast address: EVERY listening unit
        // executes the write and none of them respond, so success cannot be
        // confirmed. Only safe with exactly one unit powered on the bus.
        modbusWriteRegister(&MODBUS_SERIAL, sourceAddress, 200, targetAddress);
        server.send(200, "text/plain",
                    "broadcast sent: every listening unit now has the target address (no confirmation possible - verify with a read)");
        return;
    }

    if (modbusWriteRegister(&MODBUS_SERIAL, sourceAddress, 200, targetAddress)->success()) {
        debugPrintln("address change write was successfull");
        server.send(200, "text/plain", "done");
    } else {
        server.send(500, "text/plain", "address changed seems to have failed");
    }
}

// register 224 bit 2 is the unit's remote-enable (rE) flag - field-PROVEN by
// induced failure and recovery (2026-08-29, unit 17: clearing the bit dropped
// the unit out of remote control, setting it restored remote control, live,
// no reboot needed). Read-modify-write preserves all other bits (the swing
// flag lives in the same register); the read is envelope-validated like
// every transaction.
void handleRemoteEnable() {
    uint8_t addr = getAddress();
    if (!(addr > 0 && addr <= 32)) {
        server.send(500, "text/plain", "address must be between 1 and 32");
        return;
    }
    bool enable = isTrue(server.arg("enable"));

    if (modbusBusy) {
        server.send(503, "text/plain", "modbus busy, retry");
        return;
    }

    IncomingMessage *res = modbusReadRegister(&MODBUS_SERIAL, addr, 224, 1);
    if (!res->success()) {
        server.send(500, "text/plain", "could not read register 224");
        return;
    }
    uint16_t val = (res->data[1] << 8) | res->data[2];
    uint16_t newVal = enable ? (val | 0x0004) : (val & ~0x0004);

    if (newVal == val) {
        server.send(200, "text/plain", enable ? "remote control was already enabled" : "remote control was already disabled");
        return;
    }

    if (modbusWriteRegister(&MODBUS_SERIAL, addr, 224, newVal)->success()) {
#ifdef FANCOIL_REMOTE_GUARD
        if (enable) {
            Fancoil *fancoil = getFancoilByAddress(addr);
            if (fancoil != nullptr) fancoil->notifyRemoteEnabled();
        }
#endif
        server.send(200, "text/plain", enable ? "remote control enabled" : "remote control disabled");
    } else {
        server.send(500, "text/plain", "write to register 224 failed");
    }
}

void handleModbusReadCount() {
    server.send(200, "text/plain", String(modbusReadCount));
}

void handleModbusCollisions() {
    server.send(200, "text/plain", String(modbusCollisionSuspicions));
}

void handleModbusReadErrors() {
    server.send(200, "text/plain", String(modbusReadErrors));
}


void handleModbusErrorRatio() {
    if (modbusReadCount == 0) {
        server.send(200, "text/plain", "0% errors");
        return;
    }
    server.send(200, "text/plain", String(modbusReadErrors * 100 / modbusReadCount) + "% errors");
}


/*
void handleSwing() {
  uint8_t addr = getAddress();

  Fancoil *fancoil = getFancoilByAddress(addr);
  if (fancoil == nullptr) {
    server.send(404, "text/plain", "address not registered");
    return;
  }

  if (fancoil->swing(&MODBUS_SERIAL, getOn())) {
    server.send(200, "text/plain", "ok");
  } else {
    server.send(500, "text/plain", "something went wrong");
  }
}*/

void handleTest() {
    server.send(200, "text/plain", "hi");
}

void setupHttp() {
    // Main routes
    server.on("/", handleRoot);
    server.on("/scripts.js", handleScript);
    server.on("/styles.css", handleStyles);

    // API routes
    server.on("/get", handleGet);
    server.on("/read", handleRead);
    server.on("/write", HTTP_POST, handleWrite);
    server.on("/register", HTTP_POST, handleRegister);
    server.on("/unregister", HTTP_POST, handleUnregister);
    server.on("/list", handleList);
    server.on("/factoryReset", HTTP_POST, handleFactoryReset);
    server.on("/resetWaterTemperatureFault", handleResetWaterTemperatureFault);
    server.on("/uptime", handleUptime);

    // Statistics routes
    server.on("/modbusReadCount", handleModbusReadCount);
    server.on("/modbusReadErrors", handleModbusReadErrors);
    server.on("/modbusErrorRatio", handleModbusErrorRatio);
    server.on("/modbusCollisions", handleModbusCollisions);

    // Control routes
    server.on("/set", HTTP_POST, handleSet);
    server.on("/set", HTTP_GET, handleSet);
    server.on("/test", handleTest);
    server.on("/changeAddress", HTTP_POST, handleChangeAddress);
    server.on("/remoteEnable", HTTP_POST, handleRemoteEnable);

    // Start the server
    //server.begin();
}
