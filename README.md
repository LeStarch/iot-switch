IoT Switch
==========

This project uses an ESP 8266 and a Teensy 3.2 to creat an internet-of-things (IoT) light switch.  Connect to port 333 and send "ON\r\n" or "OFF\r\n" to turn on and off the switch respectively.  Run this through a relay to turn on/off more than just 3.3V logic.

Note: in order for this to compile, the user should create "private.h" in the "src" folder containing two #defines "SSID" and "PASSWORD" to configure this for their home network.

Uses platform IO.
