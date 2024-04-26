# MLX90109_Reader_ATTiny85

# Development

These instructions were tested with:
* Ubuntu 22.04 x86-64.
* Arduino IDE 2.3.2 installed from .zip file.
* SendOnlySoftwareSerial commit a1d3d44110a6.
* Compilation verified, but actually running the code not verified.

Download the Arduino IDE from https://www.arduino.cc/en/software.

Run the IDE. Open preferences: File menu -> Preferences menu item.

Make a note of the "Sketchbook location". At least on Linux, this defaults to
`~/Arduino`. Use this path in the commands below.

Add `http://drazzy.com/package_drazzy.com_index.json` to "Additional boards
(sic) manager URLs".

Click OK. You might need to restart the IDE,

Toole menu > Board > Boards Manager. Search for ATTinyCore. Click Install.

Tools menu > Board > ATTinyCore > ATTiny 25/45/85 (no bootloader). A lot of
other config options will show up in the tools menu. I assume the defaults
will work fine.

Run the commands below to get a dependency library. Unfortunately, the
libraries can't be part of the sketch directory despite what the IDE docs say
in some places (at least for older versions; maybe that feature got removed in
more recent versions.)

```shell
mkdir -p ~/Arduino/libraries
cd ~/Arduino/libraries
git clone https://github.com/nickgammon/SendOnlySoftwareSerial.git
```

Click the "verify" button to build the project.

Click the "upload" button to build the project then upload it to an attached
device.

TODO: automate all of the above using arduino-cli and Docker.
