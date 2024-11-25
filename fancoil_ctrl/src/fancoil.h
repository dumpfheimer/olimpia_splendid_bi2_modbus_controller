//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_FANCOIL_H
#define FANCOIL_CTRL_FANCOIL_H

#include <cstdint>
#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "logging.h"
#include "modbus_ascii.h"

enum FanSpeed {
    AUTOMATIC = 0x00,
    MIN = 0x01,
    NIGHT = 0x10,
    MAX = 0x11
};
enum Mode {
    COOLING = 0x10,
    HEATING = 0x01,
    AUTO = 0x00,
    FAN_ONLY = 0x11
};
enum SyncState {
    HAPPY = 0x11,
    WRITING = 0x01,
    INVALID = 0x10
};
enum AbsenceCondition {
    FORCED,
    NOT_FORCED
};
enum PushResult {
    SUCCESS = 0b11,
    READ_CHANGED_VALUES = 0b111,
    WRITE_FAILED = 0x01010001
};

extern bool noSwing;

class Fancoil {
private:
    SyncState syncState = SyncState::INVALID;

    uint8_t address = 0;

    bool on = false;
    bool isBusy = false;
    FanSpeed speed = FanSpeed::AUTOMATIC;
    Mode mode = Mode::COOLING;
    AbsenceCondition absenceConditionForced = AbsenceCondition::NOT_FORCED;

    double setpoint = 22;
    double ambientTemperature = 21;

    uint8_t data[2];
    uint8_t recData[2];

    // the last successful write
    unsigned long lastSend = 0;
    unsigned long sendPeriod = 60000;

    // the last receive time
    unsigned long lastAmbientSet = 0;
#ifdef AMBIENT_TEMPERATURE_TIMEOUT_S
    unsigned long ambientSetTimeout = AMBIENT_TEMPERATURE_TIMEOUT_S;
#endif

    // the last successful read
    unsigned long lastRead = 0;
    unsigned long lastReadTry = 0;
    unsigned long readPeriod = 40000;

    byte communicationTimer = 0;
    bool swingOn = true;
    bool swingReadOnce = false;
    bool ev1 = false;
    bool ev2 = false;
    bool boiler = false;
    bool chiller = false;
    bool waterFault = false;
#ifdef LOAD_WATER_TEMP
    double waterTemp = 0;
#endif

#ifdef LOAD_AMBIENT_TEMP
    double ambientTemp = 0;
#endif

public:
#ifdef ENABLE_READ_STATE
    bool lastReadChangedValues = false;
#endif
    bool isInUse = false;
    bool hasValidDesiredState = false;

    Fancoil();
    Fancoil(uint8_t add);
    uint8_t getAddress();
    void init(uint8_t addr);
    void setOn(bool set);
    bool isOn();
    void notifyHasValidState();
    void setSwing(bool swingOnSet);
    bool isSwingOn();
    void setSpeed(FanSpeed newSpeed);
    FanSpeed getSpeed();
    void setMode(Mode m);
    Mode getMode();
    void setSetpoint(double newSetpoint);
    double getSetpoint();
    void setAmbient(double newAmbient);
    double getAmbient();
    double getWaterTemp();
    bool ev1On();
    bool ev2On();
    bool chillerOn();
    bool boilerOn();
    bool hasWaterFault();
    SyncState getSyncState();
    bool ambientTemperatureIsValid();
    bool readTimeout();
    bool wantsToWrite();
    bool wantsToRead();
    PushResult pushState(Stream *stream);
    bool writeTo(Stream *stream);
    bool readState(Stream *stream);
    bool writeSwingIfNeeded(Stream *stream);
    bool resetWaterTemperatureFault(Stream* stream);
    void loop(Stream *stream);
    uint8_t getRecData1();
    uint8_t getRecData2();
    uint8_t getData1();
    uint8_t getData2();
};


#endif //FANCOIL_CTRL_FANCOIL_H
