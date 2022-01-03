# Olimpia Splendid Bi2 Modbus Controller

## THIS IS WORK IN PROGRESS, PLEASE LET ME KNOW IF THINGS ARE NOT WORKING, BUT DO NOT BLAME ME FOR IT

### Temporary, crappy HowTo:

1. Open the project in Arduino IDE
2. Configure WiFi (copy configuration.example.h to configuration.h)
3. Flash it on your ESP8266 (I used a D1 Mini)
4. Wire up the modbus converter (details will follow)
5. Connect the modbus devices
6. Put your Fan Coil into "Remote" Mode (check the manual, for me, I think, it was touching "-" and the fan "button" for 10s)
7. If needed, change the fan coil address
8. Register the fan coil address (see registering/unregistering)
9. Change Fancoil Settings over http://CONTROLLER_IP/set?on=1

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

1. Download and setup Arduino IDE https://www.arduino.cc/en/software/
2. Open this project in the IDE
3. Install the ESP8266 board (google instructions; or follow https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/)
4. Install the Elegant OTA update library https://github.com/ayushsharma82/ElegantOTA#how-to-install
5. Hope that I didn't forget any other needed libs
6. Copy configuration.example.h to configuration.h
7. Change the WiFi name and password in configuration.h
8. compile and run it =)

## HARDWARE

I used a Wemos D1 Mini with a MAX 485 Module like https://www.amazon.de/ANGEEK-MAX485-Module-Converter-arduino/dp/B07X541M2T

Check out the PCB design in the pcb folder.


#### Wire Connections:

(need to look that up)


## ADDRESSING

By default the fancoil uses address 0
You can change the address of the fancoil by http://CONTROLLER_IP/changeAddress?sourceAddress=X&targetAddress=Y
NOTE: EVERY fancoil connected to the modbus using the source address will change its address. Make sure you are changing one by one.

## Registering / Unregistering

Address must be registered. You can register an address by opening http://CONTROLLER_IP/ in a browser, enter the address (int) at the "register address form" and hit submit
You should unregister unused fancoils, because the controller will try to reach the address periodically unnessecarily otherwise.
