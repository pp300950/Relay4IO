#include <Preferences.h>
Preferences prefs;

const int RL_PINS[4] = {33, 27, 26, 25};  // R1 R2 R3 R4

const int IN_PINS[4] = {22, 21, 14, 12};  // IN1 IN2 IN3 IN4

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
      char buf[40];
      sprintf(buf, "R1:%s R2:%s R3:%s R4:%s",
        relayState[0]?"ON":"OF", relayState[1]?"ON":"OF",
        relayState[2]?"ON":"OF", relayState[3]?"ON":"OF");
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

    pinMode(IN_PINS[i], INPUT);

    int raw = digitalRead(IN_PINS[i]);
    prevInputState[i]    = (raw == INPUT_ACTIVE) ? 1 : 0;
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
    int raw   = digitalRead(IN_PINS[i]);
    int state = (raw == INPUT_ACTIVE) ? 1 : 0;

    if (state != pendingInputState[i]) {
      pendingInputState[i] = state;
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
