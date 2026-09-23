//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_FANCOIL_H
#define FANCOIL_CTRL_FANCOIL_H

#include <Arduino.h>

#include "modbus_ascii.h"

enum FanSpeed {
    AUTOMATIC = 0b00,
    MIN = 0b01,
    NIGHT = 0b10,
    MAX = 0b11
};
enum Mode {
    COOLING = 0b10,
    HEATING = 0b01,
    AUTO = 0b00,
    FAN_ONLY = 0b11
};
enum SyncState {
    HAPPY = 0b11,
    WRITING = 0b01,
    INVALID = 0b10
};
enum AbsenceCondition {
    FORCED,
    NOT_FORCED
};
enum PushResult {
    SUCCESS = 0b11,
    READ_CHANGED_VALUES = 0b111,
    WRITE_FAILED = 0b01010001
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
    // whether a real ambient temperature has ever been received; until it has,
    // the value is not valid regardless of timers (avoids treating the boot
    // default as a fresh reading for the first AMBIENT_TEMPERATURE_TIMEOUT_S)
    bool ambientEverSet = false;
#ifdef AMBIENT_TEMPERATURE_TIMEOUT_S
    unsigned long ambientSetTimeout = AMBIENT_TEMPERATURE_TIMEOUT_S;
#endif

    // the last successful read
    unsigned long lastRead = 0;
    unsigned long lastReadTry = 0;
    unsigned long readPeriod = 40000;

    // periodic address-collision probe (see checkForCollisions)
    unsigned long lastCollisionCheck = 0;
    unsigned long collisionCheckPeriod = 600000; // 10 minutes
    bool collisionSuspected = false;
    uint16_t collisionSuspicionCount = 0;
    bool lastProbeSuspicious = false;

    // remote-mode guard (see loop): register 201 read on the probe cadence;
    // != 128 means the unit fell back to local control and ignores commands
    bool localModeDetected = false;
    uint16_t localModeRepairs = 0;

    // backoff for unresponsive units (see loop): after 3 consecutive comm
    // failures the unit is only probed once per backoffPeriod
    uint8_t consecutiveFailures = 0;
    unsigned long lastCommAttempt = 0;
    unsigned long backoffPeriod = 30000;

    void noteCommResult(bool ok);

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
    bool checkForCollisions(Stream *stream);
#ifdef FANCOIL_REMOTE_GUARD
    void guardRemoteMode(Stream *stream);
    void notifyRemoteEnabled();
#endif
    [[nodiscard]] bool isCollisionSuspected() const;
    [[nodiscard]] uint16_t getCollisionSuspicionCount() const;
    [[nodiscard]] uint8_t getConsecutiveFailures() const;
    [[nodiscard]] bool isLocalModeDetected() const;
    [[nodiscard]] uint16_t getLocalModeRepairs() const;
    void loop(Stream *stream);
    [[nodiscard]] uint8_t getRecData1() const;
    [[nodiscard]] uint8_t getRecData2() const;
    [[nodiscard]] uint8_t getData1() const;
    [[nodiscard]] uint8_t getData2() const;
};


#endif //FANCOIL_CTRL_FANCOIL_H
