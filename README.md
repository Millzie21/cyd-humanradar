# cyd-humanradar
Turns 2.8 inch cyd (cheap yellow display) into a 3 target human radar using a ld2450 mmwave radar module. 

In the ino source code you can set #define SIMULATE to true to show blips if you do not have radar module yet, however if you have it ensure it is set to false otherwise you would get fake data.

Wiring
| CYD | LD2450 | 
| --- | --- | 
| GPIO 27 (RX) | TX |
| GPIO 22 (TX) | RX | 

Parts required.
- LD2450 mmwave radar
- 2.8 inch cheap yellow display
- tp4056 charger with boost
- lipo battery, used one from a old vape
<br>
  Demo of it in action with simulate flag set to true
<p align="center"><img width="50%" alt="20261005_092002-ezgif com-optimize (1)" src="https://github.com/user-attachments/assets/410214fa-b1e9-4d77-b140-a6704adde8bc" /></p>


