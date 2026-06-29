#include "httpHandlers.h"

bool isTrue(String str) {
    return str == "true" ||
           str == "True" ||
           str == "Yes" ||
           str == "yes" ||
           str == "1" ||
           str == "on" ||
           str == "ON";
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

void handleRoot() {
    String html = "";
    html.reserve(4096);
    
    // HTML header
    html += "<!DOCTYPE html>";
    html += "<html lang=\"en\">";
    html += "<head>";
    html += "    <meta charset=\"UTF-8\">";
    html += "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "    <title>Fancoil Controller</title>";
    html += "    <link rel=\"stylesheet\" href=\"/styles.css\">";
    html += "</head>";
    html += "<body>";
    
    // Header
    html += "    <div class=\"header\">";
    html += "        <div class=\"container header-content\">";
    html += "            <h1>Fancoil Controller</h1>";
    html += "            <div>";
    html += "                <a href=\"https://github.com/dumpfheimer/olimpia_splendid_bi2_modbus_controller\" class=\"btn\">GitHub</a>";
    html += "                <a href=\"/wifiMgr/configure\" class=\"btn\">WiFi & MQTT</a>";
    html += "            </div>";
    html += "        </div>";
    html += "    </div>";
    
    // Navigation
    html += "    <div class=\"nav\">";
    html += "        <div class=\"container\">";
    html += "            <ul class=\"nav-list\">";
    html += "                <li class=\"nav-item\"><button class=\"nav-link tab-link active\" data-tab=\"dashboard\">Dashboard</button></li>";
    html += "                <li class=\"nav-item\"><button class=\"nav-link tab-link\" data-tab=\"settings\">Settings</button></li>";
    html += "                <li class=\"nav-item\"><button class=\"nav-link tab-link\" data-tab=\"statistics\">Statistics</button></li>";
    html += "                <li class=\"nav-item\"><button class=\"nav-link tab-link\" data-tab=\"debug\">Debug</button></li>";
    html += "            </ul>";
    html += "        </div>";
    html += "    </div>";
    
    // Main container
    html += "    <div class=\"container mt-2\">";
    
    // Messages
    html += "        <div id=\"error-message\" style=\"display: none;\" class=\"card\">";
    html += "            <div class=\"card-content\" style=\"background-color: var(--error-color); color: white;\"></div>";
    html += "        </div>";
    html += "        <div id=\"success-message\" style=\"display: none;\" class=\"card\">";
    html += "            <div class=\"card-content\" style=\"background-color: var(--success-color); color: white;\"></div>";
    html += "        </div>";
    
    // Dashboard Tab
    html += "        <div id=\"dashboard-content\" class=\"tab-content active\">";
    html += "            <div class=\"loading\"></div>";
    html += "        </div>";
    
    // Settings Tab
    html += "        <div id=\"settings-content\" class=\"tab-content\">";
    
    // Register Fancoil
    html += "            <div class=\"card mb-2\">";
    html += "                <div class=\"card-header\">Register Fancoil</div>";
    html += "                <div class=\"card-content\">";
    html += "                    <div class=\"form-group\">";
    html += "                        <label class=\"form-label\">Fancoil Address (1-32)</label>";
    html += "                        <input type=\"number\" min=\"1\" max=\"32\" id=\"register-address\" class=\"form-control\">";
    html += "                    </div>";
    html += "                    <button class=\"btn\" onclick=\"registerFancoil()\">Register</button>";
    html += "                </div>";
    html += "            </div>";
    
    // Unregister Fancoil
    html += "            <div class=\"card mb-2\">";
    html += "                <div class=\"card-header\">Unregister Fancoil</div>";
    html += "                <div class=\"card-content\">";
    html += "                    <div class=\"form-group\">";
    html += "                        <label class=\"form-label\">Fancoil Address (1-32)</label>";
    html += "                        <input type=\"number\" min=\"1\" max=\"32\" id=\"unregister-address\" class=\"form-control\">";
    html += "                    </div>";
    html += "                    <button class=\"btn\" onclick=\"unregisterFancoil()\">Unregister</button>";
    html += "                </div>";
    html += "            </div>";
    
    // Change Address
    html += "            <div class=\"card mb-2\">";
    html += "                <div class=\"card-header\">Change Fancoil Address</div>";
    html += "                <div class=\"card-content\">";
    html += "                    <div class=\"form-group\">";
    html += "                        <label class=\"form-label\">Source Address (factory default is 0)</label>";
    html += "                        <input type=\"number\" min=\"0\" max=\"32\" id=\"source-address\" class=\"form-control\">";
    html += "                    </div>";
    html += "                    <div class=\"form-group\">";
    html += "                        <label class=\"form-label\">Target Address (1-32)</label>";
    html += "                        <input type=\"number\" min=\"1\" max=\"32\" id=\"target-address\" class=\"form-control\">";
    html += "                    </div>";
    html += "                    <button class=\"btn\" onclick=\"changeFancoilAddress()\">Change Address</button>";
    html += "                </div>";
    html += "            </div>";
    
    // Refresh Rate
    html += "            <div class=\"card mb-2\">";
    html += "                <div class=\"card-header\">Auto Refresh</div>";
    html += "                <div class=\"card-content\">";
    html += "                    <div class=\"form-group\">";
    html += "                        <label class=\"form-label\">Refresh Rate</label>";
    html += "                        <select id=\"refresh-rate\" class=\"form-select\">";
    html += "                            <option value=\"5000\">5 seconds</option>";
    html += "                            <option value=\"10000\" selected>10 seconds</option>";
    html += "                            <option value=\"30000\">30 seconds</option>";
    html += "                            <option value=\"60000\">1 minute</option>";
    html += "                        </select>";
    html += "                    </div>";
    html += "                </div>";
    html += "            </div>";
    
    // Factory Reset
    html += "            <div class=\"card\">";
    html += "                <div class=\"card-header\">Factory Reset</div>";
    html += "                <div class=\"card-content\">";
    html += "                    <p class=\"mb-1\">This will remove all registered fancoils.</p>";
    html += "                    <button class=\"btn btn-danger\" onclick=\"factoryReset()\">Factory Reset</button>";
    html += "                </div>";
    html += "            </div>";
    html += "        </div>";
    
    // Statistics Tab
    html += "        <div id=\"statistics-content\" class=\"tab-content\">";
    html += "            <div class=\"loading\"></div>";
    html += "        </div>";
    
    // Debug Tab
    html += "        <div id=\"debug-content\" class=\"tab-content\">";
    html += "            <div class=\"card\">";
    html += "                <div class=\"card-header\">Debug</div>";
    html += "                <div class=\"card-content\">";
    html += "                    <div class=\"form-group\">";
    html += "                        <label class=\"form-label\">Fancoil Address</label>";
    html += "                        <input type=\"number\" min=\"0\" max=\"32\" id=\"debugAddress\" class=\"form-control\">";
    html += "                    </div>";
    html += "                    <div class=\"btn-group\">";
    html += "                        <button class=\"btn\" onclick=\"debug()\">Debug</button>";
    html += "                        <button class=\"btn\" onclick=\"debug(quickDebugRegs)\">Quick Debug</button>";
    html += "                    </div>";
    html += "                    <div id=\"debugOut\" class=\"mt-2\"></div>";
    html += "                </div>";
    html += "            </div>";
    html += "        </div>";
    html += "    </div>";
    
    // Footer
    html += "    <script src=\"/scripts.js\"></script>";
    html += "</body>";
    html += "</html>";
    
    server.send(200, "text/html", html);
}

void handleUptime() {
    server.send(200, "text/html", String(millis() / 1000));
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
        String ret = "{";
        ret += "\"address\": " + String(fancoil->getAddress(), HEX) + ",";
        ret += "\"setpoint\": " + String(fancoil->getSetpoint()) + ",";
        ret += "\"ambient\": " + String(fancoil->getAmbient()) + ",";

        if (fancoil->hasValidDesiredState) {
            ret += "\"hasValidDesiredState\": true, ";
        } else {
            ret += "\"hasValidDesiredState\": false, ";
        }

        if (fancoil->wantsToRead()) {
            ret += "\"wantsToRead\": true, ";
        } else {
            ret += "\"wantsToRead\": false, ";
        }

        if (fancoil->wantsToWrite()) {
            ret += "\"wantsToWrite\": true, ";
        } else {
            ret += "\"wantsToWrite\": false, ";
        }

        if (fancoil->isOn()) {
            ret += "\"on\": true, ";
        } else {
            ret += "\"on\": false, ";
        }

        switch (fancoil->getSpeed()) {
            case FanSpeed::MAX:
                ret += "\"speed\": \"MAX\", ";
                break;
            case FanSpeed::NIGHT:
                ret += "\"speed\": \"NIGHT\", ";
                break;
            case FanSpeed::MIN:
                ret += "\"speed\": \"MIN\", ";
                break;
            case FanSpeed::AUTOMATIC:
                ret += "\"speed\": \"AUTOMATIC\", ";
                break;
        }

        if (fancoil->getMode() == Mode::FAN_ONLY) {
            ret += "\"mode\": \"FAN_ONLY\", ";
        } else if (fancoil->getMode() == Mode::COOLING) {
            ret += "\"mode\": \"COOLING\", ";
        } else if (fancoil->getMode() == Mode::HEATING) {
            ret += "\"mode\": \"HEATING\", ";
        } else {
            ret += "\"mode\": \"AUTO\", ";
        }

        if (fancoil->ambientTemperatureIsValid()) {
            ret += "\"ambientTemperatureIsValid\": true, ";
        } else {
            ret += "\"ambientTemperatureIsValid\": false, ";
        }

        if (fancoil->readTimeout()) {
            ret += "\"readTimeout\": true, ";
        } else {
            ret += "\"readTimeout\": false, ";
        }

        if (fancoil->isSwingOn()) {
            ret += "\"swing\": true, ";
        } else {
            ret += "\"swing\": false, ";
        }

        if (fancoil->ev1On()) {
            ret += "\"ev1\": true, ";
        } else {
            ret += "\"ev1\": false, ";
        }

        if (fancoil->ev2On()) {
            ret += "\"ev2\": true, ";
        } else {
            ret += "\"ev2\": false, ";
        }

        if (fancoil->boilerOn()) {
            ret += "\"boiler\": true, ";
        } else {
            ret += "\"boiler\": false, ";
        }

        if (fancoil->chillerOn()) {
            ret += "\"chiller\": true, ";
        } else {
            ret += "\"chiller\": false, ";
        }

        if (fancoil->hasWaterFault()) {
            ret += "\"waterFault\": true, ";
        } else {
            ret += "\"waterFault\": false, ";
        }

#ifdef LOAD_WATER_TEMP
        ret += "\"waterTemp\": " + String(fancoil->getWaterTemp()) + ", ";
#endif

#ifdef LOAD_AMBIENT_TEMP
        ret += "\"ambientTemp\": " + String(fancoil->getAmbientTemp()) + ", ";
#endif

        switch (fancoil->getSyncState()) {
            case SyncState::HAPPY:
                ret += "\"syncState\": \"HAPPY\",";
                break;
            case SyncState::WRITING:
                ret += "\"syncState\": \"WRITING\",";
                break;
            default:
                ret += "\"syncState\": \"INVALID\",";
                break;
        }
	ret += "\"data\":[";
	ret += "\"";
	ret += String(fancoil->getData1(), BIN);
	ret += "\",";
	ret += "\"";
	ret += String(fancoil->getData2(), BIN);
	ret += "\"";
	ret += "],";
	ret += "\"recData\":[";
	ret += "\"";
	ret += String(fancoil->getRecData1(), BIN);
	ret += "\",";
	ret += "\"";
	ret += String(fancoil->getRecData2(), BIN);
	ret += "\"";
	ret += "]";

        ret += "}";

        server.send(200, "application/json", ret);
    }
}

