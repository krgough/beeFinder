# beeFinder

This device consists of:
- An array of 5x5 LEDs used as a display
- An RFM69HW 868MHz radio
- Arduino Pro Mini used as the controller

We have 2 modes of operation:
1) Display Name of Owner - Name scrolls from left to right.
2) Transmit a broadcast to other devices on 868MHz network.  Each column of LEDs is a user and the column leds correspond to the RSSI for that device.  The column for this device has only the lower LED lit

Mode selection is done using the SCAN/NAME switch.

# config.h
Create a config.h file with the radio encryption key.  This must be exactly 16 chars.
