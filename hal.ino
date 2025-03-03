#include <SoftwareSerial.h>
#include <EEPROM.h>

#define RADAR_RX 11  // Radar TX -> Arduino RX
#define RADAR_TX 12  // Radar RX -> Arduino TX
#define EEPROM_SENSITIVITY_ADDR 0
#define EEPROM_DISTANCE_ADDR 1  // EEPROM address for distance setting

SoftwareSerial radarSerial(RADAR_RX, RADAR_TX);
bool motionDetected = false;
bool presenceDetected = false;
int radarSensitivity = 5;
int maxDistance = 300;  // Default max detection distance (cm)

void setup() {
    Serial.begin(115200);
    radarSerial.begin(115200);

    int savedSensitivity = EEPROM.read(EEPROM_SENSITIVITY_ADDR);
    radarSensitivity = (savedSensitivity >= 0 && savedSensitivity <= 9) ? savedSensitivity : 5;
    
    int savedDistance = EEPROM.read(EEPROM_DISTANCE_ADDR);
    maxDistance = (savedDistance > 0 && savedDistance <= 500) ? savedDistance : 300;

    Serial.print("ACK_SEN,");
    Serial.println(radarSensitivity);
    Serial.print("ACK_DIS,");
    Serial.println(maxDistance);

    setRadarSensitivity(radarSensitivity);
    setRadarDistance(maxDistance);
}

void loop() {
    checkRadar();
    checkSerialCommands();
}

// Function to read radar data and send formatted response
void checkRadar() {
    while (radarSerial.available() > 4) {  
        int motionFlag = radarSerial.read();
        int presenceFlag = radarSerial.read();
        int distance = radarSerial.read() * 10; // Radar distance is in 10cm steps
        int signalStrength = radarSerial.read();

        Serial.print("RADAR,");
        Serial.print(motionFlag);
        Serial.print(",");
        Serial.print(presenceFlag);
        Serial.print(",");
        Serial.print(distance);
        Serial.print(",");
        Serial.println(signalStrength);
    }
}

// Function to process commands from Python
void checkSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "PING") {
            Serial.println("PONG");  
        } else if (command.startsWith("SET_SEN,")) {
            int newSensitivity = command.substring(8).toInt();
            if (newSensitivity >= 0 && newSensitivity <= 9) {
                radarSensitivity = newSensitivity;
                EEPROM.write(EEPROM_SENSITIVITY_ADDR, radarSensitivity);
                setRadarSensitivity(radarSensitivity);
                Serial.print("ACK_SEN,");
                Serial.println(radarSensitivity);
            }
        } else if (command.startsWith("SET_DIS,")) {
            int newDistance = command.substring(8).toInt();
            if (newDistance > 0 && newDistance <= 500) {
                maxDistance = newDistance;
                EEPROM.write(EEPROM_DISTANCE_ADDR, maxDistance);
                setRadarDistance(maxDistance);
                Serial.print("ACK_DIS,");
                Serial.println(maxDistance);
            }
        }
    }
}

// Function to set radar sensitivity
void setRadarSensitivity(int level) {
    radarSerial.print("AT+sen=");
    radarSerial.print(level);
    radarSerial.print("\r\n");
}

// Function to set radar detection distance
void setRadarDistance(int distance) {
    radarSerial.print("AT+dis=");
    radarSerial.print(distance);
    radarSerial.print("\r\n");
}
