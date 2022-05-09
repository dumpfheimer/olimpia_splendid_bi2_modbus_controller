#include <EEPROM.h>

#define FANCOIL_EEPROM_START_ADDRESS 0
#define FANCOIL_EEPROM_LENGTH 32

struct LinkedFancoilListElement {
  Fancoil *fancoil;
  LinkedFancoilListElement *next;
  LinkedFancoilListElement *prev;
};
typedef LinkedFancoilListElement LinkedFancoilListElement;

LinkedFancoilListElement firstListElement;

struct LinkedFancoilListElement* getLastListElement() {
  LinkedFancoilListElement *current;
  current = &firstListElement;
  
  while (current->next != NULL) {
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
  while (current != &firstListElement) {
    if (current->fancoil != NULL) free(current->fancoil);
    current = current->prev;
    free(current->next);
  }
  debugPrintln("we are at last fancoil");
  DEBUG_SERIAL.flush();
  // handle first element
  if (current->fancoil != NULL) free(current->fancoil);
  current->fancoil = NULL;
  current->next = NULL;
  current->prev = NULL; // should never have been anything else, but anyway...
}

bool addFancoil(uint8_t address) {
  if (getFancoilByAddress(address) == NULL) {
    LinkedFancoilListElement *lastEntry = getLastListElement();

    LinkedFancoilListElement *newEntry;
    
    // populate first element if not used yet
    if (lastEntry->fancoil == NULL) newEntry = lastEntry;
    else newEntry = (LinkedFancoilListElement*) calloc(1, sizeof(LinkedFancoilListElement));
    
    if (newEntry == NULL) {
      return false;
    }
    Fancoil *newFancoil = (Fancoil*) calloc(1, sizeof(Fancoil));
    if (newFancoil == NULL) {
      free(newEntry);
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
    for (int i = FANCOIL_EEPROM_START_ADDRESS + 1; i <= FANCOIL_EEPROM_START_ADDRESS + FANCOIL_EEPROM_LENGTH + 1; i++) {
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
  if (getFancoilByAddress(registerAddress) == NULL) {
    uint8_t check = EEPROM.read(FANCOIL_EEPROM_START_ADDRESS);
    if (check != 0xFC) {
      debugPrintln("EEPROM uninitialized");
      return false;
    }
    debugPrint("registering new fancoil: ");
    debugPrintln(registerAddress);
    
    for (int i = FANCOIL_EEPROM_START_ADDRESS + 1; i < FANCOIL_EEPROM_START_ADDRESS + FANCOIL_EEPROM_LENGTH + 1; i++) {
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
  if (getFancoilByAddress(unregisterAddress) != NULL) {
    uint8_t check = EEPROM.read(FANCOIL_EEPROM_START_ADDRESS);
    if (check != 0xFC) {
      debugPrintln("EEPROM uninitialized");
      return false;
    }
    
    for (int i = FANCOIL_EEPROM_START_ADDRESS + 1; i < FANCOIL_EEPROM_START_ADDRESS + FANCOIL_EEPROM_LENGTH + 1; i++) {
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
  EEPROM.begin(512);
  loadFancoils();
}

Fancoil* getFancoilByAddress(uint8_t addr) {
  LinkedFancoilListElement *current = &firstListElement;

  do {
    if (current->fancoil != NULL && current->fancoil->getAddress() == addr) {
      return current->fancoil;
    }
    current = current->next;
  } while(current != NULL);
  return NULL;
}
struct LinkedFancoilListElement* getFirstFancoilListElement() {
  return &firstListElement;
}
unsigned long lastFancoilManagerRun = 0;
void loopFancoils(Stream *stream) {
  // max twice per second
  if (millis() - lastFancoilManagerRun > 500) {
    lastFancoilManagerRun = millis();
    LinkedFancoilListElement *listElement = getFirstFancoilListElement();
    do {
      if (listElement->fancoil != NULL) listElement->fancoil->loop(stream);
    } while ((listElement = listElement->next) != NULL);
  }
}
