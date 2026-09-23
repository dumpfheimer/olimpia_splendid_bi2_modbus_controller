#include "modbus_ascii.h"

#include <new>

#define INCOMING_MESSAGE_BUFFER_SIZE 40

void (*modbusYieldCallback)(void) = nullptr;

void setModbusYieldCallback(void (*callback)(void)) {
    modbusYieldCallback = callback;
}

unsigned long lastMessageAt = 0;
unsigned long messageQuietTime = 25; // milliseconds between messages
unsigned long readTimeout = 500;
unsigned long modbusReadErrors = 0;
unsigned long modbusReadCount = 0;

IncomingMessage* incomingMessage;

// global bus lock: true while a transaction owns the RS485 bus.
// Because the modbus wait loop yields to the web server, an HTTP handler can
// re-enter the bus mid-transaction; this flag makes such a re-entrant call
// bounce instead of interleaving frames or clobbering the shared
// incomingMessage buffer.
volatile bool modbusBusy = false;
// sentinel returned to a re-entrant caller: always invalid, and never written
// to by modbusRead, so bouncing cannot corrupt the suspended outer
// transaction's buffer.
static IncomingMessage busyMessage;

bool IncomingMessage::crcIsValid() {
    uint16_t checksum = 0;
    checksum += address;
    checksum += functionCode;

    for (int i = 0; i < dataLength; i++) {
        checksum += data[i];
    }
    checksum = checksum & 0b11111111;
    checksum = 0xFF - checksum;
    uint8_t calculatedCRC = checksum + 1;

    if (crc == calculatedCRC) {
        return true;
    } else {
        /*debugPrint("CRC mismatch: expected ");
        debugPrint(calculatedCRC);
        debugPrint("/");
        debugPrint(calculatedCRC, 16);
        debugPrint(" got ");
        debugPrint(crc);
        debugPrint("/");
        debugPrint(crc, 16);
        debugPrint(" len=");
        debugPrintln(dataLength);*/
        return false;
    }
}

bool IncomingMessage::success() {
    return valid && !isError && crcIsValid();
}

double IncomingMessage::toTemperature() {
    uint16_t tmp = data[1] << 8 | data[2];
    return tmp;
}

char readBuffer[INCOMING_MESSAGE_BUFFER_SIZE * 2]{0};

void setupModbus() {
    // static storage: modbusRead dereferences this unconditionally, so a
    // failed heap allocation here used to mean a guaranteed crash at the
    // first transaction
    static IncomingMessage message;
    incomingMessage = &message;
}

void preTransmission() {
    digitalWrite(READ_ENABLE_PIN, 1);
    digitalWrite(DRIVER_ENABLE_PIN, 1);
}

void postTransmission() {
    digitalWrite(DRIVER_ENABLE_PIN, 0);
}

void preReceive() {
    digitalWrite(DRIVER_ENABLE_PIN, 0);
    digitalWrite(READ_ENABLE_PIN, 0);
}

void postReceive() {
    digitalWrite(READ_ENABLE_PIN, 1);
}

byte convertHexStringToByte(char char1, char char2) {
    // Validate that the characters are valid hex digits
    if (!isxdigit(char1) || !isxdigit(char2)) {
        debugPrintln("Invalid hex character");
        return 0; // Return 0 for invalid input
    }
    
    char buff[3];
    buff[0] = char1;
    buff[1] = char2;
    buff[2] = 0;
    uint16_t l = strtoul(buff, nullptr, 16);
    return l & 0xFF; // Ensure we only return a byte
}

void printByte(byte b, Stream *stream) {
    stream->print(b >> 4, HEX);
    stream->print(b & 0xF, HEX);
}

// total time budget for one response, no matter what arrives on the wire. A
// complete valid frame takes well under a second; without this cap, line
// noise arriving in sub-500ms intervals keeps the byte loop alive forever,
// starving the WiFi/MQTT servicing that only runs between transactions.
unsigned long transactionDeadline = 2000;

