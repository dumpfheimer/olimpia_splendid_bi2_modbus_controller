//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_LOGGING_H
#define FANCOIL_CTRL_LOGGING_H

#include "Arduino.h"

//#define USE_LOGGING
#define DEBUG_SERIAL Serial


void setupLogging();
void debugPrint(String s);
void debugPrint(float f);
void debugPrint(unsigned long l, int conf);
void debugPrintln(String s);
void debugPrintln(float f);
void debugPrintln(unsigned long l, int conf);
void debugPrintln();
bool useLogging();
Stream* getLogger();

#endif //FANCOIL_CTRL_LOGGING_H
