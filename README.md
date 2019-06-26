# ShroomLight
What are these shining things in the grass? oh cool!

A few LED shrooms (addressable APA106) hooked up to a ESP32 Wemos.
Together a bunch of shroomlights can create awesome patterns when placed out in a adressable grid.

A calibrate procedure will be run to assign each shroomlight an address in the grid.

To each shroomlight is a PIR sensor hooked up to detect movement. If movement is detected a multicast package will be sent out which contains the address of where the movement was detected.
If a neighboring shroomlight detects movement it will light up, and in its turn send out a multicast package of its adress. This way light patterns will spread out like rings on water.

# Configure
## Certificate
Generate a certificate and key.
Based on (https://github.com/espressif/esp-idf/tree/master/examples/system/ota)
```bash
openssl req -x509 -newkey rsa:2048 -keyout server_certs/ca_key.pem -out server_certs/ca_cert.pem -days 365 -nodes
```
Enter your computers IP address in Common Name (CN) field.
Fill in random credentials in the other fields.

## Configuration
Set wifi credentials and url to fetch the new binaries from.
```bash
make menuconfig
```
Enter "Shroom Light Configuration --->"

# Build and flash
## Build
```bash
make
```
## Flash
(Usually only done at first setup.)
```bash
make flash
```

## Flash via OTA
### Setup web server using certificates
```bash
cd build
openssl s_server -WWW -key ../server_certs/ca_key.pem -cert ../server_certs/ca_cert.pem -port 8070
```
### Flash
Restart the ESP to have it look for a new firmware.

# Serial Monitor
```bash
make monitor
```
Press <kbd>CTRL</kbd>+<kbd>^</kbd> to exit.


