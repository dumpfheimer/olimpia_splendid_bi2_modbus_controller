#include "fancoil.h"

bool noSwing = false;

Fancoil::Fancoil() {
    init(0);
}

Fancoil::Fancoil(uint8_t add) {
    init(add);
}

uint8_t Fancoil::getAddress() {
    return address;
}

void Fancoil::init(uint8_t addr) {
    if (isInUse) return;
    on = false;
    isBusy = false;
    address = addr;
    isInUse = true;
    setpoint = 22;
    ambientTemperature = 21;
    speed = FanSpeed::AUTOMATIC;
    mode = Mode::COOLING;
    absenceConditionForced = AbsenceCondition::NOT_FORCED;
    syncState = SyncState::INVALID;

    sendPeriod = 60000;
    readPeriod = 30000;
#ifdef AMBIENT_TEMPERATURE_TIMEOUT_S
    ambientSetTimeout = AMBIENT_TEMPERATURE_TIMEOUT_S;
#endif
    communicationTimer = 0;

    ev1 = false;
    ev2 = false;
    boiler = false;
    chiller = false;
    waterFault = false;
#ifdef LOAD_WATER_TEMP
    waterTemp = 0;
#endif
#ifdef LOAD_AMBIENT_TEMP
    ambientTemp = 0;
#endif

}

void Fancoil::setOn(bool set) {
    if (on != set) syncState = SyncState::WRITING;
    on = set;
}

bool Fancoil::isOn() {
    return on;
}

void Fancoil::setSwing(bool swingOnSet) {
    swingOn = swingOnSet;
}

bool Fancoil::isSwingOn() {
    return swingOn;
}

void Fancoil::setSpeed(FanSpeed newSpeed) {
    if (speed != newSpeed) syncState = SyncState::WRITING;
    speed = newSpeed;
}

FanSpeed Fancoil::getSpeed() {
    return speed;
}

void Fancoil::setMode(Mode m) {
    if (mode != m) syncState = SyncState::WRITING;
    mode = m;
}

Mode Fancoil::getMode() {
    return mode;
}

void Fancoil::setSetpoint(double newSetpoint) {
    if (newSetpoint < 15) newSetpoint = 15;
    if (newSetpoint > 40) newSetpoint = 40;

    uint16_t intVal = newSetpoint * 10;
    newSetpoint = (double) intVal / (double) 10;

    if (setpoint != newSetpoint) syncState = SyncState::WRITING;
    setpoint = newSetpoint;
}

double Fancoil::getSetpoint() {
    return setpoint;
}

void Fancoil::setAmbient(double newAmbient) {
    if (newAmbient < 1) newAmbient = 1;
    if (newAmbient > 45) newAmbient = 45;

    uint16_t intVal = newAmbient * 10;
    newAmbient = (double) intVal / (double) 10;

    if (ambientTemperature != newAmbient) syncState = SyncState::WRITING;
    ambientTemperature = newAmbient;
    lastAmbientSet = millis();
}

double Fancoil::getAmbient() {
    return ambientTemperature;
}

#ifdef LOAD_WATER_TEMP
double Fancoil::getWaterTemp() {
  return waterTemp;
}
#endif
#ifdef LOAD_AMBIENT_TEMP
double Fancoil::getAmbientTemp() {
  return ambientTemp;
}
#endif

bool Fancoil::ev1On() {
    return ev1;
}

bool Fancoil::ev2On() {
    return ev2;
}

bool Fancoil::chillerOn() {
    return chiller;
}

bool Fancoil::boilerOn() {
    return boiler;
}

bool Fancoil::hasWaterFault() {
    return waterFault;
}

SyncState Fancoil::getSyncState() {
    return syncState;
}

bool Fancoil::ambientTemperatureIsValid() {
#ifdef AMBIENT_TEMPERATURE_TIMEOUT_S
    if ((millis() - lastAmbientSet) < ambientSetTimeout * 1000) {
      return true;
    } else {
      return false;
    }
#else
    return true;
#endif
}

bool Fancoil::readTimeout() {
    if ((millis() - lastRead) > 2 * readPeriod) {
        return true;
    } else {
        return false;
    }
}

bool Fancoil::wantsToWrite() {
    if (syncState == SyncState::INVALID) {
        // we want to read first
        debugPrintln("sync state is invalid");
        return false;
    }
    // write periodically
    if ((millis() - lastSend) > sendPeriod) {
        debugPrintln("periodic send");
        return true;
    }
    if (syncState == SyncState::WRITING) {
        debugPrintln("sync state is writing");
        return true;
    }
    return false;
}

bool Fancoil::wantsToRead() {
    if (syncState == SyncState::WRITING) {
        debugPrintln("no read: writing");
        // do not read if we are writing
        return false;
    }
    if (syncState == SyncState::INVALID && (millis() - lastRead) > 10000) {
        debugPrintln("read: invalid state");
        // we want to read if there was an error
        return true;
    }
    if ((millis() - lastRead) > readPeriod) {
        debugPrintln("read periodically!");
        return true;
    }
    debugPrintln("no read");
    return false;
}

