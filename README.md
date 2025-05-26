# Microwave-Oven-Control
Simple C program for controlling a microwave oven using an ESP8266/ESP32, a TM1638 LED&amp;KEY board, and a 3 relays board

After a power surge the electronics of my old 800 watts micro wave oven died. However, the electric circuit and microwave components of the appliance survived. Thus, new electronics had to be created to control the average power (100W, 200W, 400W, 600W, and 800W) and the duration of the heating process. Furthermore, the device sports a grill that I actually never use but it is there so it has to run.

I did not want to create any custom PCB so I combined a bunch of off the shelve modules that a easily and at very low cost available on the internet:
- ESP 8266 NodeMCU board
- TM 1638 LED&Key board
- Three-Relay board
- An old USB power supply
- Six buttons
- A power switch
- Some bread board cabling
- A piece of plexiglas 

The interesting part is that all functional safety aspects are taken care of by the electric circuit of the microwave oven. There is no need to make the electronics functional save.

The setup is now running very smooth since a long time so I can recomment copying to anybody who wants to run their own micro wave oven electronics.

![mw](https://github.com/user-attachments/assets/64d40f81-95f6-4380-bd15-08df3b3603f4)
