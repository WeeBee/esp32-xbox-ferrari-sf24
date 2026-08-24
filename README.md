**ESP32 BLE bridge for Xbox controller and Bburago Ferrari SL-SF-24**

As the name says, it's a simple project to replace the original app *ShellRacingLegends*. I think the original app lacks a joystick option acting only like bridge, with screen turned off.
It's intended for the SL-SF-24 only. I can add more profiles in future releases (and a mobile app, maybe?).

The ESP32 now acts as a BLE central for both devices. It reads the Xbox HID reports directly and sends the same car commands to the SL-SF-24, so the PC, Python, WiFi credentials and UDP discovery are no longer needed. `xbox_bridge.py` is retained as the original PC-based fallback.

~~Originally, the ESP32 received four-byte command packets over WiFi from `xbox_bridge.py`. The Python program read the Xbox controller, discovered the ESP32 over UDP, and the ESP32 translated those commands into BLE instructions for the car.~~

## Arduino IDE setup

1. Install the ESP32 board package and select the ESP32 board used by the project.
2. Install `NimBLE-Arduino` from Library Manager.
3. Open the `Joy2SF24_BLE` sketch folder and upload `Joy2SF24_BLE.ino`.
4. Turn on the car and an Xbox One S / Series controller (models 1708, 1914 or adaptive). The sketch scans for both and reconnects after a disconnect.

The controller must support Bluetooth. Xbox 360 controllers and Xbox pads that only use the proprietary Xbox wireless protocol cannot connect to an ESP32 BLE host. Open Serial Monitor at 115200 baud to see discovery and connection status.

The original `WiFiHost_BLEClient` sketch remains in its own folder as the PC/Python and WiFi alternative.

~~At first, Bluetooth Low Energy advertising could not act as host and client efficiently enough to receive controller data and send it to the SF-24, so the first version used a Python script on a PC. The script read the Xbox controller and sent six commands over WiFi to the ESP32: steer left, steer right, accelerate, brake, turbo, and lantern.~~

I'm running ESP32 on a protoboard, so its power supply still comes from Micro-USB, but I thought of it with batteries in my pocket or attached to the controller.

Thanks to the people who shared information about the BLE profile in these little machines. Links are below.
    2026-08-23, WeeBee

- https://github.com/tmk907/RacingCarsController
- https://gist.github.com/scrool/e79d6a4cb50c26499746f4fe473b3768
