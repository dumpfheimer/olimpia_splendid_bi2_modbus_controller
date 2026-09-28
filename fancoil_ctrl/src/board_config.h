//
// Board revision selection.
//
// Three physically different wirings are in service at once. Flashing the
// wrong one is not a cosmetic mistake: it leaves DE on an unconfigured pin
// (driver keyed permanently, bus jammed), reads RX from a pin with no wire,
// or puts a transceiver output back onto a boot-strap pin - which turns an
// ordinary crash into a board that sits dark in UART-download mode until it
// is physically power cycled.
//
// So the revision must be chosen explicitly by the build environment; there
// is deliberately no default. platformio.ini defines exactly one of:
//
//   BOARD_V1          original wiring, A/B swapped (needs inverted serial)
//   BOARD_V1_REWIRED  11.61: RX/DE/RE moved off the strap pins, A/B corrected
//   BOARD_V2          the 2026-09 PCB: hardware UART, logging on Serial1
//
// BOARD_NAME is reported by /uptime so a device can be asked what it is
// actually running rather than assumed.
//
#ifndef FANCOIL_CTRL_BOARD_CONFIG_H
#define FANCOIL_CTRL_BOARD_CONFIG_H

#if defined(ESP8266)

#if (defined(BOARD_V1) + defined(BOARD_V1_REWIRED) + defined(BOARD_V2)) != 1
#error "Define exactly one board revision: BOARD_V1, BOARD_V1_REWIRED or BOARD_V2 (see platformio.ini)"
#endif


#if defined(BOARD_V1)
// Original wiring. DE sits on GPIO0 and RX on GPIO2 - both boot-strap pins.
// Tolerable only because the transceiver module's 10k pull-ups to 5V hold
// them high at reset; do not add a pull-down or a divider to either.
#define BOARD_NAME              "v1"
#define MODBUS_TX_PIN           D1  // GPIO5  -> DI
#define MODBUS_RX_PIN           D4  // GPIO2  <- RO   (strap pin, see above)
#define DRIVER_ENABLE_PIN       D3  // GPIO0  -> DE   (strap pin, see above)
#define READ_ENABLE_PIN         D2  // GPIO4  -> RE
#define MODBUS_SOFTWARE_SERIAL  1
#define MODBUS_INVERT           true   // A/B swapped at the terminal

#elif defined(BOARD_V1_REWIRED)
// 11.61: RO moved off GPIO2 (divider fitted) and DE off GPIO0, so a reset can
// no longer land in UART-download mode. A/B corrected, so no inversion.
#define BOARD_NAME              "v1-rewired"
#define MODBUS_TX_PIN           D1  // GPIO5  -> DI
#define MODBUS_RX_PIN           D6  // GPIO12 <- RO via divider
#define DRIVER_ENABLE_PIN       D2  // GPIO4  -> DE
#define READ_ENABLE_PIN         D7  // GPIO13 -> RE
#define MODBUS_SOFTWARE_SERIAL  1
#define MODBUS_INVERT           false

#elif defined(BOARD_V2)
// 2026-09 PCB. Modbus runs on the hardware UART (no bit-banging, native 7E1),
// which means UART0 is no longer free for the console - logging moves to
// Serial1 on GPIO2, transmit-only, brought out on the Dbg header.
// UART0 is shared with the CH340C: while USB is plugged in, the USB chip also
// drives GPIO3. Harmless in practice because the transceiver's RE pull-up
// disables the receiver whenever the ESP is not driving it (reset, boot,
// bootloader), so RO is high-Z exactly when the USB chip is talking.
#define BOARD_NAME              "v2"
#define DRIVER_ENABLE_PIN       D7  // GPIO13 -> DE
#define READ_ENABLE_PIN         D6  // GPIO12 -> RE
#define MODBUS_HARDWARE_SERIAL  1
#define LOG_ON_SERIAL1          1
#endif

#elif defined(ESP32)
#define BOARD_NAME              "esp32"
#define DRIVER_ENABLE_PIN       D3
#define READ_ENABLE_PIN         D2
#define MODBUS_HARDWARE_SERIAL  1
#else
#error "This hardware is not supported"
#endif

#endif //FANCOIL_CTRL_BOARD_CONFIG_H
