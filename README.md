**ESP32 bridge for Xbox controller and Bburago Ferrari SL-SF-24**

As the name says, it's a simple project to replace the original app *ShellRacingLegends*. I think the original app lacks a joystick option acting only like bridge, with screen turned off.
It's intended for the SL-SF-24 only. I can add more profiles in future releases (and a mobile app, maybe?).

Bluetooth Low Energy advertising can't act as host and client in a efficient way to receive data and send it to SF-24, so I initially chose to read inputs in a Python script running on a PC and send to ESP32 via WiFi, which translates to SF-24 six main instructions (steer-left, steer-right, accelerate, brake, turbo and lantern -- the last function isn't available in SF-24). This approach is nice to move between rooms, since you will be next to your car with your joystick and ESP32.

I'm running ESP32 on a protoboard, so its power supply still comes from Micro-USB, but I thought of it with batteries in my pocket or attached to the controller.

    2026-08-23, WeeBee
