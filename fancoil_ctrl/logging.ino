#define USE_LOGGING


void setupLogging() {
  #ifdef USE_LOGGING
    DEBUG_SERIAL.begin(115200);
    DEBUG_SERIAL.println("Logging enabled");
  #endif
}

void debugPrint(String s) {
  #ifdef USE_LOGGING
    DEBUG_SERIAL.print(s);
  #endif
}

void debugPrint(float f) {
  #ifdef USE_LOGGING
    DEBUG_SERIAL.print(f);
  #endif
}

void debugPrint(unsigned long l, int conf) {
  #ifdef USE_LOGGING
    DEBUG_SERIAL.print(l, conf);
  #endif
}


void debugPrintln(String s) {
  #ifdef USE_LOGGING
    DEBUG_SERIAL.println(s);
  #endif
}


void debugPrintln(float f) {
  #ifdef USE_LOGGING
    DEBUG_SERIAL.println(f);
  #endif
}
void debugPrintln(unsigned long l, int conf) {
  #ifdef USE_LOGGING
    DEBUG_SERIAL.println(l, conf);
  #endif
}


void debugPrintln() {
  #ifdef USE_LOGGING
    DEBUG_SERIAL.println();
  #endif
}

#ifdef USE_LOGGING
bool useLogging() {
  return true;
}

Stream* getLogger() {
  return &DEBUG_SERIAL;
}
#else
bool useLogging() {
  return false;
}

Stream* getLogger() {
  return NULL;
}
#endif
