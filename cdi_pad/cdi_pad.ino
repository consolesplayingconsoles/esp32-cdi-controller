// Bluetooth pad (Bluepad32) -> Philips CD-i joypad on an ESP32.
// Protocol as proven on the CD-i 450: 1200 baud, inverted TX, ID 0xCA ('J') 2.2 ms after
// each RTS rise, then 3-byte packets (TwBurn/cdi-controller-library).
#include <Bluepad32.h>

struct CdiPort {
    const char* name;
    int txPin;                 // to CD-i pin 2 (RXD), through the level shifter
    int rtsPin;                // from CD-i pin 7 (RTS), through the level shifter
    HardwareSerial* ser;
    volatile bool rise;
    volatile uint32_t riseUs;
    bool connected;
    uint32_t lastSendMs;
    uint8_t last[3];
};

// CD-i 450 on UART2.
CdiPort ports[] = {
    {"450", 17, 16, &Serial2, false, 0, false, 0, {0, 0, 0}},
};
const int NPORTS = sizeof(ports) / sizeof(ports[0]);

const uint8_t ID_PACKET[] = {0xCA, 0xC0, 0x80, 0x80};  // 'J' joypad, then centre status
const int SPEED = 32;       // max step per packet
const int DEADZONE = 80;    // of Bluepad32's -511..512 stick range
const uint32_t MOVE_MS = 26;  // a 3-byte packet takes 25 ms at 1200 baud

ControllerPtr pad = nullptr;

void IRAM_ATTR rtsIsr(void* arg) {
    CdiPort* p = (CdiPort*)arg;
    p->riseUs = micros();
    p->rise = true;
}

void onConnectedController(ControllerPtr ctl) {
    Serial.printf("controller connected: %s\n", ctl->getModelName().c_str());
    if (pad == nullptr) pad = ctl;   // its class may only be known once data arrives
}

void onDisconnectedController(ControllerPtr ctl) {
    if (ctl == pad) {
        pad = nullptr;
        Serial.println("pad disconnected");
    }
}

void packet(int x, int y, bool b1, bool b2, uint8_t out[3]) {
    uint8_t ux = (uint8_t)(int8_t)x;
    uint8_t uy = (uint8_t)(int8_t)y;
    out[0] = 0xC0 | (ux >> 6) | ((uy & 0xC0) >> 4) | (b1 << 5) | (b2 << 4);
    out[1] = 0x80 | (ux & 0x3F);
    out[2] = 0x80 | (uy & 0x3F);
}

int axis(int v) {
    if (v > -DEADZONE && v < DEADZONE) return 0;
    return v * SPEED / 512;
}

void setup() {
    Serial.begin(115200);
    for (int i = 0; i < NPORTS; i++) {
        CdiPort& p = ports[i];
        if (p.txPin < 0) continue;
        p.ser->begin(1200, SERIAL_8N1, -1, p.txPin, true);  // true = inverted line
        pinMode(p.rtsPin, INPUT);
        attachInterruptArg(digitalPinToInterrupt(p.rtsPin), rtsIsr, &p, RISING);
        Serial.printf("CD-i %s: TX GPIO%d, RTS GPIO%d\n", p.name, p.txPin, p.rtsPin);
    }
    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.enableVirtualDevice(false);
}

void loop() {
    BP32.update();

    int x = 0, y = 0;
    bool b1 = false, b2 = false;
    if (pad && pad->isConnected() && pad->isGamepad()) {
        uint8_t d = pad->dpad();
        x = (d & DPAD_RIGHT) ? SPEED : (d & DPAD_LEFT) ? -SPEED : axis(pad->axisX());
        y = (d & DPAD_DOWN) ? SPEED : (d & DPAD_UP) ? -SPEED : axis(pad->axisY());
        b1 = pad->a() || pad->x();   // X = both buttons (CD-i button 3)
        b2 = pad->b() || pad->x();
    }
    uint8_t pkt[3];
    packet(x, y, b1, b2, pkt);

    uint32_t now = millis();
    for (int i = 0; i < NPORTS; i++) {
        CdiPort& p = ports[i];
        if (p.txPin < 0) continue;
        if (!digitalRead(p.rtsPin)) {
            p.connected = false;
            continue;
        }
        if (p.rise) {                  // a new request from the player: answer it again
            if (micros() - p.riseUs < 2200) continue;
            p.rise = false;
            p.connected = false;
        }
        if (!p.connected) {            // RTS high and not identified yet, also after an ESP32 reset
            p.ser->write(ID_PACKET, sizeof(ID_PACKET));
            memcpy(p.last, ID_PACKET + 1, 3);
            p.connected = true;
            p.lastSendMs = now;
            Serial.printf("CD-i %s: ID sent\n", p.name);
            continue;
        }
        // At most one packet per MOVE_MS, always the newest state: anything faster
        // than the line drains only queues up in the UART and shows as lag.
        if (now - p.lastSendMs < MOVE_MS) continue;
        bool changed = memcmp(pkt, p.last, 3) != 0;
        bool moving = x != 0 || y != 0;
        if (changed || moving) {
            p.ser->write(pkt, 3);
            memcpy(p.last, pkt, 3);
            p.lastSendMs = now;
        }
    }
    delay(1);
}
