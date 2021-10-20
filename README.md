# ShroomLight
What are these shining things in the grass? oh cool!

A few LED shrooms (addressable APA106) hooked up to a ESP32 Wemos.
Together a bunch of shroomlights can create awesome patterns when placed out in a adressable grid.

To each shroomlight is a PIR sensor hooked up to detect movement. If movement is detected a Bluetooth mesh package will be sent out which contains the address of where the movement was detected.
If a neighboring shroomlight detects movement it will light up, and in its turn send out a new package of its adress. This way light patterns will spread out like rings on water.

# Build and flash

## Build
```bash
idf.py build
```

## Flash
```bash
idf.py flash
```

# Serial Monitor
## Monitor
```bash
idf.py monitor
```

## Close monitor
Press <kbd>CTRL</kbd>+<kbd>]</kbd> to exit.

## Build and flash within monitor
Press <kbd>CTRL</kbd>+<kbd>T</kbd> then <kbd>A</kbd> to build and flash.