unsigned long lingerAfterResponse = 0;
unsigned long modbusCollisionSuspicions = 0;

// after a complete response, optionally keep the receiver open to catch a
// second unit answering the same request a few ms later (address collision).
// Only active during collision probes (lingerAfterResponse > 0) so normal
// transactions pay no extra bus time.
static void lingerForStrays(Stream *stream) {
    if (lingerAfterResponse == 0) return;
    unsigned long lingerStart = millis();
    uint16_t strays = 0;
    while ((millis() - lingerStart) < lingerAfterResponse) {
        if (stream->available()) {
            stream->read();
            strays++;
        }
        yield();
    }
    if (strays > 0) modbusCollisionSuspicions++;
}

IncomingMessage *modbusRead(Stream *stream) {
    preReceive();
    modbusReadCount++;
    // the shared buffer still holds the previous transaction's result;
    // invalidate it up front so no abort path (deadline, buffer overflow,
    // short frame) can hand stale data to the caller as a fresh success
    incomingMessage->valid = false;
    unsigned long transactionStart = millis();
    long start = millis();
    while ((millis() - start) < readTimeout) {
        if (stream->available()) break;
        lastMessageAt = millis();
        if (modbusYieldCallback) modbusYieldCallback();
        yield();
    }

    if (stream->available()) {
        uint16_t readBufferPos = 0;
        while (stream->available()) {
            if ((millis() - transactionStart) > transactionDeadline) {
                debugPrintln("transaction deadline exceeded");
                postReceive();
                modbusReadErrors++;
                return incomingMessage;
            }
            lastMessageAt = millis();
            readBuffer[readBufferPos] = stream->read();
            if (readBuffer[readBufferPos] == ':') {
                //debugPrint("received (crap) ");
                //debugPrintln(readBuffer[readBufferPos]);
            } else {
                //debugPrint("received ");
                //debugPrintln(readBuffer[readBufferPos]);
                if (readBufferPos > 0 && readBuffer[readBufferPos - 1] == '\r' && readBuffer[readBufferPos] == '\n') {
                    debugPrintln("got complete message");
                    //convert to binary, calculate and check CRC
                    if (readBufferPos < 2) {
                        debugPrintln("message too short");
                        postReceive();
                        modbusReadErrors++;
                        return incomingMessage;
                    }
                    uint16_t dataPos = 0;
                    for (uint16_t convertPos = 0; convertPos < readBufferPos - 1 /* before \r */; convertPos += 2) {
                        // Make sure we don't go out of bounds in readBuffer
                        if (convertPos + 1 >= INCOMING_MESSAGE_BUFFER_SIZE * 2) {
                            debugPrintln("readBuffer index out of bounds");
                            break;
                        }
                        
                        uint8_t byteValue = convertHexStringToByte(readBuffer[convertPos], readBuffer[convertPos + 1]);
                        if (convertPos == 0) {
                            incomingMessage->address = byteValue;
                        } else if (convertPos == 2) {
                            incomingMessage->functionCode = byteValue;
                        } else {
                            // Make sure we don't go out of bounds in data array
                            if (dataPos < INCOMING_MESSAGE_BUFFER_SIZE) {
                                incomingMessage->data[dataPos] = byteValue;
                                dataPos++;
                            } else {
                                debugPrintln("data array index out of bounds");
                                break;
                            }
                        }
                    }
                    if (dataPos == 0) {
                        debugPrintln("message has no data/crc byte");
                        incomingMessage->valid = false;
                        postReceive();
                        modbusReadErrors++;
                        return incomingMessage;
                    }
                    incomingMessage->crc = incomingMessage->data[dataPos - 1];
                    incomingMessage->data[dataPos - 1] = 0;
                    incomingMessage->dataLength = dataPos - 1;


                    debugPrint("received ");
                    debugPrint(incomingMessage->address, HEX);
                    debugPrint(" ");
                    debugPrint(incomingMessage->functionCode, HEX);
                    debugPrint(" ");
                    for (int i = 0; i < incomingMessage->dataLength; i++) {
                        debugPrint(incomingMessage->data[i], HEX);
                        debugPrint(",");
                    }
                    debugPrint("= ");
                    debugPrintln(incomingMessage->crc, HEX);

                    if (!incomingMessage->crcIsValid()) {
                        debugPrintln("CRC is invalid");
                        incomingMessage->valid = false;
                        lingerForStrays(stream);
                        postReceive();
                        modbusReadErrors++;
                        return incomingMessage;
                    }
                    incomingMessage->valid = true;
                    if ((incomingMessage->functionCode & 0b10000000) > 0) {
                        debugPrintln("function code has error bit set");
                        incomingMessage->isError = true;
                    } else {
                        incomingMessage->isError = false;
                    }
                    lingerForStrays(stream);
                    postReceive();
                    return incomingMessage;
                }
                readBufferPos++;
                if (readBufferPos >= INCOMING_MESSAGE_BUFFER_SIZE) {
                    debugPrintln("buffer overflow");
                    break;
                }
            }
            start = millis();
            while ((millis() - start) < readTimeout) {
                if (modbusYieldCallback) modbusYieldCallback();
                yield();
                if (stream->available()) break;
            }
        }
        //debugPrintln("end");
        //debugPrintln(readBuffer);
        lastMessageAt = millis();
        postReceive();
        modbusReadErrors++;
        return incomingMessage;
    } else {
        debugPrintln("No begin sign");
        incomingMessage->valid = false;
        postReceive();
        modbusReadErrors++;
        return incomingMessage;
    }
}

