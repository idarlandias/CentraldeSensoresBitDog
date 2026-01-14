# ============================================
# BitDogLab Environmental Station - MicroPython
# ============================================
# main.py - Entry point for Pico W

import time
import network
import urequests
import json
from machine import Pin, I2C, PWM

# --- CONFIGURATION ---
# Try with bytes for SSID with special character
WIFI_SSID = "@idarlan"
WIFI_PASSWORD = "pf255181"
SERVER_URL = "http://192.168.1.11:5001/api/sensor"

# Alternative: If SSID with @ fails, try escaping
# WIFI_SSID = b"@idarlan"  # bytes version

# --- HARDWARE PINOUT ---
SDA_OLED = 14
SCL_OLED = 15
SDA_SENSORS = 0
SCL_SENSORS = 1
LED_PIN = 7
BUZZER_PIN = 21

# I2C Addresses
MPU6050_ADDR = 0x68
BH1750_ADDR = 0x23
OLED_ADDR = 0x3C

# --- INITIALIZE I2C ---
i2c_oled = I2C(1, sda=Pin(SDA_OLED), scl=Pin(SCL_OLED), freq=400000)
i2c_sensors = I2C(0, sda=Pin(SDA_SENSORS), scl=Pin(SCL_SENSORS), freq=100000)

# --- SIMPLE OLED (SSD1306) ---
try:
    from ssd1306 import SSD1306_I2C

    oled = SSD1306_I2C(128, 64, i2c_oled, addr=OLED_ADDR)
    oled_available = True
except Exception as e:
    print(f"OLED not available: {e}")
    oled_available = False


def oled_print(text, line=0):
    if oled_available:
        oled.fill_rect(0, line * 10, 128, 10, 0)
        oled.text(text[:16], 0, line * 10)
        oled.show()


# --- BUZZER ---
buzzer = PWM(Pin(BUZZER_PIN))
buzzer.duty_u16(0)


def beep(freq=880, duration=0.1):
    buzzer.freq(freq)
    buzzer.duty_u16(32768)
    time.sleep(duration)
    buzzer.duty_u16(0)


def startup_melody():
    beep(660, 0.1)
    beep(880, 0.1)
    beep(990, 0.1)


# --- NEOPIXEL LED MATRIX (WS2812) ---
try:
    from neopixel import NeoPixel

    np = NeoPixel(Pin(LED_PIN), 25)  # 5x5 matrix
    led_available = True
except Exception as e:
    print(f"NeoPixel not available: {e}")
    led_available = False


def set_all_leds(r, g, b):
    if led_available:
        for i in range(25):
            np[i] = (r, g, b)
        np.write()


def clear_leds():
    set_all_leds(0, 0, 0)


# LED Bitmaps (5x5)
ARROW = [0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0]
LETTER_B = [1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0]
CHECK = [0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0]
X_MARK = [1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1]


def draw_bitmap(bitmap, r, g, b):
    if led_available:
        for i in range(25):
            np[i] = (r, g, b) if bitmap[i] else (0, 0, 0)
        np.write()


# --- MPU6050 FUNCTIONS ---
def mpu6050_init():
    try:
        i2c_sensors.writeto(MPU6050_ADDR, bytes([0x6B, 0x00]))  # Wake up
        return True
    except:
        return False


def mpu6050_read():
    try:
        data = i2c_sensors.readfrom_mem(MPU6050_ADDR, 0x3B, 6)
        ax = data[0] << 8 | data[1]
        ay = data[2] << 8 | data[3]
        az = data[4] << 8 | data[5]
        # Convert to signed
        if ax > 32767:
            ax -= 65536
        if ay > 32767:
            ay -= 65536
        if az > 32767:
            az -= 65536
        return ax, ay, az
    except:
        return 0, 0, 0


# --- BH1750 FUNCTIONS ---
def bh1750_init():
    try:
        i2c_sensors.writeto(BH1750_ADDR, bytes([0x10]))  # Continuous H-Res Mode
        return True
    except:
        return False


def bh1750_read():
    try:
        data = i2c_sensors.readfrom(BH1750_ADDR, 2)
        lux = (data[0] << 8 | data[1]) / 1.2
        return lux
    except:
        return 0.0


