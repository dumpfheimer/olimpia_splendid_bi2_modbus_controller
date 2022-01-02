#define MODBUS_ASCII 1
#define MODBUS_RTU 0

#if MODBUS_ASCII
#include "modbus_ascii.h"
#endif
#if MODBUS_RTU
#include "modbus_rtu.h"
#endif
