//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_LOGGING_H
#define FANCOIL_CTRL_LOGGING_H

#include "Arduino.h"
#include "configuration.h"
#include "board_config.h"

// On BOARD_V2 modbus owns UART0, so the console moves to UART1 - GPIO2,
// transmit-only, brought out on the Dbg header. Connect only an adapter's RX
// there: anything that drives GPIO2 low at reset stops the board booting.
#if defined(LOG_ON_SERIAL1)
#define DEBUG_SERIAL Serial1
#else
#define DEBUG_SERIAL Serial
#endif


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
