#include <Arduino.h>

/*
  DIP Switch Settings (ACTIVE LOW)

  DIP1 (Pin 1) - Range Select LSB
  DIP2 (Pin 2) - Range Select MSB
    00 = 2 meters
    01 = 3 meters
    10 = 4 meters
    11 = 5 meters

  DIP3 (Pin 3) - Track Mode
    ON (LOW)  = Random tracks (00001–00034)
    OFF (HIGH)= Track 00050 only

  DIP4 (Pin 4) - Mute
    ON (LOW)  = Silent (LED/radar only)
    OFF (HIGH)= Sound playback enabled
*/

// ----- Pin Definitions -----
#define LED_PIN         0
#define MP3_BUSY_PIN    5
#define RADAR_BOOT_PIN  16
#define radarSerial     Serial1
#define mp3Serial       Serial0

// DIP Switches (INPUT_PULLUP)
#define DIP1 1
#define DIP2 2
#define DIP3 3
#define DIP4 4

// ----- Timing Settings -----
#define CALM_TIMEOUT_MS     30000UL
#define CALM_LOCK_MS        30000UL
#define DIP_UPDATE_MS       5000UL
#define PRESENCE_TRIGGER_MS 15000UL
#define COOLDOWN_MS         180000UL  // 3 minutes

// ----- LED Breathing -----
const int MIN_BRIGHT = 13;
const int MAX_BRIGHT = 255;
const float BREATH_MIN_DIST = 1.0;
const float BREATH_MAX_DIST = 2.5;
const float MIN_CYCLE_MS = 2000;
const float MAX_CYCLE_MS = 6000;
const float BREATH_SMOOTHING = 0.05;

// ----- State -----
String radarBuffer = "";
float lastDistance = BREATH_MAX_DIST;
bool triggered = false;
bool inRange = false;
unsigned long rangeStart = 0;
unsigned long lastPlayEnd = 0;

bool inCalmState = false;  // Start in pulse mode
unsigned long lastSeenTime = 0;
unsigned long enteredCalmTime = 0;

float smoothedDistance = BREATH_MAX_DIST;
float cycleMs = MAX_CYCLE_MS;
float phase = 0.0;
unsigned long lastMillis = 0;
unsigned long lastDipUpdate = 0;

uint8_t detectionRangeMeters = 4;

// ----- DIP Logic -----
uint8_t getRangeMetersFromDIP() {
  uint8_t val = 0;
  if (digitalRead(DIP1) == LOW) val |= 0x01;
  if (digitalRead(DIP2) == LOW) val |= 0x02;
  return constrain(val + 2, 2, 5);  // DIP 00 = 2m, 01 = 3m, etc.
}

bool isMuted() {
  return digitalRead(DIP4) == LOW;
}

bool useRandomTracks() {
  return digitalRead(DIP3) == LOW;
}

// ----- Play Track (5-digit filenames like 00001.mp3) -----
void playTrack(uint16_t track) {
  if (isMuted()) return;

  uint8_t cmd[] = {
    0xAA, 0x07, 0x02,
    (uint8_t)(track >> 8),
    (uint8_t)(track & 0xFF),
    0x00
  };
  uint8_t sum = 0;
  for (int i = 0; i < 5; i++) sum += cmd[i];
  cmd[5] = sum & 0xFF;
  for (int i = 0; i < 6; i++) {
    mp3Serial.write(cmd[i]);
  }
}

// ----- Radar Parser -----
void processLine(String line) {
  line.trim();
  int idx = line.indexOf("dis=");
  if (idx != -1) {
    lastDistance = line.substring(idx + 4).toFloat();
    bool nowInRange = (lastDistance <= detectionRangeMeters);

    if (nowInRange && !inRange) {
      inRange = true;
      rangeStart = millis();
    } else if (!nowInRange && inRange) {
      inRange = false;
      rangeStart = 0;
    }

    if (nowInRange) {
      lastSeenTime = millis();
      if (inCalmState && millis() - enteredCalmTime >= CALM_LOCK_MS) {
        inCalmState = false;
      }
    }
  }
}

// ----- LED Behavior -----
void updateEyeLED() {
  if (!inCalmState && millis() - lastSeenTime > CALM_TIMEOUT_MS) {
    inCalmState = true;
    enteredCalmTime = millis();
  }

  if (inCalmState) {
    analogWrite(LED_PIN, MAX_BRIGHT);
    return;
  }

  smoothedDistance = smoothedDistance * (1.0 - BREATH_SMOOTHING) + lastDistance * BREATH_SMOOTHING;
  float tNorm = (smoothedDistance - BREATH_MIN_DIST) / (BREATH_MAX_DIST - BREATH_MIN_DIST);
  tNorm = constrain(tNorm, 0.0, 1.0);
  float targetCycle = MIN_CYCLE_MS + tNorm * (MAX_CYCLE_MS - MIN_CYCLE_MS);
  cycleMs = cycleMs * 0.9 + targetCycle * 0.1;

  unsigned long now = millis();
  float delta = (now - lastMillis) / cycleMs;
  phase += delta;
  lastMillis = now;
  if (phase >= 1.0) phase -= 1.0;

  float breath = 0.5 * (1 - cos(2 * PI * phase));
  int brightness = MIN_BRIGHT + breath * (MAX_BRIGHT - MIN_BRIGHT);
  analogWrite(LED_PIN, brightness);
}

// ----- Setup -----
void setup() {
  pinMode(DIP1, INPUT_PULLUP);
  pinMode(DIP2, INPUT_PULLUP);
  pinMode(DIP3, INPUT_PULLUP);
  pinMode(DIP4, INPUT_PULLUP);
  pinMode(MP3_BUSY_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RADAR_BOOT_PIN, OUTPUT);
  digitalWrite(RADAR_BOOT_PIN, HIGH);

  radarSerial.begin(115200);
  mp3Serial.begin(9600);
  delay(500);

  radarSerial.println("test_mode=0");
  delay(50);
  radarSerial.println("rmax=4");
  delay(50);
  radarSerial.println("save");

  detectionRangeMeters = getRangeMetersFromDIP();
  lastSeenTime = millis();
  enteredCalmTime = millis();
  lastPlayEnd = millis() - COOLDOWN_MS;  // Skip initial wait
  lastMillis = millis();
  lastDipUpdate = millis();
}

// ----- Main Loop -----
void loop() {
  while (radarSerial.available()) {
    char c = radarSerial.read();
    if (c == '\n' || c == '\r') {
      if (radarBuffer.length() > 0) {
        processLine(radarBuffer);
        radarBuffer = "";
      }
    } else {
      radarBuffer += c;
    }
  }

  bool busy = (digitalRead(MP3_BUSY_PIN) == LOW);

  if (!busy && !triggered && millis() - lastPlayEnd > COOLDOWN_MS) {
    if (inRange && millis() - rangeStart >= PRESENCE_TRIGGER_MS) {
      uint16_t track = useRandomTracks() ? random(1, 35) : 50;
      playTrack(track);
      triggered = true;
    }
  }

  if (triggered && !busy) {
    lastPlayEnd = millis();
    triggered = false;
  }

  if (millis() - lastDipUpdate > DIP_UPDATE_MS) {
    detectionRangeMeters = getRangeMetersFromDIP();
    lastDipUpdate = millis();
  }

  updateEyeLED();
}
