#include <Arduino.h>

// ----- DIP Switch Pins -----
#define DIP0 PA5
#define DIP1 PA6
#define DIP2 PA7
#define MODE_PIN PB5

// ----- Debug UART (Software Serial on PC2 TX only) -----
#define DEBUG_TX_PIN PC2

// ----- Constants -----
#define DETECTION_TIME_MS 2000
#define COOLDOWN_MS       300000UL  // 5 minutes
#define MAX_TRACK         34

// ----- Serial Aliases -----
#define radarSerial Serial1   // HLK-LD1125H (USART1)
#define mp3Serial   Serial0   // DY-HV8F (USART0)

// ----- State Tracking -----
String radarBuffer = "";
bool isMoving = false;
bool triggered = false;
unsigned long movStart = 0;
unsigned long lastMov = 0;
unsigned long lastPlay = 0;

// ----- Setup DIP switches -----
uint8_t getRmaxFromDip() {
  uint8_t val = 0;
  if (!digitalRead(DIP0)) val |= 0x01;
  if (!digitalRead(DIP1)) val |= 0x02;
  if (!digitalRead(DIP2)) val |= 0x04;
  return max(val, 1); // Minimum 1m
}

// ----- Send command to radar -----
void sendRadarCommand(const String& cmd) {
  radarSerial.print(cmd + "\r\n");
}

// ----- Configure radar with distance -----
void configureRadar(uint8_t rmax) {
  sendRadarCommand("test_mode=0");
  delay(50);
  sendRadarCommand("rmax=" + String(rmax));
  delay(50);
  sendRadarCommand("save");
}

// ----- Play MP3 Track (DY-HV8F) -----
void playTrack(uint8_t track) {
  mp3Serial.write(0x7E);     // Start byte
  mp3Serial.write(0x03);     // Play command
  mp3Serial.write(track);    // Track number
  mp3Serial.write(0xEF);     // End byte
  Serial.print("🎵 Playing track ");
  Serial.println(track);
}

// ----- Setup -----
void setup() {
  // DIP Inputs
  pinMode(DIP0, INPUT_PULLUP);
  pinMode(DIP1, INPUT_PULLUP);
  pinMode(DIP2, INPUT_PULLUP);
  pinMode(MODE_PIN, INPUT_PULLUP);

  // Debug Serial (PC2 = TX)
  Serial.begin(9600);
  Serial.println("🛠 Debug serial ready (PC2 TX)");

  // Device UARTs
  mp3Serial.begin(9600);      // DY-HV8F (USART0)
  radarSerial.begin(115200);  // HLK-LD1125H (USART1)

  delay(500);  // let devices power up

  // Configure radar
  uint8_t rmax = getRmaxFromDip();
  configureRadar(rmax);
  Serial.print("📡 Radar configured for ");
  Serial.print(rmax);
  Serial.println("m detection range");
}

// ----- Loop -----
void loop() {
  // Read radar lines
  while (radarSerial.available()) {
    char c = radarSerial.read();
    if (c == '\n' || c == '\r') {
      if (radarBuffer.length() > 0) {
        processRadarLine(radarBuffer);
        radarBuffer = "";
      }
    } else {
      radarBuffer += c;
    }
  }

  // Check motion state
  if (isMoving && millis() - movStart >= DETECTION_TIME_MS) {
    if (!triggered && millis() - lastPlay >= COOLDOWN_MS) {
      triggered = true;
      lastPlay = millis();

      bool modeHigh = digitalRead(MODE_PIN);
      if (modeHigh) {
        playTrack(50);
      } else {
        playTrack(random(1, MAX_TRACK + 1));
      }
    }
  }

  // Reset if motion stopped
  if (millis() - lastMov > 250) {
    isMoving = false;
    triggered = false;
  }
}

// ----- Parse radar output line -----
void processRadarLine(String line) {
  line.trim();
  if (line.startsWith("mov")) {
    float dis = line.substring(line.indexOf("=") + 1).toFloat();
    uint8_t rmax = getRmaxFromDip();
    if (dis <= rmax) {
      if (!isMoving) {
        movStart = millis();
        Serial.println("➡️ Movement started");
      }
      isMoving = true;
      lastMov = millis();
    }
  }
}
/*
  DIP Switch Settings for Detection Range (PA5 = LSB, PA7 = MSB)

  | PA7 | PA6 | PA5 | Binary | Range (rmax) |
  |-----|-----|-----|--------|--------------|
  |  0  |  0  |  0  | 0b000  | 1 meter      |
  |  0  |  0  |  1  | 0b001  | 1 meter      |
  |  0  |  1  |  0  | 0b010  | 2 meters     |
  |  0  |  1  |  1  | 0b011  | 3 meters     |
  |  1  |  0  |  0  | 0b100  | 4 meters     |
  |  1  |  0  |  1  | 0b101  | 5 meters     |
  |  1  |  1  |  0  | 0b110  | 6 meters     |
  |  1  |  1  |  1  | 0b111  | 7 meters     |

  Note:
  - DIP switches use INPUT_PULLUP, so "0" = switch ON (connected to GND)
  - Values below 1 are clamped to 1 meter in the code for safety
*/