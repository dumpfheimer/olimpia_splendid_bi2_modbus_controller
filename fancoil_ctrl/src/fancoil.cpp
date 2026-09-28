#include "fancoil.h"

// swing support can be compiled out with #define DISABLE_SWING. Note that
// register 224 lives in the units' wear-limited EEPROM region ("registers
// 200 and successive" per docs/RDE109); writeSwingIfNeeded only writes it
// when a validated read disagrees with the desired state, which is rare now
// that responses are envelope-validated - but on a bus with duplicated
// addresses the alternating responders can disagree every cycle and cause
// sustained EEPROM writes.
#ifdef DISABLE_SWING
bool noSwing = true;
#else
bool noSwing = false;
#endif

Fancoil::Fancoil() {
    init(0);
}

Fancoil::Fancoil(uint8_t add) {
    init(add);
}

uint8_t Fancoil::getAddress() const {
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

#ifdef ENABLE_READ_STATE
    lastReadChangedValues = false;
#endif

    hasValidDesiredState = false;

    sendPeriod = 60000;
    readPeriod = 30000;
    lastRead = 0;
    lastReadTry = 0;
    lastAmbientSet = 0;
    ambientEverSet = false;
    lastCollisionCheck = 0;
    collisionCheckPeriod = 600000;
    collisionSuspected = false;
    collisionSuspicionCount = 0;
    lastProbeSuspicious = false;
    consecutiveFailures = 0;
    lastCommAttempt = 0;
    backoffPeriod = 30000;
    localModeDetected = false;
    localModeRepairs = 0;
#ifdef AMBIENT_TEMPERATURE_TIMEOUT_S
    ambientSetTimeout = AMBIENT_TEMPERATURE_TIMEOUT_S;
#endif

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

    data[0] = 0;
    data[1] = 0;
    recData[0] = 0;
    recData[1] = 0;

}

void Fancoil::setOn(bool set) {
    if (on != set) {
        syncState = SyncState::WRITING;
        notifyStateChanged();
    }
    on = set;
}

void Fancoil::notifyHasValidState() {
    this->hasValidDesiredState = true;
}

bool Fancoil::isOn() const {
    return on;
}

void Fancoil::setSwing(bool swingOnSet) {
    if (swingOn != swingOnSet) {
        syncState = SyncState::WRITING;
        notifyStateChanged();
    }
    swingOn = swingOnSet;
}

bool Fancoil::isSwingOn() const {
    return swingOn;
}

void Fancoil::setSpeed(FanSpeed newSpeed) {
    if (speed != newSpeed) {
        syncState = SyncState::WRITING;
        notifyStateChanged();
    }
    speed = newSpeed;
}

FanSpeed Fancoil::getSpeed() const {
    return speed;
}

void Fancoil::setMode(Mode m) {
    if (mode != m) {
        syncState = SyncState::WRITING;
        notifyStateChanged();
    }
    mode = m;
}

Mode Fancoil::getMode() const {
    return mode;
}

void Fancoil::setSetpoint(double newSetpoint) {
    if (newSetpoint < 15) newSetpoint = 15;
    if (newSetpoint > 40) newSetpoint = 40;

    uint16_t intVal = newSetpoint * 10;
    newSetpoint = (double) intVal / (double) 10;

    if (setpoint != newSetpoint) {
        syncState = SyncState::WRITING;
        notifyStateChanged();
    }
    setpoint = newSetpoint;
}

double Fancoil::getSetpoint() const {
    return setpoint;
}

void Fancoil::setAmbient(double newAmbient) {
    if (newAmbient < 1) newAmbient = 1;
    if (newAmbient > 45) newAmbient = 45;

    uint16_t intVal = newAmbient * 10;
    newAmbient = (double) intVal / (double) 10;

    if (ambientTemperature != newAmbient) {
        syncState = SyncState::WRITING;
        notifyStateChanged();
    }
    ambientTemperature = newAmbient;
    lastAmbientSet = millis();
    ambientEverSet = true;
}

double Fancoil::getAmbient() const {
    return ambientTemperature;
}