PushResult Fancoil::pushState(Stream *stream) {
    readState(stream);
    if (lastReadChangedValues) {
        return PushResult::READ_CHANGED_VALUES;
    }
    if (!writeTo(stream)) {
        return PushResult::WRITE_FAILED;
    }
    return PushResult::SUCCESS;
}

bool Fancoil::writeTo(Stream *stream) {
    uint8_t data[2];

    while (isBusy) yield();
    isBusy = true;

    // data
    uint8_t data1 = 0;
    if (mode == Mode::COOLING) {
        data1 = data1 | (1 << 6);
    } else if (mode == Mode::HEATING) {
        data1 = data1 | (1 << 5);
    } else if (mode == Mode::FAN_ONLY) {
        data1 = data1 | (1 << 5) | (1 << 6);
    } else {
        // leave the 0s
    }
    if (absenceConditionForced) {
        //data1 = data1 | (1 << 4);
    }
    data1 = data1 | communicationTimer;
    // communication timer?!

    // data
    uint8_t data2 = 0;
    if (!on) {
        // always on right now
        data2 = data2 | (1 << 7);
    }
    if (speed == FanSpeed::AUTOMATIC) {
        // 00
    } else if (speed == FanSpeed::MIN) {
        data2 = data2 | 0b01;
    } else if (speed == FanSpeed::NIGHT) {
        data2 = data2 | 0b10;
    } else if (speed == FanSpeed::MAX) {
        data2 = data2 | 0b11;
    }

    data[0] = data1;
    data[1] = data2;

    uint8_t successfullWrites = 0;
    if (modbusWriteRegister(stream, address, 101, (data1 << 8) | data2)->success()) {
        debugPrintln("write 1 was successfull");
        successfullWrites++;
    }

    double writeSetpoint = getSetpoint();

    if (modbusWriteRegister(stream, address, 102, (uint16_t)(writeSetpoint * 10))->success()) {
        debugPrintln("write 2 was successfull");
        successfullWrites++;
    }

    if (modbusWriteRegister(stream, address, 103, (uint16_t)(getAmbient() * 10))->success()) {
        debugPrintln("write 3 was successfull");
        successfullWrites++;
    }

    if (!writeSwingIfNeeded(stream)) {
        successfullWrites--;
    }

    isBusy = false;
    readState(stream);
    if (successfullWrites == 3) {
        syncState = SyncState::HAPPY;
        lastSend = millis();
        return true;
    } else {
        return false;
    }
}

bool Fancoil::readState(Stream *stream) {
    // read 101, ( and maybe 009 105)

    while (isBusy) yield();
    isBusy = true;

    IncomingMessage *res = modbusReadRegister(stream, address, 101);
    if (res->success()) {
        lastReadChangedValues = false;

        byte data1 = res->data[1];
        byte data2 = res->data[2];
        debugPrintln("successfully read 101: ");
        debugPrintln(data1, BIN);
        debugPrintln(data2, BIN);

        communicationTimer = data1 & 0x0F;

        if ((data1 & 0b01000000) && (data1 & 0b00100000)) {
            if (mode != Mode::FAN_ONLY) lastReadChangedValues = true;
            mode = Mode::FAN_ONLY;
        } else if (data1 & 0b01000000) {
            if (mode != Mode::COOLING) lastReadChangedValues = true;
            mode = Mode::COOLING;
        } else if (data1 & 0b00100000) {
            if (mode != Mode::HEATING) lastReadChangedValues = true;
            mode = Mode::HEATING;
        } else {
            if (mode != Mode::AUTO) lastReadChangedValues = true;
            mode = Mode::AUTO;
        }

        if (data1 & 0b00010000) {
            if (absenceConditionForced != AbsenceCondition::FORCED) lastReadChangedValues = true;
            absenceConditionForced = AbsenceCondition::FORCED;
        } else {
            if (absenceConditionForced != AbsenceCondition::NOT_FORCED) lastReadChangedValues = true;
            absenceConditionForced = AbsenceCondition::NOT_FORCED;
        }

        if (data2 & 0b10000000) {
            if (on) lastReadChangedValues = true;
            debugPrintln("switched off by read");
            on = false;
        } else {
            if (!on) lastReadChangedValues = true;
            debugPrintln("switched on by read");
            on = true;
        }

        switch (data2 & 0b111) {
            case 0b11:
                if (speed != FanSpeed::MAX) lastReadChangedValues = true;
                speed = FanSpeed::MAX;
                break;
            case 0b10:
                if (speed != FanSpeed::NIGHT) lastReadChangedValues = true;
                speed = FanSpeed::NIGHT;
                break;
            case 0b01:
                if (speed != FanSpeed::MIN) lastReadChangedValues = true;
                speed = FanSpeed::MIN;
                break;
            case 0b00:
                if (speed != FanSpeed::AUTOMATIC) lastReadChangedValues = true;
                speed = FanSpeed::AUTOMATIC;
                break;
        }

        if (!swingReadOnce && !noSwing) {
            debugPrintln("reading swing configuration");
            IncomingMessage *i = modbusReadRegister(stream, address, 224, 1);
            if (i->valid) {
                if (i->address == address && i->functionCode == 3) {
                    byte data1 = i->data[1];
                    byte data2 = i->data[2];
                    bool isOn = (data1 & 0b10) > 0;

                    debugPrint("swing is ");
                    if (isOn) {
                        debugPrintln("on");
                    } else {
                        debugPrintln("off");
                    }
                    swingOn = isOn;
                    swingReadOnce = true;
                }
            }
        }

        IncomingMessage *valveRead = modbusReadRegister(stream, address, 9);
        if (valveRead->success()) {
            ev1 = (valveRead->data[1] & 0b01000000) > 0;
            boiler = (valveRead->data[1] & 0b00100000) > 0;
            chiller = (valveRead->data[1] & 0b00010000) > 0;
            ev2 = (valveRead->data[1] & 0b00001000) > 0;
        } else {
            debugPrintln("read error");
            isBusy = false;
            return false;
        }

#ifdef LOAD_WATER_TEMP
        IncomingMessage* waterTempRead = modbusReadRegister(stream, address, 1);
        if (waterTempRead->success()) {
          waterTemp = (waterTempRead->data[1] << 8 | waterTempRead->data[2]) / 10;
        } else {
          debugPrintln("read error");
          isBusy = false;
          return false;
        }
#endif

#ifdef LOAD_AMBIENT_TEMP
        IncomingMessage* ambientTempRead = modbusReadRegister(stream, address, 0);
        if (ambientTempRead->success()) {
          ambientTemp = (ambientTempRead->data[1] << 8 | ambientTempRead->data[2]) / 10;
        } else {
          debugPrintln("read error");
          isBusy = false;
          return false;
        }
#endif

        if (on) {
            IncomingMessage *faultRead = modbusReadRegister(stream, address, 104);
            if (faultRead->success()) {
                waterFault = (faultRead->data[2] & 0b01000000) > 0;
            } else {
                debugPrintln("read error");
                isBusy = false;
                return false;
            }
        } else {
            waterFault = false;
        }

        debugPrintln("read success");
        lastRead = millis();

#ifdef MQTT_HOST
        if (lastReadChangedValues) notifyStateChanged();
#endif

        isBusy = false;
        return true;
    } else {
        debugPrintln("read error");
        isBusy = false;
        return false;
    }
}

