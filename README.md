# cyd-humanradar
Turns 2.8 inch cyd (cheap yellow display) into a 3 target human radar using a ld2450 mmwave radar module. 

In the ino source code you can set #define SIMULATE to true to show blips if you do not have radar module yet, however if you have it ensure it is set to false otherwise you would get fake data.

Ensure you have lovyanGFX library installed in arduino library manager to compile code

Wiring
| CYD | LD2450 | 
| --- | --- | 
| GPIO 27 (RX) | TX |
| GPIO 22 (TX) | RX | 

Parts required.
- LD2450 mmwave radar
- 2.8 inch cheap yellow display
- tp4056 charger with 5v boost
- lipo battery, used one from a old vape
<br>
  Demo of it in action with simulate flag set to true
  <br>
  <img width="30%" alt="20261005_092002-ezgif com-optimize (1)" src="https://github.com/user-attachments/assets/410214fa-b1e9-4d77-b140-a6704adde8bc" />

  To flash bin file use <a href="https://esptool.spacehuhn.com" target="_blank">Spacehuhn web flasher</a>. Select connect and connect via your com port then remove the 3 flash partitions by pressing the x on the side and should be left with one then set the target address to 0x0 and select the latest bin file that you downloaded. Should look like this below and hit program once you are done.
  <br>
  <br>
  <img width="753" height="379" alt="flasher" src="https://github.com/user-attachments/assets/527e6af3-0034-4de5-887d-508725d9880b" />



