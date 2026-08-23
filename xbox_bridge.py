import socket
import time
import pygame

# --- CONFIGURATION ---
UDP_PORT = 4210
# ---------------------

def discover_esp32():
    """Waits passively for the ESP32 to shout its location at startup."""
    print("⏳ Listening for ESP32 boot shout... (Turn on or reset your ESP32 now)")
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Bind to all local interfaces to catch network broadcasts securely
    sock.bind(('', UDP_PORT)) 
    sock.settimeout(30.0) # Give you 30 seconds to boot or reset the board
    
    try:
        data, addr = sock.recvfrom(1024)
        if data == b"FERRARI_BRIDGE_HERE":
            print(f"🎯 Auto-Discovery Success! ESP32 captured at IP: {addr[0]}")
            sock.close()
            return addr[0]
    except socket.timeout:
        print("❌ Auto-Discovery timed out! No boot shout heard.")
        sock.close()
        return None

# Start Auto-Discovery
ESP32_IP = discover_esp32()
if not ESP32_IP:
    exit()

# Initialize Xbox Controller System
pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    print("❌ No Xbox controller detected!")
    exit()

joystick = pygame.joystick.Joystick(0)
joystick.init()
print(f"✅ Controller Connected: {joystick.get_name()}")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
print("🏁 Ready to race! Use Triggers to Drive, Left Stick to Steer. Press Ctrl+C to exit.")

lights_on = 0
last_x_button_state = 0

try:
    while True:
        pygame.event.pump()
        
        # Read Triggers (RT = Forward, LT = Reverse)
        rt_axis = joystick.get_axis(5)
        lt_axis = joystick.get_axis(4)
        
        forward_pressed = (rt_axis + 1.0) / 2.0 > 0.3
        reverse_pressed = (lt_axis + 1.0) / 2.0 > 0.3

        drive = 0
        if forward_pressed: drive = 1
        elif reverse_pressed: drive = 2

        # Read Left Joystick for Steering
        steering_axis = joystick.get_axis(0)
        steer = 0
        if steering_axis < -0.4: steer = 1    # Left
        elif steering_axis > 0.4: steer = 2   # Right

        # Read Button A for Turbo
        turbo = 1 if joystick.get_button(0) else 0

        # Read Button X for Lights Toggle
        x_button = joystick.get_button(2)
        if x_button == 1 and last_x_button_state == 0:
            lights_on = 1 if lights_on == 0 else 0  
        last_x_button_state = x_button

        # Send the 4-byte packet to the discovered IP address
        packet = bytes([drive, steer, turbo, lights_on])
        sock.sendto(packet, (ESP32_IP, UDP_PORT))
        
        time.sleep(0.03)

except KeyboardInterrupt:
    sock.sendto(bytes([0, 0, 0, 0]), (ESP32_IP, UDP_PORT))
    print("\n🛑 Stopping controller link. Car stopped safely.")
