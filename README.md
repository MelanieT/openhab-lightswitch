# openhab-lightswitch
A wall switch for OpenHAB

This is a work in progress, although quite usable. I use it in my home.

![Screenshot of a sitemap](/screenshots/Picture_2025_02_27T20_59_52_898Z.jpeg)
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

# OpenHAB

Recent versions of OpenHAB serve icons as svg. The task of properly decoding SVG images is too large for a microcontroller, so such projects depend on a small python program that needs to run on the Raspberry Pi that OpenHAB is installed on.

```
import cairosvg
import requests
from flask import Flask, make_response
from flask import request
app = Flask(__name__)

@app.route('/')
def svgtopng():  # put application's code here
    name = request.args.get("image")
    size = request.args.get("size")
    if size is None:
        size = 64
    else:
        size=int(size)

    req = requests.get(f"http://openhab.t-data.com:8080/icon/{name}?format=png&anyFormat=true&state=0&iconset=classic")
    if req.status_code != 200:
        resp = make_response("")
        resp.status_code = req.status_code
        return resp

    if req.headers.get("Content-type") == "image/png":
        print("image is png")
        resp = make_response(req.content)
        resp.headers.set("Content-type", req.headers.get("Content-type"))
        return resp

    resp = make_response(cairosvg.svg2png(req.content, output_width=size, output_height=size))
    resp.headers.set("Content-type", "image/png")

    return resp

if __name__ == '__main__':
    app.run()
```

Install this python app into a virtual env and run it from there. Make sure it starts when OpenHAB restarts, or you will see no icons.
Please note that port 5050 is hardcoded in this project.

A systemd unit that will run this is here:

```
[Unit]
Description=SVG to PNG converter
Wants=network-online.target
After=network-online.target

[Service]
User=svgserver
Group=svgserver
Type=simple
WorkingDirectory=/home/svgserver
ExecStart=/home/svgserver/venv/bin/python -m flask run --host 0.0.0.0 --port 5050
RestartSec=5
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

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

