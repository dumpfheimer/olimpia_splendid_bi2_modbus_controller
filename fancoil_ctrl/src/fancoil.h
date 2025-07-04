//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_FANCOIL_H
#define FANCOIL_CTRL_FANCOIL_H

#include <cstdint>
#include <Arduino.h>

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
    volatile bool isBusy = false;
    FanSpeed speed = FanSpeed::AUTOMATIC;
    Mode mode = Mode::COOLING;
    AbsenceCondition absenceConditionForced = AbsenceCondition::NOT_FORCED;

    double setpoint = 22;
    double ambientTemperature = 21;
    bool forceWrite_ = false;
    unsigned long forceWriteAt = 0;

    uint8_t data[2]{0};
    uint8_t recData[2]{0};

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
    explicit Fancoil(uint8_t add);
    [[nodiscard]] uint8_t getAddress() const;
    void init(uint8_t addr);
    void setOn(bool set);
    [[nodiscard]] bool isOn() const;
    void notifyHasValidState();
    void setSwing(bool swingOnSet);
    [[nodiscard]] bool isSwingOn() const;
    void setSpeed(FanSpeed newSpeed);
    [[nodiscard]] FanSpeed getSpeed() const;
    void setMode(Mode m);
    [[nodiscard]] Mode getMode() const;
    void setSetpoint(double newSetpoint);
    [[nodiscard]] double getSetpoint() const;
    void setAmbient(double newAmbient);
    [[nodiscard]] double getAmbient() const;
    [[nodiscard]] double getWaterTemp() const;
    [[nodiscard]] bool ev1On() const;
    [[nodiscard]] bool ev2On() const;
    [[nodiscard]] bool chillerOn() const;
    [[nodiscard]] bool boilerOn() const;
    [[nodiscard]] bool hasWaterFault() const;
    [[nodiscard]] SyncState getSyncState() const;
    [[nodiscard]] bool ambientTemperatureIsValid() const;
    [[nodiscard]] bool readTimeout() const;
    void forceWrite();
    void forceWrite(unsigned long ms);
    bool wantsToWrite();
    bool wantsToRead();
    PushResult pushState(Stream *stream);
    bool writeTo(Stream *stream);
    bool readState(Stream *stream);
    bool writeSwingIfNeeded(Stream *stream);
    bool resetWaterTemperatureFault(Stream* stream);
    void loop(Stream *stream);
    [[nodiscard]] uint8_t getRecData1() const;
    [[nodiscard]] uint8_t getRecData2() const;
    [[nodiscard]] uint8_t getData1() const;
    [[nodiscard]] uint8_t getData2() const;
};


#endif //FANCOIL_CTRL_FANCOIL_H