# --- WIFI CONNECTION ---
def connect_wifi():
    import rp2

    # Set country code (Brazil) - affects radio behavior
    try:
        rp2.country("BR")
        print("Country set to BR")
    except:
        print("Could not set country")

    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    time.sleep(1)

    # Disable power save mode (can cause issues)
    try:
        wlan.config(pm=0xA11140)  # Disable PM
        print("Power management disabled")
    except:
        pass

    if wlan.isconnected():
        print("Already connected!")
        return wlan

    # Disconnect and wait longer
    wlan.disconnect()
    time.sleep(3)

    # Scan for networks to find our SSID
    print("Scanning networks...")
    oled_print("Scanning...", 0)
    networks = wlan.scan()

    target_bssid = None
    for net in networks:
        ssid = net[0].decode("utf-8", "ignore")
        bssid = net[1]
        rssi = net[3]
        print(f"  Found: {ssid} (RSSI: {rssi})")
        if ssid == WIFI_SSID:
            target_bssid = bssid
            print(f"  -> Target found! BSSID: {bssid.hex()}")

    if target_bssid is None:
        print(f"Network '{WIFI_SSID}' not found!")
        oled_print("Net NotFound", 0)
        draw_bitmap(X_MARK, 15, 0, 0)
        return None

    print(f"Connecting to {WIFI_SSID}...")
    oled_print("Connecting", 0)
    draw_bitmap(ARROW, 0, 0, 15)  # Blue

    # Try connecting with multiple methods
    for attempt in range(5):
        print(f"Attempt {attempt + 1}/5...")
        oled_print(f"Try {attempt+1}/5", 3)

        # Try with just SSID and password
        try:
            wlan.connect(WIFI_SSID, WIFI_PASSWORD)
        except Exception as e:
            print(f"Connect error: {e}")

        # Wait for connection with status monitoring
        for i in range(20):
            status = wlan.status()

            if status == 3:  # STAT_GOT_IP
                ip = wlan.ifconfig()[0]
                print(f"SUCCESS! IP: {ip}")
                oled_print(f"IP:{ip}", 0)
                draw_bitmap(CHECK, 0, 15, 0)  # Green
                return wlan

            if status == 1:  # STAT_CONNECTING
                print(f"  Connecting...")
            elif status == 2:  # STAT_CONNECT_PENDING
                print(f"  Pending...")
            elif status == -1:  # STAT_CONNECT_FAIL
                print(f"  Connection failed")
                break
            elif status == -2:  # STAT_WRONG_PASSWORD
                print(f"  Wrong password?!")
                break
            elif status == -3:  # STAT_NO_AP_FOUND
                print(f"  AP not found")
                break
            else:
                print(f"  Status: {status}")

            time.sleep(1)

        # Reset for next attempt
        wlan.disconnect()
        wlan.active(False)
        time.sleep(2)
        wlan.active(True)
        time.sleep(2)

    print("All attempts failed!")
    oled_print("WiFi FAIL", 0)
    draw_bitmap(X_MARK, 15, 0, 0)  # Red
    return None


# --- SEND DATA TO SERVER ---
def send_data(lux, ax, ay, az):
    try:
        data = {"lux": round(lux, 2), "accel": {"x": ax, "y": ay, "z": az}}
        headers = {"Content-Type": "application/json"}
        response = urequests.post(SERVER_URL, json=data, headers=headers)
        response.close()
        return True
    except Exception as e:
        print(f"Send error: {e}")
        return False


# --- MAIN LOOP ---
def main():
    print("\n=== BitDogLab MicroPython ===")

    # Startup animation
    draw_bitmap(ARROW, 0, 15, 0)  # Green arrow
    time.sleep(1)
    draw_bitmap(LETTER_B, 0, 0, 15)  # Blue B
    time.sleep(1.5)

    startup_melody()

    # Init sensors
    oled_print("Init sensors...", 1)
    mpu_ok = mpu6050_init()
    bh_ok = bh1750_init()
    print(f"MPU6050: {mpu_ok}, BH1750: {bh_ok}")

    # Connect WiFi
    wlan = connect_wifi()
    wifi_ok = wlan is not None and wlan.isconnected()

    if wifi_ok:
        oled_print("WiFi: OK", 5)
    else:
        oled_print("WiFi: --", 5)

    # Main loop
    while True:
        # Read sensors
        lux = bh1750_read()
        ax, ay, az = mpu6050_read()

        # Update OLED
        oled_print(f"Lux: {lux:.1f}", 0)
        oled_print(f"X:{ax} Y:{ay}", 1)
        oled_print(f"Z: {az}", 2)

        # Send data if WiFi connected
        if wifi_ok and wlan.isconnected():
            if send_data(lux, ax, ay, az):
                oled_print("TX: OK", 4)
            else:
                oled_print("TX: ERR", 4)

        time.sleep(0.5)


# Run on boot
if __name__ == "__main__":
    main()
