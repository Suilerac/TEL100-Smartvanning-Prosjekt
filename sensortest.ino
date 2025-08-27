#include <Ultrasonic.h>

int motorPin = 5;
int echoPin = 3; // Orange wire
int trigPin = 4; // Yellow wire
Ultrasonic ultrasonic(trigPin, echoPin);

int distance;
int moisture;

void setup() {
  Serial.begin(9600);
  Serial.println("Hello World!");
  pinMode(motorPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
}

void loop() {
  setSensorValues();
  
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println("cm");
  
  Serial.print("Moisture: ");
  Serial.print(moisture);
  Serial.println("%");

  if (moisture < 50) {
    digitalWrite(motorPin, HIGH);
  } else {
    digitalWrite(motorPin, LOW);
  }
}

void setSensorValues() {
  delay(10);
  distance = ultrasonic.read();
  moisture = map(analogRead(A6), 400, 800, 100, 0);
}


