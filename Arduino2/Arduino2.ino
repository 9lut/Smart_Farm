#include <avr/wdt.h>

#define Water_tank 11
#define Water_farm 10
#define Fan 9
#define LED 8

void setup() {
  Serial.begin(115200);
  for (int i=LED; i < 12; i++){
    pinMode(i, OUTPUT);
  }
  digitalWrite(LED, HIGH);
  digitalWrite(Water_tank, HIGH);
  digitalWrite(Water_farm, HIGH);
  digitalWrite(Fan, HIGH);

}


void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    // ประมวลผลคำสั่ง
    if (command == "PUMP1ON") {
      digitalWrite(Water_farm, LOW);
    } else if (command == "PUMP1OFF") {
      digitalWrite(Water_farm, HIGH);
    } else if (command == "PUMP2ON") {
      digitalWrite(Water_tank, LOW);
    } else if (command == "PUMP2OFF") {
      digitalWrite(Water_tank, HIGH);
    } else if (command == "Light_ON") {
      digitalWrite(LED, LOW);
    } else if (command == "Light_OFF") {
      digitalWrite(LED, HIGH);
    } else if (command == "Fan_ON") {
      digitalWrite(Fan, LOW);
    } else if (command == "Fan_OFF") {
      digitalWrite(Fan, HIGH);
    } else {
      Serial.println("Unknown command: " + command);
    }
  }
}