#ifdef LOAD_WATER_TEMP
double Fancoil::getWaterTemp() const {
  return waterTemp;
}
#endif
#ifdef LOAD_AMBIENT_TEMP
double Fancoil::getAmbientTemp() {
  return ambientTemp;
}
#endif

bool Fancoil::ev1On() const {
    return ev1;
}

bool Fancoil::ev2On() const {
    return ev2;
}

bool Fancoil::chillerOn() const {
    return chiller;
}

bool Fancoil::boilerOn() const {
    return boiler;
}

bool Fancoil::hasWaterFault() const {
    return waterFault;
}

uint8_t Fancoil::getData1() const {
    return data[0];
}

uint8_t Fancoil::getData2() const {
    return data[1];
}

uint8_t Fancoil::getRecData1() const {
    return recData[0];
}

uint8_t Fancoil::getRecData2() const {
    return recData[1];
}

SyncState Fancoil::getSyncState() const {
    return syncState;
}

bool Fancoil::ambientTemperatureIsValid() const {
#ifdef AMBIENT_TEMPERATURE_TIMEOUT_S
    // never valid until a real reading has been received at least once
    if (!ambientEverSet) {
      return false;
    }
    if ((millis() - lastAmbientSet) < ambientSetTimeout * 1000) {
      return true;
    } else {
      return false;
    }
#else
    return true;
#endif
}

bool Fancoil::readTimeout() const {
    if ((millis() - lastRead) > 2 * readPeriod) {
        return true;
    } else {
        return false;
    }
}

void Fancoil::forceWrite() {
    forceWrite_ = true;
    forceWriteAt = millis();
}
// we do not care about rollover scenarios here
void Fancoil::forceWrite(const unsigned long ms) {
    forceWrite_ = true;
    forceWriteAt = millis() + ms;
}