IncomingMessage *modbusWrite(Stream *stream, byte address, byte functionCode, byte binaryMsg[], uint8_t length) {
    if (modbusBusy) {
        // the bus is already owned by a transaction higher up the call stack
        // (we reached here re-entrantly via the modbus yield callback); bounce
        // immediately rather than interleaving on the wire.
        return &busyMessage;
    }
    modbusBusy = true;

    uint16_t checksum = 0;
    checksum += address;
    checksum += functionCode;

    for (int i = 0; i < length; i++) {
        checksum += binaryMsg[i];
    }

    checksum = checksum & 0b11111111;
    checksum = 0xFF - checksum;
    uint8_t crc = checksum + 1;

    while ((millis() - lastMessageAt) < messageQuietTime) yield();
    lastMessageAt = millis();

    // discard unconsumed bytes from an earlier transaction (e.g. a response
    // that arrived after its read had already timed out) so they cannot be
    // parsed as this transaction's response
    while (stream->available()) stream->read();

    preTransmission();

    // begin message
    stream->print(":");

    printByte(address, stream);
    printByte(functionCode, stream);

    for (int i = 0; i < length; i++) {
        printByte(binaryMsg[i], stream);
    }

    printByte(crc, stream);

    stream->write(0x0D);
    stream->write(0x0A);

    stream->flush();

    postTransmission();

    debugPrint(":");
    printByte(address, &DEBUG_SERIAL);
    printByte(functionCode, &DEBUG_SERIAL);
    for (int i = 0; i < length; i++) {
        printByte(binaryMsg[i], &DEBUG_SERIAL);
    }
    printByte(crc, &DEBUG_SERIAL);
    DEBUG_SERIAL.write(0x0D);
    DEBUG_SERIAL.write(0x0A);
    lastMessageAt = millis(); // update inbetween

    debugPrintln("Waiting for response");
    IncomingMessage *result = modbusRead(stream);

    // A frame carries no reference to the request it answers; the only thing
    // tying a response to a request is timing. A delayed response to an
    // earlier request (same bus, different device or register) passes the LRC
    // check and would be parsed as this transaction's data, so reject any
    // response whose envelope does not match what we just asked.
    if (result->valid) {
        if (result->address != address || (result->functionCode & 0b01111111) != functionCode) {
            debugPrintln("response envelope does not match request, discarding");
            result->valid = false;
            modbusReadErrors++;
        } else if (!result->isError && length == 4) {
            if (functionCode == 0x03) {
                // response to "read n registers" must carry exactly 2n data
                // bytes, announced in its first byte
                uint8_t expectedByteCount = binaryMsg[3] * 2;
                if (result->data[0] != expectedByteCount || result->dataLength != expectedByteCount + 1) {
                    debugPrintln("response length does not match request, discarding");
                    result->valid = false;
                    modbusReadErrors++;
                }
            } else if (functionCode == 0x06) {
                // response to "write register" echoes the register address
                if (result->dataLength != 4 || result->data[0] != binaryMsg[0] || result->data[1] != binaryMsg[1]) {
                    debugPrintln("write echo does not match request, discarding");
                    result->valid = false;
                    modbusReadErrors++;
                }
            }
        }
    }

    if (!result->valid) {
        // no valid message received
        debugPrintln("no valid message received");
    } else if (result->isError) {
        // device responded with error
        debugPrintln("error received");
    } else if (result->address == address && result->functionCode == functionCode) {
        debugPrintln("write success");
    } else {
        debugPrintln("error received");
    }
    modbusBusy = false;
    return result;
}

