//
// Created by chris on 30.03.23.
//

#ifndef FANCOIL_CTRL_MODBUS_ASCII_H
#define FANCOIL_CTRL_MODBUS_ASCII_H


#include <Arduino.h>

#include "main.h"

#define INCOMING_MESSAGE_BUFFER_SIZE 40

extern unsigned long lastMessageAt;
extern unsigned long messageQuietTime; // milliseconds between messages
extern unsigned long readTimeout;
extern unsigned long modbusReadErrors;
extern unsigned long modbusReadCount;


class IncomingMessage {
public:
    byte address = 0;
    byte functionCode = 0;
    bool valid = false;
    byte crc = 0;

    bool isError = false;
    byte data[INCOMING_MESSAGE_BUFFER_SIZE];
    uint8_t dataLength = 0;

    bool crcIsValid();
    bool success();
    double toTemperature();
};

extern char readBuffer[];
IncomingMessage* incomingMessage;

void setupModbus();
void preTransmission();
void postTransmission();
void preReceive();
void postReceive();
byte convertHexStringToByte(char char1, char char2);
void printByte(byte b, Stream *stream);

IncomingMessage* modbusRead(Stream *stream);
IncomingMessage* modbusWrite(Stream *stream, byte address, byte functionCode, byte binaryMsg[], uint8_t length);
IncomingMessage* modbusReadRegisterI(Stream *stream, byte address, uint16_t registe, uint8_t count);
IncomingMessage* modbusReadRegister(Stream *stream, byte address, uint16_t registe, uint8_t count, uint8_t retry);
IncomingMessage* modbusReadRegister(Stream *stream, byte address, uint16_t registe, uint8_t count);
IncomingMessage* modbusReadRegister(Stream *stream, byte address, uint16_t registe);


IncomingMessage* modbusWriteRegisterI(Stream *stream, byte address, uint16_t registe, uint16_t data);
IncomingMessage* modbusWriteRegister(Stream *stream, byte address, uint16_t registe, uint16_t data, uint8_t retry);
IncomingMessage* modbusWriteRegister(Stream *stream, byte address, uint16_t registe, uint16_t data);


#endif //FANCOIL_CTRL_MODBUS_ASCII_H
