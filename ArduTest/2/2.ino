#define DY_BUSY_PIN 12       // PB4 (LOW = playing)
#define DY_LED_PIN 0         // PA4 LED
#define mp3Serial Serial0    // UART0 to DY-HV8F

unsigned long lastPlayed = 0;
bool lastBusy = false;
bool isPlaying = false;
unsigned long lastStateChange = 0;

void setup() {
  pinMode(DY_BUSY_PIN, INPUT);
  pinMode(DY_LED_PIN, OUTPUT);
  digitalWrite(DY_LED_PIN, LOW);

  mp3Serial.begin(9600);
  delay(500);  // Allow DY-HV8F to initialize

  randomSeed(analogRead(1));  // Seed for randomness
}

void loop() {
  // Read and debounce BUSY pin
  bool currentBusy = (digitalRead(DY_BUSY_PIN) == LOW);
  if (currentBusy != lastBusy) {
    lastStateChange = millis();
    lastBusy = currentBusy;
  }

  if (millis() - lastStateChange > 100) {
    isPlaying = currentBusy;
  }

  // Update LED
  digitalWrite(DY_LED_PIN, isPlaying ? HIGH : LOW);

  // If not playing, wait 5s, then play random track
  if (!isPlaying && millis() - lastPlayed > 5000) {
    uint16_t track = random(1, 33);  // Track 1–32
    playTrack(track);
    lastPlayed = millis();
  }
}

// Send official DY-HV8F "play track" command
void playTrack(uint16_t track) {
  uint8_t cmd[] = {
    0xAA,           // Start code
    0x07,           // Play track command
    0x02,           // Length
    track >> 8,     // High byte
    track & 0xFF,   // Low byte
    0x00            // Placeholder for checksum
  };

  uint8_t sum = 0;
  for (int i = 0; i < 5; i++) sum += cmd[i];
  cmd[5] = sum & 0xFF;  // Checksum = low 8 bits

  for (int i = 0; i < 6; i++) {
    mp3Serial.write(cmd[i]);
  }
}