bool Fancoil::writeSwingIfNeeded(Stream *stream) {
    if (noSwing) {
        return true;
    }
    if (WiFi.macAddress() == "48:3F:DA:45:35:EE") {
        return true;
    }
    IncomingMessage *i = modbusReadRegister(stream, address, 224, 1);
    if (i->valid) {
        if (i->address == address && i->functionCode == 3) {
            byte data1 = i->data[1];
            byte data2 = i->data[2];
            bool isOn = (data1 & 0b10) > 0;

            if (isOn == swingOn) {
                return true;
            } else {
                data1 = data1 ^ 0b10; // flip that bit! = toggle

                IncomingMessage *i2 = modbusWriteRegister(stream, address, 224, (data1 << 8) | data2);
                if (!i2->valid) {
                    server.send(501, "text/plain", "write invalid");
                    return false;
                }
                if (i2->address == address && i2->functionCode == 6) {
                    return true;
                } else {
                    server.send(501, "text/plain", "write address or function code mismatch");
                    return false;
                }
            }
        } else {
            server.send(501, "text/plain", "address or function code mismatch");
            return false;
        }
    } else {
        server.send(501, "text/plain", "not valid");
        return false;
    }
}

bool Fancoil::resetWaterTemperatureFault(Stream *stream) {
    while (isBusy) {}
    isBusy = true;

    IncomingMessage *i = modbusReadRegister(stream, address, 104, 1);
    if (i->valid) {
        if (i->address == address && i->functionCode == 3) {
            byte data1 = i->data[1];
            byte data2 = i->data[2];
            bool isFaulty = data2 & 0x01000000 > 0;

            if (isFaulty) {
                data2 = data2 & 0x10111111;

                IncomingMessage *i2 = modbusWriteRegister(stream, address, 104, (data1 << 8) | data2);
                isBusy = false;

                if (!i2->valid) {
                    return false;
                }
                if (i2->address == address && i2->functionCode == 6) {
                    return true;
                } else {
                    return false;
                }
            } else {
                isBusy = false;
                return true;
            }
        }
    }

    isBusy = false;
    return false;
}

void Fancoil::loop(Stream *stream) {
    debugPrintln("loop");
    // 1. check if values have been received over network recently
    // 2 if not, disconnect, break
    // 3. check if fancoil is connected
    // 4 try to connect to fancoil
    // 5 if needed, read values
    // 6 if needed, write values

    // 1
    if (!ambientTemperatureIsValid()) {
        // ambient temperature is invalid
        // turn off fancoil, if its running.
        if (on) {
            setOn(false);
            writeTo(stream);
        }
        return;
    }
    // check if read is appropriate
    if (wantsToRead()) {
        debugPrintln("wants to read");
        // read timeout!
        if (!readState(stream)) {
            debugPrintln("Read failed");
            return;
        }
    }
    if (wantsToWrite()) {
        debugPrintln("write!");
        writeTo(stream);
    }
}