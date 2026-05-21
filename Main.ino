#include <Preferences.h>
Preferences prefs;

//RL_PINS ทำหน้าที่คู่ OUTPUT สั่ง Relay + INPUT อ่าน External IN
const int RL_PINS[4] = {33, 27, 26, 25};

const char KEY_ON[4]  = {'A', 'B', 'C', 'D'};
const char KEY_OFF[4] = {'W', 'X', 'Y', 'Z'};

#define RELAY_ACTIVE   LOW
#define RELAY_INACTIVE HIGH
#define INPUT_ACTIVE   LOW

#define DB9_RX   16
#define DB9_TX   17
#define DB9_BAUD 9600

#define DEBOUNCE_MS 20

int relayState[4];
int prevInputState[4];
unsigned long lastDebounceTime[4];
int pendingInputState[4];

//อ่าน External IN ผ่าน shared pin โยด3-sample ต้องตรงทั้งหมด
// คืนค่า: 1=active, 0=inactive, -1=uncertain(noise)
int readExternalIN(int ch) {
  int pin = RL_PINS[ch];

  pinMode(pin, INPUT);
  delayMicroseconds(10);// รอสัญญาณนิ่ง

  int a = digitalRead(pin);
  int b = digitalRead(pin);
  int c = digitalRead(pin);

  pinMode(pin, OUTPUT);
  digitalWrite(pin, relayState[ch] ? RELAY_ACTIVE : RELAY_INACTIVE);

  if (a == b && b == c) return (a == INPUT_ACTIVE) ? 1 : 0;
  return -1;
}

void saveRelay(int ch, int state) {
  prefs.begin("relay", false);
  char key[4]; sprintf(key, "r%d", ch);
  prefs.putInt(key, state);
  prefs.end();
}

int loadRelay(int ch) {
  prefs.begin("relay", true);
  char key[4]; sprintf(key, "r%d", ch);
  int val = prefs.getInt(key, 0);
  prefs.end();
  return val;
}

void serialSend(const char* msg) {
  Serial.println(msg);
  Serial2.println(msg);
}

void setRelay(int ch, int state) {
  relayState[ch] = state;
  digitalWrite(RL_PINS[ch], state ? RELAY_ACTIVE : RELAY_INACTIVE);
  saveRelay(ch, state);
  char buf[2] = { state ? KEY_ON[ch] : KEY_OFF[ch], 0 };
  serialSend(buf);
}

void handleCommand(char cmd) {
  switch (cmd) {
    case 'A': setRelay(0,1); break;  case 'W': setRelay(0,0); break;
    case 'B': setRelay(1,1); break;  case 'X': setRelay(1,0); break;
    case 'C': setRelay(2,1); break;  case 'Y': setRelay(2,0); break;
    case 'D': setRelay(3,1); break;  case 'Z': setRelay(3,0); break;

    case '?': {
      int in[4];
      for (int i = 0; i < 4; i++) in[i] = readExternalIN(i);

      char buf[64];
      sprintf(buf, "R1:%s/I:%s R2:%s/I:%s R3:%s/I:%s R4:%s/I:%s",
        relayState[0]?"ON":"OF", in[0]==1?"ON": in[0]==0?"OF":"??",
        relayState[1]?"ON":"OF", in[1]==1?"ON": in[1]==0?"OF":"??",
        relayState[2]?"ON":"OF", in[2]==1?"ON": in[2]==0?"OF":"??",
        relayState[3]?"ON":"OF", in[3]==1?"ON": in[3]==0?"OF":"??"
      );
      serialSend(buf);
      break;
    }

    case '0': for(int i=0;i<4;i++) setRelay(i,0); serialSend("ALL:OFF"); break;
    case '1': for(int i=0;i<4;i++) setRelay(i,1); serialSend("ALL:ON");  break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(DB9_BAUD, SERIAL_8N1, DB9_RX, DB9_TX);

  for (int i = 0; i < 4; i++) {
    pinMode(RL_PINS[i], OUTPUT);
    relayState[i] = loadRelay(i);
    digitalWrite(RL_PINS[i], relayState[i] ? RELAY_ACTIVE : RELAY_INACTIVE);

    int raw = readExternalIN(i);
    prevInputState[i]    = (raw == 1) ? 1 : 0;
    pendingInputState[i] = prevInputState[i];
    lastDebounceTime[i]  = 0;
  }

  serialSend("READY");
  char buf[40];
  sprintf(buf, "NVS R1:%s R2:%s R3:%s R4:%s",
    relayState[0]?"ON":"OF", relayState[1]?"ON":"OF",
    relayState[2]?"ON":"OF", relayState[3]?"ON":"OF");
  serialSend(buf);
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') handleCommand(c);
  }

  while (Serial2.available()) {
    char c = Serial2.read();
    if (c != '\n' && c != '\r') handleCommand(c);
  }

  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    int raw = readExternalIN(i);
    if (raw == -1) continue;

    if (raw != pendingInputState[i]) {
      pendingInputState[i] = raw;
      lastDebounceTime[i]  = now;
    }

    if ((now - lastDebounceTime[i]) >= DEBOUNCE_MS) {
      if (pendingInputState[i] != prevInputState[i]) {
        prevInputState[i] = pendingInputState[i];
        setRelay(i, prevInputState[i]);
      }
    }
  }

  delay(10);
}