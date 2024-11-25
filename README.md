# Olimpia Splendid Bi2 Modbus Controller

## THIS IS WORK IN PROGRESS, PLEASE LET ME KNOW IF THINGS ARE NOT WORKING, BUT DO NOT BLAME ME FOR IT

### Temporary, crappy HowTo:

1. Configure, compile and flash Software (see [Software section](#SOFTWARE))
2. Wire up the modbus converter (see [Hardware section](#HARDWARE))
3. Connect the modbus devices
4. Put your Fan Coil into "Remote" Mode (check the manual, for me, I think, it was touching "-" and the fan "button" for 10s)
5. If needed, change the fan coil address
6. Register the fan coil address (see registering/unregistering)
7. Change Fancoil Settings over http://CONTROLLER_IP/set?on=1

### Controlling the fan coil over HTTP

HTTP examples:
/set?addr=1&ambient=22.9&setpoint=23.0&on=0&speed=MIN&swing=False&fanonly=False&mode=COOLING


HTTP params:
- addr: (0...31) modbus address of the fancoil
- ambient: (XX.X) temperature in deg C of the current room temperature
- setpoint: (XX.X) temperature in deg C of the desired setpoint
- on: (true/false) ...on/off
- speed (MIN/MAX/AUTO/NIGHT): speed of the ventilator
- swing: (true/false): turn on/off swinging
- fanonly: (true/false): only turn on fan, do not heat ore cool
- mode: (COOLING/HEATING): mode


## SOFTWARE

How to compile the software:

1. Download and install Platform IO ( https://platformio.org/platformio-ide )
2. chdir into the software folder
3. Copy src/configuration.example.h to src/configuration.h
4. Change the WiFi name and password in configuration.h (double check; they are case sensitive)
5. run "pio run" (or compile/run using Visual Studio)


## HARDWARE

I used a Wemos D1 Mini Lite (but any ESP8266 should work fine, and the code should be adaptable to an ESP32 with minimal effort) with a MAX 485 Module like https://www.amazon.de/ANGEEK-MAX485-Module-Converter-arduino/dp/B07X541M2T
Check out the PCB design in the pcb folder.


### Wire Connections

| ESP        | MAX485 |
|------------|--------|
| 5V         | 5V     |
| GND        | GND    |
| GPIO2 / D4 | RO     |
| GPIO0 / D3 | RE     |
| GPIO4 / D2 | DE     |
| GPIO5 / D1 | DI     |



## ADDRESSING

By default, the fancoil uses address 0
You can change the address of the fancoil by http://CONTROLLER_IP/changeAddress?sourceAddress=X&targetAddress=Y
NOTE: EVERY fancoil connected to the modbus using the source address will change its address. Make sure you are changing addresses one by one.

## Registering / Unregistering

Address must be registered. You can register an address by opening http://CONTROLLER_IP/ in a browser, enter the address (int) at the "register address form" and hit submit
You should unregister unused fancoils, because the controller will try to reach the address periodically unnecessarily otherwise.
