# cyd-humanradar
Turns 2.8 inch cyd (cheap yellow display) into a 3 target human radar using a ld2450 mmwave radar module. 

In the ino source code you can set #define SIMULATE to true to show blips if you do not have radar module yet, however if you have it ensure it is set to false otherwise you would get fake data.

Wiring
| CYD | LD2450 | 
| --- | --- | 
| GPIO 27 (RX) | TX |
| GPIO 22 (TX) | RX | 
