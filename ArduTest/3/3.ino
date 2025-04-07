#include <Arduino.h>

// ----- Pin Definitions -----
#define LED_PIN         0
#define MP3_BUSY_PIN    12
#define RADAR_BOOT_PIN  16
#define radarSerial     Serial1
#define mp3Serial       Serial0

// DIP Switches
#define DIP1 1  // Range LSB
#define DIP2 2  // Range MSB
#define DIP3 3  // Mode: Random (ON) / Track 50 (OFF)
#define DIP4 4  // Mute (ON disables audio)

// LED Brightness & Breathing Timing
const int MIN_BRIGHT = 13;
const int MAX_BRIGHT = 255;
const float BREATH_MIN_DIST = 1.0;
const float BREATH_MAX_DIST = 2.5;
const float MIN_CYCLE_MS = 2000;
const float MAX_CYCLE_MS = 6000;
const float BREATH_SMOOTHING = 0.05;

// Timing & Triggering
#define DETECTION_TIME_MS   2000
#define COOLDOWN_MS         300000UL
#define CALM_TIMEOUT_MS     30000UL
#define CALM_LOCK_MS        30000UL
#define DIP_UPDATE_MS       5000UL

// State Tracking
String radarBuffer = "";
float lastDistance = BREATH_MAX_DIST;
bool isMoving = false;
bool triggered = false;
unsigned long movStart = 0;
unsigned long lastMov = 0;
unsigned long lastPlay = 0;

// Calm State
bool inCalmState = true;
unsigned long lastSeenTime = 0;
unsigned long enteredCalmTime = 0;

// Breathing State
float smoothedDistance = BREATH_MAX_DIST;
float cycleMs = MAX_CYCLE_MS;
float phase = 0.0;
unsigned long lastMillis = 0;
unsigned long lastDipUpdate = 0;

// Active range (updated from DIP1/2)
uint8_t detectionRangeMeters = 4;

// ----- DIP Utilities -----
uint8_t getRangeMetersFromDIP() {
  uint8_t val = 0;
  if (!digitalRead(DIP1)) val |= 0x01;
  if (!digitalRead(DIP2)) val |= 0x02;
  return constrain(val + 1, 1, 4);
}

bool isMuted() {
  return !digitalRead(DIP4);  // Active LOW
}

bool useRandomTracks() {
  return digitalRead(DIP3);   // HIGH = Random
}

// ----- Track Playback -----
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

// ----- Radar Parsing -----
void processLine(String line) {
  line.trim();
  int idx = line.indexOf("dis=");
  if (idx != -1) {
    lastDistance = line.substring(idx + 4).toFloat();
    if (lastDistance <= detectionRangeMeters) {
      if (!isMoving) movStart = millis();
      isMoving = true;
      lastMov = millis();
      lastSeenTime = millis();
      if (inCalmState && millis() - enteredCalmTime >= CALM_LOCK_MS) {
        inCalmState = false;
      }
    }
  }
}

// ----- LED Breathing -----
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

  // Set radar to normal mode and max range once
  radarSerial.println("test_mode=0");
  delay(50);
  radarSerial.println("rmax=4");
  delay(50);
  radarSerial.println("save");

  detectionRangeMeters = getRangeMetersFromDIP();
  lastSeenTime = millis();
  enteredCalmTime = millis();
  inCalmState = true;
  lastMillis = millis();
  lastDipUpdate = millis();
}

// ----- Main Loop -----
void loop() {
  // Radar input
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

  // Trigger playback
  if (isMoving && millis() - movStart >= DETECTION_TIME_MS) {
    if (!triggered && millis() - lastPlay >= COOLDOWN_MS) {
      triggered = true;
      lastPlay = millis();
      if (useRandomTracks()) {
        playTrack(random(1, 35));
      } else {
        playTrack(50);
      }
    }
  }

  if (millis() - lastMov > 250) {
    isMoving = false;
    triggered = false;
  }

  // DIP switch update (range only)
  if (millis() - lastDipUpdate > DIP_UPDATE_MS) {
    detectionRangeMeters = getRangeMetersFromDIP();
    lastDipUpdate = millis();
  }

  updateEyeLED();
}
