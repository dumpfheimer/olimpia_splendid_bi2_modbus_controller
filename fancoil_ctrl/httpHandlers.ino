bool isLocked = false;
long lockRequestedAt = 0;
long lockTimeout = 10000;

bool isTrue(String str) {
  return str == "true" ||
         str == "True" ||
         str == "Yes" ||
         str == "yes" ||
         str == "1" ||
         str == "on" ||
         str == "ON";
}

void requestLockBlocking() {
  while (isLocked && (millis() - lockRequestedAt) < lockTimeout) ;;
  
  isLocked = true;
  lockRequestedAt = millis();
}

void releaseLock() {
  isLocked = false;
}

void handleScript() {
  // let used = [0,1,8,9,15,16,101,102,103,104,105,108,198,199,200,201,202,203,204,205,206,207,209,210,211,212,213,215,216,218,219,221,222,224,231,233,235,236,237,238,245,246]
  server.send(200, "application/javascript", "let quickDebugRegs = [0,1,8,9,15,16,101,102,103,104,105,108,198,199,200,201,202,203,204,205,206,207,209,210,211,212,213,215,216,218,219,221,222,224,231,233,235,236,237,238,245,246];async function load(url, retry) { try { retry = retry || 5; let x = await fetch(url); let t = await x.text(); if (t != null) return t; else throw 'retry'; } catch(e) {return load(url, retry -1);}}; async function loadNr(url, retry) {let x = await load(url, retry); let r = parseInt(x); if (isNaN(r)) return await loadNr(url, (retry || 5) - 1); else return r}; async function loadReg(addr, reg, retry) {if (retry <= 0) return -1; retry = retry || 5; let x = await fetch(\"/read?addr=\" + addr + \"&reg=\" + reg + \"&len=1\"); if (x.status == 200) {let t = await x.text(); if (t != null && t.indexOf('dec: ') > -1) { let valS = t.substr(t.indexOf(\"dec:\") + 5); let val = parseInt(valS); return val; } else return loadReg(addr, reg, retry - 1);} else {return loadReg(addr, reg, retry - 1);}} async function debug(registers) {if (typeof registers === 'undefined') {registers=[];for (let i = 0; i <= 254; i++){registers.append(i)}};let html=\"<table><thead><tr><td>Addr</td><td>Val (dec)<td></tr></thead><tbody>\"; let s = document.getElementById('debugOut'); let e = document.getElementById('debugAddress'); let a = e.value; let ret = {}; for (let i of registers) { s.innerText = 'Loading register ' + i; ret[i] = await loadReg(a, i); html += \"<tr><td>\" + i + \"</td><td>\" + ret[i] + \"</td></tr>\";}; ret['errorRatio'] = await load('/modbusErrorRatio'); ;html +=\"</tbody></table>Base64: \" + btoa(JSON.stringify(ret)); s.innerHTML = html;}");
}

void handleRoot() {
  server.send(200, "text/html", "<html><head><script src=\"s.js\"></script></head><body><a href=\"https://github.com/dumpfheimer/olimpia_splendid_bi2_modbus_controller\">Github page for help</a></br><form method=\"POST\" action=\"register\"><h3>Register fancoil</h3><br/><input type=\"number\" min=\"1\" max=\"32\" name=\"addr\"><input type=\"submit\"></form><form method=\"POST\" action=\"unregister\"><h3>Unregister fancoil</h3><br/><input type=\"number\" min=\"1\" max=\"32\" name=\"addr\"><input type=\"submit\"></form><form method=\"POST\" action=\"changeAddress\"><h3>Change fancoil address</h3><br/>Source Address (factory default is 0)<br/><input type=\"number\" min=\"0\" max=\"32\" name=\"sourceAddress\"><br/>Target Address (1-32):<br/><input type=\"number\" min=\"0\" max=\"32\" name=\"targetAddress\"><br/><input type=\"submit\"></form><h3>Debug</h3>Fan coil address:<br/><input type=\"number\" min=\"0\" max=\"32\" id=\"debugAddress\"><button onclick=\"debug()\">Debug</button><button onclick=\"debug(quickDebugRegs)\">Quick Debug</button><div id=\"debugOut\"></div></body></html>");
}

void handleGet() {
  uint8_t addr = getAddress();
  
  if (!(addr > 0 && addr <= 32)) {
    server.send(500, "text/plain", "address must be between 1 and 32");
    return;
  }

  Fancoil *fancoil = getFancoilByAddress(addr);
  
  if (fancoil == NULL) {
    server.send(404, "text/plain", "address not registered");
  } else {
    String ret = "{";
    ret += "\"address\": " + String(fancoil->getAddress(), HEX) + ",";
    ret += "\"setpoint\": " + String(fancoil->getSetpoint()) + ",";
    ret += "\"ambient\": " + String(fancoil->getAmbient()) + ",";
    
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
        ret += "\"syncState\": \"HAPPY\"";
        break;
      case SyncState::WRITING:
        ret += "\"syncState\": \"WRITING\"";
        break;
      default:
        ret += "\"syncState\": \"INVALID\"";
        break;
    }
    
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

  Fancoil *fancoil = getFancoilByAddress(addr);

  uint16_t reg = server.arg("reg").toDouble();
  uint16_t len = server.arg("len").toDouble();

  IncomingMessage* i = modbusReadRegister(&MODBUS_SERIAL, addr, reg, len);

  if (!i->valid) {
    server.send(500, "text/plain", "invalid or no response");
  } else if (i->isError) {
    server.send(500, "text/plain", "fan coil returned error");
  } else {
    server.send(200, "text/plain", String(i->data[1], HEX) + " " + String(i->data[2], HEX) + " bin: " +  String(i->data[1], BIN) + " " + String(i->data[2], BIN) + " dec: " + String((i->data[1] << 8) | i->data[2], DEC));
  }
}