bool Fancoil::wantsToWrite() {
    if (syncState == SyncState::INVALID) {
        // we want to read first
        debugPrintln("sync state is invalid");
        return false;
    }
    if (forceWrite_ && forceWriteAt <= millis()) {
        debugPrintln("write due to forceWrite");
        forceWrite_ = false;
        return true;
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
	if (millis() - lastReadTry > 10000) {
        	// we want to read if there was an error
        	return true;
	} else {
		// do not read too often
		return false;
	}
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
#ifdef ENABLE_READ_STATE
    if (lastReadChangedValues) {
        return PushResult::READ_CHANGED_VALUES;
    }
#endif
    if (!writeTo(stream)) {
        return PushResult::WRITE_FAILED;
    }
    return PushResult::SUCCESS;
}

// called at every comm-determined exit of readState/writeTo (not at the
// try-lock or no-desired-state bounces, which say nothing about the unit)
void Fancoil::noteCommResult(bool ok) {
    lastCommAttempt = millis();
    if (ok) {
        consecutiveFailures = 0;
    } else if (consecutiveFailures < 255) {
        consecutiveFailures++;
    }
}

uint8_t Fancoil::getConsecutiveFailures() const {
    return consecutiveFailures;
}

bool Fancoil::isLocalModeDetected() const {
    return localModeDetected;
}

uint16_t Fancoil::getLocalModeRepairs() const {
    return localModeRepairs;
}

#ifdef FANCOIL_REMOTE_GUARD
// Detects a unit that is not executing remote control, using a signal the
// unit cannot fake: in genuine remote mode registers 0 and 8 ECHO the
// ambient/setpoint the master writes; in local or half-transitioned "zombie"
// mode they show the unit's own sensor and local setpoint (field-mapped on
// Bi2 Wall TR, 2026-08-29, see docs/register-dumps/). Register 201 (128 =
// remote, 3 = local) is deliberately NOT the detector: writes to it land in
// a RAM shadow that reads back 128 while the unit stays local - including
// our own neutralizing write below, which would blind a 201-based check.
// Both echoes must mismatch to latch, so a written value coinciding with
// the room temperature cannot false-flag.
// The neutralizing write of 128 only DISARMS the local thermostat (stops
// autonomous fan/valve action); full remote restore has only ever been
// achieved at the unit's panel (rE). Damage control + alerting, not repair.
void Fancoil::guardRemoteMode(Stream *stream) {
    // primary signal, field-proven 2026-08-29: register 224 bit 2 IS the
    // remote-enable flag. Definitive, no tolerance heuristics.
    IncomingMessage *resRe = modbusReadRegisterI(stream, address, 224, 1);
    if (resRe->success() && ((((resRe->data[1] << 8) | resRe->data[2]) & 0x0004) == 0)) {
        localModeDetected = true;
        debugPrint("remote-enable bit not set: ");
        debugPrintln(address);
        return;
    }

    // secondary cross-check via the echo test: catches a unit whose rE bit
    // reads set but which is not actually executing remote control
    if (!hasValidDesiredState || !ambientTemperatureIsValid()) return;

    IncomingMessage *res0 = modbusReadRegisterI(stream, address, 0, 1);
    if (!res0->success()) return;
    double effAmbient = ((res0->data[1] << 8) | res0->data[2]) / 10.0;
    if (fabs(effAmbient - getAmbient()) <= 1.0) {
        localModeDetected = false;
        return;
    }

    IncomingMessage *res8 = modbusReadRegisterI(stream, address, 8, 1);
    if (!res8->success()) return;
    double effSetpoint = ((res8->data[1] << 8) | res8->data[2]) / 10.0;
    if (fabs(effSetpoint - getSetpoint()) <= 1.0) {
        localModeDetected = false;
        return;
    }

    localModeDetected = true;
    localModeRepairs++;
    debugPrint("unit not executing remote control: ");
    debugPrintln(address);
    // detection only - per the write policy, nothing is written to a unit
    // that is not remote-enabled; repair goes through /remoteEnable
}

// called by the /remoteEnable endpoint after successfully setting the flag,
// so control resumes immediately instead of waiting for the next probe cycle
void Fancoil::notifyRemoteEnabled() {
    localModeDetected = false;
    forceWrite();
}
#endif

bool Fancoil::writeTo(Stream *stream) {
    if (!hasValidDesiredState) return false;

    // try-lock: if this fancoil is already mid-transaction we got here
    // re-entrantly (the holder is suspended above us on the stack and cannot
    // release until we return), so waiting would only deadlock until timeout.
    if (isBusy) return false;
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
    // The low nibble of data1 is the unit's communication watchdog: minutes
    // without a master write before the unit switches itself off. Armed with
    // a DELIBERATE constant, never echoed back from reads (a misattributed
    // read once armed it with a garbage 1-2 minute fuse). Writes flow every
    // 60s and continue through WiFi outages, so only a dead controller or a
    // unit that lost its address goes unwritten this long - and exactly
    // those units should shut themselves off.
    data1 = data1 | (FANCOIL_COMM_WATCHDOG_MINUTES & 0x0F);

    // data
    uint8_t data2 = 0;
    if (!on) {
        // standby: the fan-speed field MUST be 00 here. A non-zero speed
        // field is a manual ventilation command the unit executes even in
        // standby (observed 2026-08-20: writing off+MAX ran the fan at full
        // with valves open, while off+00 on the sibling unit was truly off).
        // The desired speed stays in RAM and is written again on turn-on.
        data2 = data2 | (1 << 7);
    } else if (speed == FanSpeed::AUTOMATIC) {
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

    bool swingOk = writeSwingIfNeeded(stream);

    isBusy = false;
    readState(stream);
    if (successfullWrites == 3 && swingOk
#ifdef ENABLE_READ_STATE
		    && !lastReadChangedValues
#endif
		    ) {
        syncState = SyncState::HAPPY;
        lastSend = millis();
        noteCommResult(true);
        return true;
    } else {
        noteCommResult(false);
        return false;
    }
}

bool Fancoil::readState(Stream *stream) {
    lastReadTry = millis();
    // read 101, ( and maybe 009 105)
    // try-lock: a busy fancoil means re-entrancy, and the holder is suspended
    // above us on the stack - waiting can never clear it, so bounce.
    if (isBusy) return false;
    isBusy = true;

    IncomingMessage *res = modbusReadRegister(stream, address, 101);
    if (res->success()) {
#ifdef ENABLE_READ_STATE
        lastReadChangedValues = false;
#endif

        byte data1 = res->data[1];
        byte data2 = res->data[2];
	    recData[0] = data1;
	    recData[1] = data2;
        debugPrintln("successfully read 101: ");
        debugPrintln(data1, BIN);
        debugPrintln(data2, BIN);


#ifdef ENABLE_READ_STATE

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

        switch (data2 & 0b11) {
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
#endif
        if (!swingReadOnce && !noSwing) {
            debugPrintln("reading swing configuration");
            IncomingMessage *i = modbusReadRegister(stream, address, 224, 1);
            if (i->valid) {
                if (i->address == address && i->functionCode == 3) {
                    byte data1 = i->data[1];
                    //byte data2 = i->data[2];
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
          waterTemp = (waterTempRead->data[1] << 8 | waterTempRead->data[2]) / 10.0;
        } else {
          debugPrintln("read error");
          isBusy = false;
          return false;
        }
#endif

#ifdef LOAD_AMBIENT_TEMP
        IncomingMessage* ambientTempRead = modbusReadRegister(stream, address, 0);
        if (ambientTempRead->success()) {
          ambientTemp = (ambientTempRead->data[1] << 8 | ambientTempRead->data[2]) / 10.0;
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
#ifdef ENABLE_READ_STATE
        if (lastReadChangedValues) notifyStateChanged();
#endif
#endif

        isBusy = false;
        noteCommResult(true);
        return true;
    } else {
        debugPrintln("read error");
        isBusy = false;
        noteCommResult(false);
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

            // SECURITY GATE: register 224 bit 2 (low byte) is the unit's
            // remote-enable flag (field-proven 2026-08-29). If the read does
            // not show it SET, write NOTHING - a swing write composed from a
            // wrong/garbled read is exactly how units historically lost
            // remote-enable, and a deliberately-local unit must not be
            // touched. Only the /remoteEnable endpoint may write 224 in that
            // state.
            if ((data2 & 0x04) == 0) {
                debugPrintln("swing write refused: remote-enable bit not set in 224");
                return false;
            }

            if (isOn == swingOn) {
                return true;
            } else {
                data1 = data1 ^ 0b10; // flip that bit! = toggle

                // belt and braces: the written value must never clear the
                // remote-enable bit, whatever the read contained
                data2 = data2 | 0x04;

                IncomingMessage *i2 = modbusWriteRegister(stream, address, 224, (data1 << 8) | data2);
                if (!i2->valid) {
                    debugPrintln("swing write invalid");
                    return false;
                }
                if (i2->address == address && i2->functionCode == 6) {
                    return true;
                } else {
                    debugPrintln("swing write address or function code mismatch");
                    return false;
                }
            }
        } else {
            debugPrintln("swing read address or function code mismatch");
            return false;
        }
    } else {
        debugPrintln("swing read not valid");
        return false;
    }
}

bool Fancoil::resetWaterTemperatureFault(Stream *stream) {
    // try-lock: a busy fancoil means re-entrancy, and the holder is suspended
    // above us on the stack - waiting can never clear it, so bounce.
    if (isBusy) return false;
    isBusy = true;

    IncomingMessage *i = modbusReadRegister(stream, address, 104, 1);
    if (i->valid) {
        if (i->address == address && i->functionCode == 3) {
            byte data1 = i->data[1];
            byte data2 = i->data[2];
            bool isFaulty = (data2 & 0b01000000) > 0;

            if (isFaulty) {
                data2 = data2 & 0b10111111;

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

// Detects a second unit sharing this address, from 5 rapid reads of the
// current fan speed (register 16, RPM) with the post-response linger enabled.
// Identical boards answer the same request bit-synchronously (deterministic
// response latency; crystal drift over ~20ms is microseconds, far below the
// 104us bit time), so two responses carrying IDENTICAL data merge into one
// clean frame - which is why probing the water temperature failed: both units
// sit on the same water loop and report the same value. Three signals:
// 1. stray bytes after a complete response = a second, slower responder
// 2. failed reads = two merged responses carrying DIFFERENT data (two real
//    motors never spin at identical RPM, and synchronized drivers that
//    disagree on a bit produce contention garbage)
// 3. RPM spread across the reads = the responders alternate cleanly
// A single suspicious probe only counts; the badge latches on two consecutive
// suspicious probes, so a coincidental WiFi-scan corruption burst or a
// legitimate fan ramp-up during one probe cannot false-latch it.
bool Fancoil::checkForCollisions(Stream *stream) {
    if (isBusy) return false;
    isBusy = true;

    lingerAfterResponse = 50;
    unsigned long suspicionsBefore = modbusCollisionSuspicions;

    uint16_t minVal = 0xFFFF;
    uint16_t maxVal = 0;
    uint8_t validReads = 0;
    uint8_t failedReads = 0;

    for (uint8_t i = 0; i < 5; i++) {
        IncomingMessage *res = modbusReadRegisterI(stream, address, 16, 1);
        if (res->success()) {
            uint16_t val = res->data[1] << 8 | res->data[2];
            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
            validReads++;
        } else {
            failedReads++;
        }
    }

    lingerAfterResponse = 0;
    isBusy = false;

    bool strayResponder = modbusCollisionSuspicions > suspicionsBefore;
    bool contentionErrors = failedReads >= 2;
    // NOTE: an RPM-spread signal was tried and removed: a single fan running
    // at speed wobbles by more than any safe threshold (observed 28 false
    // suspicions on a healthy unit at MAX), and the merged-responder case it
    // targeted is caught by contention failures anyway.
    (void) minVal;
    (void) maxVal;

    bool suspicious = strayResponder || contentionErrors;
    if (suspicious) {
        collisionSuspicionCount++;
        if (lastProbeSuspicious) {
            collisionSuspected = true;
            debugPrint("address collision suspected on ");
            debugPrintln(address);
        }
    }
    lastProbeSuspicious = suspicious;
    return suspicious;
}

bool Fancoil::isCollisionSuspected() const {
    return collisionSuspected;
}

uint16_t Fancoil::getCollisionSuspicionCount() const {
    return collisionSuspicionCount;
}

void Fancoil::loop(Stream *stream) {
    debugPrintln("loop");
    // back off a unit that repeatedly fails to answer: every timed-out
    // transaction blocks the main loop for up to a second, so hammering a
    // dead unit every pass monopolizes the bus, delays WiFi/MQTT servicing
    // and floods the error counters. Probe with a single read per backoff
    // period instead; one success resumes normal operation immediately.
    // A unit that has not answered ONCE since boot backs off immediately: the
    // usual 3-strikes grace exists so a running unit survives a burst of bus
    // noise, but a never-seen address has no state worth protecting and is
    // most likely vacant (mistyped address, unit removed, stale EEPROM entry).
    // Paying 3 full timeout rounds per pass for it starves everything else.
    uint8_t failureBudget = (lastRead == 0) ? 1 : 3;
    if (consecutiveFailures >= failureBudget) {
        if ((millis() - lastCommAttempt) < backoffPeriod) return;
        readState(stream);
        return;
    }
    // only probe a unit that has answered recently: probing a vacant or dead
    // address is 5 blocking timeouts (~2.5s of frozen loop) for zero
    // information. Also skips the probe at boot until the first successful
    // read proves the unit is reachable.
    if (!readTimeout() && ((millis() - lastCollisionCheck) > collisionCheckPeriod || lastCollisionCheck == 0)) {
        lastCollisionCheck = millis();
        checkForCollisions(stream);
#ifdef FANCOIL_REMOTE_GUARD
        guardRemoteMode(stream);
#endif
    }
    // 1. check if values have been received over network recently
    // 2 if not, disconnect, break
    // 3. check if fancoil is connected
    // 4 try to connect to fancoil
    // 5 if needed, read values
    // 6 if needed, write values

    // 1
    if (!ambientTemperatureIsValid()) {
        // No valid ambient temperature.
        // If we have NEVER received one (e.g. we just rebooted), stay completely
        // silent and leave the fancoil in whatever state it already is - a reboot
        // should be transparent and must not disturb a running unit.
        // Only once we HAVE had a valid reading that then went stale do we treat
        // it as a lost temperature feed and turn the unit off for safety.
        if (ambientEverSet && on) {
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
        }
    }
    if (wantsToWrite()) {
        debugPrintln("write!");
        writeTo(stream);
    }
}
