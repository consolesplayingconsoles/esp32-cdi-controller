from machine import Pin
from array import array
import time

rts = Pin(16, Pin.IN)
t = array('i', [0] * 300)
v = array('b', [0] * 300)

def capture(ms=3000):
    val = rts.value
    ticks = time.ticks_us
    diff = time.ticks_diff
    last = val()
    print("waiting - power on the CD-i, RTS =", last)
    while val() == last:
        pass
    t0 = ticks()
    n = 0
    while n < 300 and diff(ticks(), t0) < ms * 1000:
        x = val()
        if x != last:
            t[n] = diff(ticks(), t0)
            v[n] = x
            n += 1
            last = x
    for i in range(n):
        print(t[i], v[i])
    print(n, "edges")

capture()
