**ESP32 bridge for Xbox controller and Bburago Ferrari SL-SF-24**

As the name says, it's a simple project to replace the original app *ShellRacingLegends*.
It's intended for the SL-SF-24 only. I can add more profiles in future releases.

Bluetooth Low Energy advertising can't connect as host and client in a efficient way to receive data and send it to SF-24, so I initially chose to read inputs in a Python script running on a PC and send to ESP32 via WiFi, which translates to SF-24 6 main instructions (steer-left, steer-right, accelerate, brake, turbo and lantern -- the last function isn't available in SF-24).

    2026-08-23, WeeBee
