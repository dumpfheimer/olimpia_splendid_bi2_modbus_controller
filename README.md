# olimpia_splendid_bi2_modbus_controller
Modbus Controller for Olimpia Splendid BI2 fancoils and similar devices.


THIS IS WORK IN PROGRESS, PLEASE LET ME KNOW IF THINGS ARE NOT WORKING, BUT DO NOT BLAME ME FOR IT

Temporary, crappy HowTo:

1. Open the project in Arduino IDE
2. Configure WiFi
3. Flash it on your ESP8266 (I used a D1 Mini)
4. Wire up the modbus converter (details will follow)
5. Connect the modbus devices
6. Change Fancoil Settings over http://IP/set?on=1

HTTP examples:
/set?addr=1&ambient=22.9&setpoint=23.0&on=0&speed=MIN&swing=False&fanonly=False&mode=COOLING

addr: (0...31) modbus address of the fancoil
ambient: (XX.X) temperature in deg C of the current room temperature
setpoint: (XX.X) temperature in deg C of the desired setpoint
on: (true/false) ...on/off
speed (MIN/MAX/AUTO/NIGHT): speed of the ventilator
swing: (true/false): turn on/off swinging
fanonly: (true/false): only turn on fan, do not heat ore cool
mode: (COOLING/HEATING): mode

HARDWARE

I used a Wemos D1 Mini with a MAX 485 Module like https://www.amazon.de/ANGEEK-MAX485-Module-Converter-arduino/dp/B07X541M2T

Check out the PCB design in the pcb folder.


Wire Connections:

(need to look that up)
