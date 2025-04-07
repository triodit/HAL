#define LED_PIN 0
#define RADAR_BOOT_PIN 16
#define radarSerial Serial1

String radarBuffer = "";
float lastDistance = 99;

const float MIN_DIST = 0.5;
const float MAX_DIST = 2.0;
const int MIN_BRIGHT = 13;    // 5% of 255
const int MAX_BRIGHT = 255;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(RADAR_BOOT_PIN, OUTPUT);
  digitalWrite(RADAR_BOOT_PIN, HIGH);

  radarSerial.begin(115200);
  delay(300);
}

void loop() {
  // Read radar data
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

  // Update LED brightness
  if (lastDistance >= MIN_DIST && lastDistance <= MAX_DIST) {
    float norm = (lastDistance - MIN_DIST) / (MAX_DIST - MIN_DIST);  // 0–1
    norm = constrain(norm, 0.0, 1.0);
    int brightness = MIN_BRIGHT + norm * (MAX_BRIGHT - MIN_BRIGHT);
    analogWrite(LED_PIN, brightness);
  } else {
    analogWrite(LED_PIN, 0);  // Out of range = off
  }

  delay(10);  // Smooth update rate
}

void processLine(String line) {
  line.trim();
  int idx = line.indexOf("dis=");
  if (idx != -1) {
    lastDistance = line.substring(idx + 4).toFloat();
  }
}
