#define INCOMING_MESSAGE_BUFFER_SIZE 40

uint16_t crc16_update(uint16_t crc, byte a) {
  int i;

  crc ^= (uint16_t)a;

  for (i = 0; i < 8; ++i) {
    /*crc >>= 1;
    if (crc & 1)
      crc = crc ^ 0xA001;
    else
      crc >>= 1;*/
    if (crc & 1)
      crc = (crc >>= 1) ^ 0xA001;
    else
      crc >>= 1;
  }

  return crc;
}
void debugByte(byte b) {
  debugPrint(b >> 4, HEX);
  debugPrint(b & 0xF, HEX);
  debugPrint(" ");
}

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
    uint16_t calc_crc = 0xFFFF;
    calc_crc = crc16_update(calc_crc, address);
    calc_crc = crc16_update(calc_crc, functionCode);
    
    for (int i = 0; i < dataLength; i++) {
      calc_crc = crc16_update(calc_crc, data[i]);
    }

    if (crc == calc_crc) {
      return true;
    } else {
      debugPrint("CRC mismatch: expected ");
      debugPrint(calc_crc);
      debugPrint("/");
      debugPrint(calc_crc, 16);
      debugPrint(" got ");
      debugPrint(crc);
      debugPrint("/");
      debugPrintln(crc, 16);
      return false;
    }
  }
  bool success() {
    return valid && !isError && crcIsValid();
  }

  double toTemperature() {
    uint16_t tmp = data[0] << 8 | data[1];
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
  digitalWrite(READ_ENABLE_PIN, 0);
  digitalWrite(DRIVER_ENABLE_PIN, 0);
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
  stream->write(b);
}
IncomingMessage modbusRead(Stream *stream) {
  IncomingMessage ret;

  long start = millis();
  while (!stream->available() && (millis() - start) < 300) {
    yield();
  }
  if (stream->available()) {
    int readBufferPos = 0;
    while (stream->available()) {
      readBuffer[readBufferPos] = stream->read();
      if (readBufferPos > 0 && readBuffer[readBufferPos-1] == '\r' && readBuffer[readBufferPos] == '\n') {
        debugPrintln("got complete message");
        //convert to binary, calculate and check CRC
        if (readBufferPos < 2) {
          debugPrintln("message too short");
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

/*
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
        debugPrintln(ret.crc, HEX);*/

        if (!ret.crcIsValid()) {
          debugPrintln("CRC is invalid");
          ret.valid = false;
          return ret;
        }
        ret.valid = true;
        if ((ret.functionCode & 0b10000000) > 0) {
          debugPrintln("function code has error bit set");
          ret.isError = true;
        } else {
          ret.isError = false;
        }
        return ret;
      }
      readBufferPos++;
      if (readBufferPos >= INCOMING_MESSAGE_BUFFER_SIZE) {
        debugPrintln("buffer overflow");
        break;
      }
      yield();
    }
    //debugPrintln("end");
    //debugPrintln(readBuffer);
    return ret;
  } else {
    debugPrintln("No begin sign");
    ret.valid = false;
    return ret;
  }
}

IncomingMessage modbusWrite(Stream *stream, byte address, byte functionCode, byte binaryMsg[], uint8_t length) {
  uint16_t calc_crc = 0xFFFF;
  calc_crc = crc16_update(calc_crc, address);
  calc_crc = crc16_update(calc_crc, functionCode);
  
  for (int i = 0; i < length; i++) {
    calc_crc = crc16_update(calc_crc, binaryMsg[i]);
  }

  preTransmission();
  
  // address
  printByte(address, stream);
  //if (address < 16) stream->print("0");
  //stream->print(address, HEX);

  // function code
  printByte(functionCode, stream);
  //if (functionCode < 16) stream->print("0");
  //stream->print(functionCode, HEX);

  // data
  for (int i = 0; i < length; i++) {
    printByte(binaryMsg[i], stream);
    //if (binaryMsg[i] < 16) stream->print("0");
    //stream->print(binaryMsg[i], HEX);
  }

  // crc
  printByte(calc_crc & 0xFF, stream);
  printByte(calc_crc >> 8, stream);

  stream->flush();

  debugByte(address);
  debugByte(functionCode);
  for (int i = 0; i < length; i++) {
    debugByte(binaryMsg[i]);
  }
  debugByte(calc_crc & 0xFF);
  debugByte(calc_crc >> 8);
  //delay(5);

  postTransmission();


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

IncomingMessage modbusReadRegister(Stream *stream, byte address, uint16_t registe, uint8_t count) {
  byte msg[4];

  uint16_t actualRegister = registe-1;
  msg[0] = (actualRegister >> 8) & 0xFF;
  msg[1] = actualRegister & 0xFF;
  msg[2] = 0x00; // read one register
  msg[3] = count;

  return modbusWrite(stream, address, 0x03, msg, 4);
}
IncomingMessage modbusReadRegister(Stream *stream, byte address, uint16_t registe) {
  return modbusReadRegister(stream, address, registe, 1);
}

IncomingMessage modbusWriteRegister(Stream *stream, byte address, uint16_t registe, uint16_t data) {
  byte msg[4];
  
  uint16_t actualRegister = registe-1;
  msg[0] = (actualRegister >> 8) & 0xFF;
  msg[1] = actualRegister & 0xFF;
  msg[2] = (data >> 8) & 0xFF;
  msg[3] = data & 0xFF;

  return modbusWrite(stream, address, 0x06, msg, 4);
}
