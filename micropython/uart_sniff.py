from machine import UART
import time

uart = UART(2, baudrate=9600, bits=8, parity=None, stop=1,
            tx=17, rx=16, invert=UART.INV_TX | UART.INV_RX)
print("power on the CD-i now")
while True:
    if uart.any():
        data = uart.read()
        print(time.ticks_ms(), " ".join("%02X" % b for b in data))
    time.sleep_ms(5)
