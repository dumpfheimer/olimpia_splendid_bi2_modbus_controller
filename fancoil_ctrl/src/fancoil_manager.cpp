#include "fancoil_manager.h"


LinkedFancoilListElement firstListElement{};

struct LinkedFancoilListElement *getLastListElement() {
    LinkedFancoilListElement *current = &firstListElement;

    if (current == nullptr) return nullptr;

    uint8_t walked = 0;
    while (current->next != nullptr && ++walked <= MAX_FANCOIL_LIST_WALK) {
        current = current->next;
    }
    return current;
}

void clearFancoils() {
    LinkedFancoilListElement *current = getLastListElement();
    debugPrintln("clearing fancoils");
    DEBUG_SERIAL.flush();
    // current now is the last element
    // now we iterate back over the list and clear everything
    uint8_t walked = 0;
    while (current != &firstListElement && ++walked <= MAX_FANCOIL_LIST_WALK) {
        if (current->fancoil != nullptr) free(current->fancoil);
        current = current->prev;
        free(current->next);
    }
    debugPrintln("we are at last fancoil");
    DEBUG_SERIAL.flush();
    // handle first element
    if (current->fancoil != nullptr) free(current->fancoil);
    current->fancoil = nullptr;
    current->next = nullptr;
    current->prev = nullptr; // should never have been anything else, but anyway...
}

bool addFancoil(uint8_t address) {
    if (getFancoilByAddress(address) == nullptr) {
        LinkedFancoilListElement *lastEntry = getLastListElement();

        LinkedFancoilListElement *newEntry;

        // populate first element if not used yet
        if (lastEntry->fancoil == nullptr) newEntry = lastEntry;
        else newEntry = (LinkedFancoilListElement *) calloc(1, sizeof(LinkedFancoilListElement));

        if (newEntry == nullptr) {
            return false;
        }
        Fancoil *newFancoil = (Fancoil *) calloc(1, sizeof(Fancoil));
        if (newFancoil == nullptr) {
            // only free what we allocated: when populating the first element,
            // newEntry aliases the STATIC firstListElement - free()ing it
            // corrupts the heap (found by Chris, 2026-08-29)
            if (newEntry != lastEntry) free(newEntry);
            return false;
        }
        newFancoil->init(address);

        // only if we are not populating the first element
        if (lastEntry != newEntry) {
            lastEntry->next = newEntry;
            newEntry->prev = lastEntry;
        }
        newEntry->fancoil = newFancoil;
        return true;
    }
    return false;
}

void loadFancoils() {
    clearFancoils();

    // fancoil addresses begin with addr 100 and go up to 199;
    // first, lets check if we have written the data.
    // 99 should be FC (for fancoil ;-))
    uint8_t check = EEPROM.read(FANCOIL_EEPROM_START_ADDRESS);
    if (check != 0xFC) {
        debugPrintln("need to initialize EEPROM");
        debugPrint("address begin is not 0xFC: ");
        debugPrintln(check);
        EEPROM.write(FANCOIL_EEPROM_START_ADDRESS, 0xFC);
        for (int i = FANCOIL_EEPROM_START_ADDRESS + 1;
             i < FANCOIL_EEPROM_START_ADDRESS + FANCOIL_EEPROM_LENGTH + 1; i++) {
            EEPROM.write(i, 0);
        }
        EEPROM.commit();
        debugPrintln("EEPROM initialized");

        if (EEPROM.read(FANCOIL_EEPROM_START_ADDRESS) != 0xFC) {
            debugPrintln("write failed");
        }
    } else {
        debugPrintln("EEPROM already initialized");
    }
    uint8_t address = 0;
    uint8_t count = 0;

    for (int i = FANCOIL_EEPROM_START_ADDRESS + 1; i < FANCOIL_EEPROM_START_ADDRESS + FANCOIL_EEPROM_LENGTH + 1; i++) {
        address = EEPROM.read(i);
        if (address != 0) {
            count++;
            if (addFancoil(address)) {
                debugPrint("Added fancoil ");
                debugPrintln(address);
            } else {
                debugPrint("Failed to add fancoil ");
                debugPrintln(address);
            }
        }
    }
    debugPrint("Loaded ");
    debugPrint(count);
    debugPrintln(" Fancoils");

#ifdef MQTT_HOST
    sendHomeAssistantConfiguration();
#endif
}