void handleWrite() {
  uint8_t addr = getAddress();
  
  if (!(addr > 0 && addr <= 32)) {
    server.send(500, "text/plain", "address must be between 1 and 32");
    return;
  }

  Fancoil *fancoil = getFancoilByAddress(addr);

  uint16_t reg = server.arg("reg").toDouble();
  uint16_t val = server.arg("val").toDouble();

  IncomingMessage* i = modbusWriteRegister(&MODBUS_SERIAL, addr, reg, val);

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

  if (fancoil != NULL) {
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

  if (fancoil == NULL) {
    server.send(500, "text/plain", "address not registered");
    return;
  }

  if (unregisterFancoil(addr)) {
    server.send(200, "text/plain", "ok");
  } else {
    server.send(500, "text/plain", "unregister failed");
  }
  #ifdef MQTT_HOST
  unconfigureHomeAssistantDevice(String(addr));
  #endif
}

void handleList() {
  LinkedFancoilListElement *fancoilLinkedList = getFirstFancoilListElement();

  debugPrintln("creating list");
  String ret = "[";

  while (fancoilLinkedList != NULL && fancoilLinkedList->fancoil != NULL) {
    ret += String(fancoilLinkedList->fancoil->getAddress());
    if (fancoilLinkedList->next != NULL) ret += ",";
    fancoilLinkedList = fancoilLinkedList->next;
  }
  debugPrintln("creating list finished");

  ret += "]";
  
  server.send(200, "application/json", ret);
}

uint8_t getAddress() {
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "addr") return strtoul(server.arg(i).c_str(), NULL, 10);
    if (server.argName(i) == "address") return strtoul(server.arg(i).c_str(), NULL, 10);
  }
  return 0;
}

String getValue() {
  if (server.hasArg("val")) return server.arg("val");
  if (server.hasArg("value")) return server.arg("value");
  
  return String("");
}

uint16_t getLength() {
  if (server.hasArg("len")) return strtoul(server.arg("len").c_str(), NULL, 10);
  if (server.hasArg("length")) return strtoul(server.arg("length").c_str(), NULL, 10);
  
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
  if (fancoil == NULL) {
    server.send(404, "application/json", "{\"error\": \"address not registered\"}");
    return;
  }

  fancoil->readState(&MODBUS_SERIAL);
  /*if (fancoil->lastReadChangedValues) {
    server.send(500, "application/json", "{\"error\": \"out of sync\"}");
    return;
  }*/

  if (server.hasArg("on")) {
    fancoil->setOn(isTrue(server.arg("on")));
  }

  if (server.hasArg("ambient")) {
    double ambient = server.arg("ambient").toDouble();
    if (ambient < 15) {
      ambient = 15;
    }
    if (ambient > 45) {
      ambient = 45;
    }
    fancoil->setAmbient(ambient);
  }

  if (server.hasArg("setpoint")) {
    double d = server.arg("setpoint").toDouble();
    if (d < 16 || d > 30) {
      server.send(500, "application/json", "{\"error\": \"invalid setpoint provided\"}");
      return;
    }
    fancoil->setSetpoint(d);
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

  if (fancoil->writeTo(&MODBUS_SERIAL)) {
    handleGet();
  } else {
    server.send(500, "application/json", "{\"error\": \"write failed\"}");
  }
  
  #ifdef MQTT_HOST
  notifyStateChanged();
  #endif
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
  server.send(200, "text/plain", String(modbusReadErrors * 100 / modbusReadCount) + "% errors");
}


/*
void handleSwing() {
  uint8_t addr = getAddress();

  Fancoil *fancoil = getFancoilByAddress(addr);
  if (fancoil == NULL) {
    server.send(404, "text/plain", "address not registered");
    return;
  }

  if (fancoil->swing(&MODBUS_SERIAL, getOn())) {
    server.send(200, "text/plain", "ok");
  } else {
    server.send(500, "text/plain", "something went wrong");
  }
}*/

void setupHttp() {
  server.on("/", handleRoot);
  server.on("/s.js", handleScript);
  server.on("/get", handleGet);
  server.on("/read", handleRead);
  server.on("/write", HTTP_POST, handleWrite);
  server.on("/register", HTTP_POST, handleRegister);
  server.on("/unregister", HTTP_POST, handleUnregister);
  server.on("/list", handleList);
  server.on("/factoryReset", HTTP_POST, handleFactoryReset);
  server.on("/resetWaterTemperatureFault", handleResetWaterTemperatureFault);
  
  server.on("/modbusReadCount", handleModbusReadCount);
  server.on("/modbusReadErrors", handleModbusReadErrors);
  server.on("/modbusErrorRatio", handleModbusErrorRatio);

  //server.on("/setAmbient", HTTP_POST, handleSetAmbient);
  //server.on("/setSetpoint", HTTP_POST, handleSetSetpoint);
  //server.on("/setFanSpeed", HTTP_POST, handleSetFanSpeed);

  //server.on("/setOn", HTTP_POST, handleSetOn);
  //server.on("/setOff", HTTP_POST, handleSetOff);
  
  server.on("/set", HTTP_POST, handleSet);
  server.on("/set", HTTP_GET, handleSet);

  //server.on("/swing", HTTP_GET, handleSwing);
  
  server.on("/changeAddress", HTTP_POST, handleChangeAddress);
 
  server.begin(); 
}