void handleRead() {
    uint8_t addr = getAddress();

    if (!(addr > 0 && addr <= 32)) {
        server.send(500, "text/plain", "address must be between 1 and 32");
        return;
    }

    //Fancoil *fancoil = getFancoilByAddress(addr);

    uint16_t reg = server.arg("reg").toDouble();
    uint16_t len = server.arg("len").toDouble();

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

    if (fancoil->resetWaterTemperatureFault(&MODBUS_SERIAL)) {
        server.send(200, "text/plain", "ok");
    } else {
        server.send(500, "text/plain", "failed");
    }
}

void handleRegister() {
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
        server.send(200, "text/plain", "ok");
    } else {
        server.send(500, "text/plain", "register failed");
    }
}

void handleFactoryReset() {
    EEPROM.write(FANCOIL_EEPROM_START_ADDRESS, 0);
    loadFancoils();
    server.send(200, "text/plain", "ok");
}

void handleUnregister() {
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

    while (fancoilLinkedList != nullptr && fancoilLinkedList->fancoil != nullptr) {
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

    if (modbusWriteRegister(&MODBUS_SERIAL, sourceAddress, 200, targetAddress)->success()) {
        debugPrintln("address change write was successfull");
        server.send(200, "text/plain", "done");
    } else {
        server.send(500, "text/plain", "address changed seems to have failed");
    }
}

void handleModbusReadCount() {
    server.send(200, "text/plain", String(modbusReadCount));
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

    // Control routes
    server.on("/set", HTTP_POST, handleSet);
    server.on("/set", HTTP_GET, handleSet);
    server.on("/test", handleTest);
    server.on("/changeAddress", HTTP_POST, handleChangeAddress);

    // Start the server
    //server.begin();
}
