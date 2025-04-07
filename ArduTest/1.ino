#define LED_PIN 0  // PA4
#define radarSerial Serial1

String buffer = "";
bool responded = false;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  radarSerial.begin(115200);
  delay(200);

  // Try to send a known radar command
  radarSerial.print("version\r\n");
}

void loop() {
  // Blink while waiting
  if (!responded) {
    digitalWrite(LED_PIN, millis() % 500 < 250);
  }

  while (radarSerial.available()) {
    char c = radarSerial.read();
    buffer += c;

    if (c == '\n' || c == '\r') {
      if (buffer.indexOf("version") >= 0 || buffer.length() > 5) {
        // Radar is talking back
        digitalWrite(LED_PIN, HIGH);
        responded = true;
      }
      buffer = "";
    }
  }
}
