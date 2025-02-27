# openhab-lightswitch
A wall switch for OpenHAB

This is a work in progress, although quite usable. I use it in my home.

# Hardware:

Get this display from Waveshare:

https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4

I got mine from 

https://www.amazon.co.uk/dp/B0DCZGB5LH?ref=ppx_yo2ov_dt_b_fed_asin_title
This is not an affiliate link, I get no money if you buy this.

There are multiple ways to power this:

- USB-C - The v2.0 version will require the bat-pwr to be clicked even when using USB power
- Wide range power input - refer to the Waveshare Wiki as you will have to cut two traces on the board
- Direct 5v : Locate the "I2C" voltage setting, it is a zero ohms resistor presoldered in the 3v position. Move it to the 5v position and apply 5v to the VCC and GND pins in the I2C section of the connector (NOT the normal voltege inputs!)

For flashing, connect to your computer using the USB connecotr and press the bat-pwr if required.

# Software
Make sure you have esp-idf installed at version 5.4 or newer, and that you have initialized it.

You also need a recent version of npm.

Get the web interface fromm here: https://github.com/MelanieT/wifi-configurator.git

Once you have checked out the web interface, change into its directory and type:

npm install

followed by:

npm run build

After cloning this repository, change into the directory you cloned to and run:

git submodule update --init

Edit CMakeLists.txt to replace the path in the spiffs_create_partition_image command with the path to the "dist" directory created by npm run build.

Make the project with:

idf.py build

Flash to the screen with

idf.py flash monitor

Use your smartphone to follow the directions on the display.

First, you will be connecting to a wifi as directed on the screen. You should see a captive portal where you can list the wifi networks in your area, select one and enter the password. The software will test the connection and, if sucessful, show an ip address on the screen. onnect to your wifi and access the given IP address with your browser.
Here you can enter the URL of your OpenHAB installation. Don't omit the http:// or port :8080, use a DNS name or the IP address, not an MDNS (*.local) address!
Once you click next, you will be able to select a sitemap from the OpenHAB server. Once you confirm the selection, the switch will restart and the sitemap will be active.
The web interface remains open for you to change the server or sitemap.

# IMPORTANT

The sitemaps this device uses need a special label format, label="Your friendly name{directives for dispay formatting}[value format["

This will be documented in due time.