bool registerFancoil(uint8_t registerAddress) {
    uint8_t tmpAddress;
    if (getFancoilByAddress(registerAddress) == nullptr) {
        uint8_t check = EEPROM.read(FANCOIL_EEPROM_START_ADDRESS);
        if (check != 0xFC) {
            debugPrintln("EEPROM uninitialized");
            return false;
        }
        debugPrint("registering new fancoil: ");
        debugPrintln(registerAddress);

        for (int i = FANCOIL_EEPROM_START_ADDRESS + 1;
             i < FANCOIL_EEPROM_START_ADDRESS + FANCOIL_EEPROM_LENGTH + 1; i++) {
            tmpAddress = EEPROM.read(i);
            if (tmpAddress == 0) {
                // use this address
                EEPROM.write(i, registerAddress);
                EEPROM.commit();

                loadFancoils();
                return true;
            }
        }
        return false;
    } else {
        debugPrint("fanregistering new fancoil: ");
        return false;
    }
}

bool unregisterFancoil(uint8_t unregisterAddress) {
    uint8_t tmpAddress;
    if (getFancoilByAddress(unregisterAddress) != nullptr) {
        uint8_t check = EEPROM.read(FANCOIL_EEPROM_START_ADDRESS);
        if (check != 0xFC) {
            debugPrintln("EEPROM uninitialized");
            return false;
        }

        for (int i = FANCOIL_EEPROM_START_ADDRESS + 1;
             i < FANCOIL_EEPROM_START_ADDRESS + FANCOIL_EEPROM_LENGTH + 1; i++) {
            tmpAddress = EEPROM.read(i);
            if (tmpAddress == unregisterAddress) {
                // use this address
                EEPROM.write(i, 0x0);
                EEPROM.commit();

                debugPrint("unregistered address ");
                debugPrintln(unregisterAddress);

                loadFancoils();
                return true;
            }
        }
        return false;
    } else {
        return false;
    }
}

void setupFancoilManager() {
    EEPROM.begin(1024);
    loadFancoils();
}

Fancoil *getFancoilByAddress(uint8_t addr) {
    LinkedFancoilListElement *current = &firstListElement;

    uint8_t walked = 0;
    do {
        if (++walked > MAX_FANCOIL_LIST_WALK) return nullptr;
        if (current != nullptr && current->fancoil != nullptr && current->fancoil->getAddress() == addr) {
            return current->fancoil;
        }
        current = current->next;
    } while (current != nullptr);
    return nullptr;
}

struct LinkedFancoilListElement *getFirstFancoilListElement() {
    return &firstListElement;
}

unsigned long lastFancoilManagerRun = 0;

void loopFancoils(Stream *stream) {
    // max twice per second
    if (millis() - lastFancoilManagerRun > 500) {
        lastFancoilManagerRun = millis();
        LinkedFancoilListElement *listElement = getFirstFancoilListElement();
        uint8_t walked = 0;
        do {
            if (++walked > MAX_FANCOIL_LIST_WALK) {
                // more elements than fancoils can exist = the list is
                // corrupted (cycle). A reboot heals it: RAM is rebuilt
                // from EEPROM. Without this, a yielding cycle freezes the
                // device silently with all watchdogs fed.
                debugPrintln("fancoil list corrupted (cycle), restarting");
                ESP.restart();
            }
            if (listElement->fancoil != nullptr) listElement->fancoil->loop(stream);
        } while ((listElement = listElement->next) != nullptr);
    }
}
