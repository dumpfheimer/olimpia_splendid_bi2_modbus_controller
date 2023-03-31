//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_FANCOIL_MANAGER_H
#define FANCOIL_CTRL_FANCOIL_MANAGER_H

#include <EEPROM.h>

#include "fancoil.h"

#define FANCOIL_EEPROM_START_ADDRESS 0
#define FANCOIL_EEPROM_LENGTH 32

class Fancoil;

struct LinkedFancoilListElement {
    Fancoil *fancoil;
    LinkedFancoilListElement *next;
    LinkedFancoilListElement *prev;
};
typedef LinkedFancoilListElement LinkedFancoilListElement;

Fancoil* getFancoilByAddress(uint8_t addr);
bool registerFancoil(uint8_t registerAddress);
void loadFancoils();
bool unregisterFancoil(uint8_t unregisterAddress);
struct LinkedFancoilListElement *getFirstFancoilListElement();
void setupFancoilManager();
void loopFancoils(Stream *stream);

#endif //FANCOIL_CTRL_FANCOIL_MANAGER_H
