#define INCOMING_MESSAGE_BUFFER_SIZE 40

unsigned long lastMessageAt = 0;
unsigned long messageQuietTime = 25; // milliseconds between messages
unsigned long readTimeout = 500;
unsigned long modbusReadErrors = 0;
unsigned long modbusReadCount = 0;

class IncomingMessage {
  public:
  byte address = 0;
  byte functionCode = 0;
  bool valid = false;
  byte crc = 0;
  
  bool isError = false;
  byte data[INCOMING_MESSAGE_BUFFER_SIZE];
  uint8_t dataLength = 0;

  bool crcIsValid() {
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
  bool success() {
    return valid && !isError && crcIsValid();
  }

  double toTemperature() {
    uint16_t tmp = data[1] << 8 | data[2];
    return tmp;
  }
};

char readBuffer[INCOMING_MESSAGE_BUFFER_SIZE * 2];

void preTransmission()
{
  digitalWrite(READ_ENABLE_PIN, 1);
  digitalWrite(DRIVER_ENABLE_PIN, 1);
}

void postTransmission()
{
  digitalWrite(DRIVER_ENABLE_PIN, 0);
}

void preReceive()
{
  digitalWrite(DRIVER_ENABLE_PIN, 0);
  digitalWrite(READ_ENABLE_PIN, 0);
}

void postReceive()
{
  digitalWrite(READ_ENABLE_PIN, 1);
}

byte convertHexStringToByte(char char1, char char2) {
  char buff[3];
  buff[0] = char1;
  buff[1] = char2;
  buff[2] = 0;
  uint16_t l = strtoul(buff, NULL, 16);
  return l;
}

void printByte(byte b, Stream *stream) {
  stream->print(b >> 4, HEX);
  stream->print(b & 0xF, HEX);
}

IncomingMessage modbusRead(Stream *stream) {
  preReceive();
  modbusReadCount++;
  IncomingMessage ret;
  char c = 0;
  long start = millis();
  while ((millis() - start) < readTimeout) {
    if (stream->available()) break;
    lastMessageAt = millis();
    yield();
  }

  if (stream->available()) {
    int readBufferPos = 0;
    while (stream->available()) {
      lastMessageAt = millis();
      readBuffer[readBufferPos] = stream->read();
      if (readBuffer[readBufferPos] == ':') {
        //debugPrint("received (crap) ");
        //debugPrintln(readBuffer[readBufferPos]);
      } else {
        //debugPrint("received ");
        //debugPrintln(readBuffer[readBufferPos]);
        if (readBufferPos > 0 && readBuffer[readBufferPos-1] == '\r' && readBuffer[readBufferPos] == '\n') {
          debugPrintln("got complete message");
          //convert to binary, calculate and check CRC
          if (readBufferPos < 2) {
            debugPrintln("message too short");
            postReceive();
            modbusReadErrors++;
            return ret;
          }
          int dataPos = 0;
          for (uint8_t convertPos = 0; convertPos < readBufferPos - 1 /* before \r */; convertPos += 2) {
            char *limit = &(readBuffer[convertPos + 2]);
            uint8_t byteValue = convertHexStringToByte(readBuffer[convertPos], readBuffer[convertPos + 1]);
            if (convertPos == 0) {
              ret.address = byteValue;
            } else if (convertPos == 2) {
              ret.functionCode = byteValue;
            } else {
              ret.data[dataPos] = byteValue;
              dataPos++;
            }
          }
          ret.crc = ret.data[dataPos - 1];
          ret.data[dataPos - 1] = 0;
          ret.dataLength = dataPos - 1;
  
  
          debugPrint("received ");
          debugPrint(ret.address, HEX);
          debugPrint(" ");
          debugPrint(ret.functionCode, HEX);
          debugPrint(" ");
          for (int i = 0; i < ret.dataLength; i++) {
            debugPrint(ret.data[i], HEX);
            debugPrint(",");
          }
          debugPrint("= ");
          debugPrintln(ret.crc, HEX);
  
          if (!ret.crcIsValid()) {
            debugPrintln("CRC is invalid");
            ret.valid = false;
            postReceive();
            modbusReadErrors++;
            return ret;
          }
          ret.valid = true;
          if ((ret.functionCode & 0b10000000) > 0) {
            debugPrintln("function code has error bit set");
            ret.isError = true;
          } else {
            ret.isError = false;
          }
          postReceive();
          return ret;
        }
        readBufferPos++;
        if (readBufferPos >= INCOMING_MESSAGE_BUFFER_SIZE) {
          debugPrintln("buffer overflow");
          break;
        }
      }
      start = millis();
      while ((millis() - start) < readTimeout) {
        yield();
        if (stream->available()) break;
      }
    }
    //debugPrintln("end");
    //debugPrintln(readBuffer);
    lastMessageAt = millis();
    postReceive();
    modbusReadErrors++;
    return ret;
  } else {
    debugPrintln("No begin sign");
    ret.valid = false;
    postReceive();
    modbusReadErrors++;
    return ret;
  }
}
IncomingMessage modbusWrite(Stream *stream, byte address, byte functionCode, byte binaryMsg[], uint8_t length) {
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
  IncomingMessage result = modbusRead(stream);
  if (!result.valid) {
    // no valid message received
    debugPrintln("no valid message received");
  } else if (result.isError) {
    // device responded with error
    debugPrintln("error received");
  } else if (result.address == address && result.functionCode == functionCode) {
    debugPrintln("write success");
  } else {
    debugPrintln("error received");
  }
  return result;
}

IncomingMessage modbusReadRegisterI(Stream *stream, byte address, uint16_t registe, uint8_t count) {
  byte msg[4];

  uint16_t actualRegister = registe;
  msg[0] = (actualRegister >> 8) & 0xFF;
  msg[1] = actualRegister & 0xFF;
  msg[2] = 0x00; // read one register
  msg[3] = count;

  return modbusWrite(stream, address, 0x03, msg, 4);
}

IncomingMessage modbusReadRegister(Stream *stream, byte address, uint16_t registe, uint8_t count, uint8_t retry) {
  IncomingMessage i = modbusReadRegisterI(stream, address, registe, count);
  if (i.valid) {
    return i;
  } else if (retry > 0) {
    return modbusReadRegister(stream, address, registe, count, retry - 1);
  } else {
    return i;
  }
}

IncomingMessage modbusReadRegister(Stream *stream, byte address, uint16_t registe, uint8_t count) {
  return modbusReadRegister(stream, address, registe, count, 1);
}

IncomingMessage modbusReadRegister(Stream *stream, byte address, uint16_t registe) {
  return modbusReadRegister(stream, address, registe, 1, 1);
}


IncomingMessage modbusWriteRegisterI(Stream *stream, byte address, uint16_t registe, uint16_t data) {
  byte msg[4];
  
  uint16_t actualRegister = registe;
  msg[0] = (actualRegister >> 8) & 0xFF;
  msg[1] = actualRegister & 0xFF;
  msg[2] = (data >> 8) & 0xFF;
  msg[3] = data & 0xFF;

  return modbusWrite(stream, address, 0x06, msg, 4);
}

IncomingMessage modbusWriteRegister(Stream *stream, byte address, uint16_t registe, uint16_t data, uint8_t retry) {
  IncomingMessage i = modbusWriteRegisterI(stream, address, registe, data);
  if (i.valid) {
    return i;
  } else if (retry > 0) {
    return modbusWriteRegister(stream, address, registe, data, retry - 1);
  } else {
    return i;
  }
}

IncomingMessage modbusWriteRegister(Stream *stream, byte address, uint16_t registe, uint16_t data) {
  return modbusWriteRegister(stream, address, registe, data, 1);
}
