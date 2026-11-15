from machine import UART, Pin
import time

uart = UART(2, baudrate=1200, bits=8, parity=None, stop=1,
            tx=17, rx=16, invert=UART.INV_TX)
rts = Pin(16, Pin.IN)

ID_PACKET = bytes([0xCA, 0xC0, 0x80, 0x80])   # 'J' ID, then the initial status

def on_rise(p):
    time.sleep_us(2200)
    uart.write(ID_PACKET)
    print(time.ticks_ms(), "ID sent, RTS =", rts.value())

def pad(x=0, y=0, b1=0, b2=0):
    x &= 255
    y &= 255
    uart.write(bytes([0xC0 | (x >> 6) | ((y & 0xC0) >> 4) | (b1 << 5) | (b2 << 4),
                      0x80 | (x & 63), 0x80 | (y & 63)]))

def tap(b1=0, b2=0):
    pad(0, 0, b1, b2)
    time.sleep_ms(100)
    pad()

def move(x=0, y=0, n=10):
    for _ in range(n):
        pad(x, y)
        time.sleep_ms(30)
    pad()

rts.irq(trigger=Pin.IRQ_RISING, handler=on_rise)
print(time.ticks_ms(), "ready - power on the CD-i now")