IncomingMessage *modbusReadRegisterI(Stream *stream, byte address, uint16_t registe, uint8_t count) {
    byte msg[4];

    uint16_t actualRegister = registe;
    msg[0] = (actualRegister >> 8) & 0xFF;
    msg[1] = actualRegister & 0xFF;
    msg[2] = 0x00; // read one register
    msg[3] = count;

    return modbusWrite(stream, address, 0x03, msg, 4);
}

IncomingMessage *modbusReadRegister(Stream *stream, byte address, uint16_t registe, uint8_t count, uint8_t retry) {
    IncomingMessage *i = modbusReadRegisterI(stream, address, registe, count);
    if (i->valid || i == &busyMessage) {
        // a busy bounce won't clear while we are above the lock holder on the
        // call stack, so retrying is pointless.
        return i;
    } else if (retry > 0) {
        return modbusReadRegister(stream, address, registe, count, retry - 1);
    } else {
        return i;
    }
}

IncomingMessage *modbusReadRegister(Stream *stream, byte address, uint16_t registe, uint8_t count) {
    return modbusReadRegister(stream, address, registe, count, 1);
}

IncomingMessage *modbusReadRegister(Stream *stream, byte address, uint16_t registe) {
    return modbusReadRegister(stream, address, registe, 1, 1);
}


IncomingMessage *modbusWriteRegisterI(Stream *stream, byte address, uint16_t registe, uint16_t data) {
    byte msg[4];

    uint16_t actualRegister = registe;
    msg[0] = (actualRegister >> 8) & 0xFF;
    msg[1] = actualRegister & 0xFF;
    msg[2] = (data >> 8) & 0xFF;
    msg[3] = data & 0xFF;

    return modbusWrite(stream, address, 0x06, msg, 4);
}

IncomingMessage *modbusWriteRegister(Stream *stream, byte address, uint16_t registe, uint16_t data, uint8_t retry) {
    IncomingMessage *i = modbusWriteRegisterI(stream, address, registe, data);
    if (i->valid || i == &busyMessage) {
        // a busy bounce won't clear while we are above the lock holder on the
        // call stack, so retrying is pointless.
        return i;
    } else if (retry > 0) {
        return modbusWriteRegister(stream, address, registe, data, retry - 1);
    } else {
        return i;
    }
}

IncomingMessage *modbusWriteRegister(Stream *stream, byte address, uint16_t registe, uint16_t data) {
    return modbusWriteRegister(stream, address, registe, data, 1);
}
