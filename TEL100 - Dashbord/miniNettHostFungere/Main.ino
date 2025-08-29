//imports necessary libraries
#include <WiFiNINA.h>
#include <SD.h>
#include <SPI.h>
#include "veret.h"
#include <Arduino.h>
#include <Ultrasonic.h>

//Pins to read sensors and drive the water pump
int motorPin = 5;
int echoPin = 3;
int trigPin = 4;
Ultrasonic ultrasonic(trigPin, echoPin);

//Variables sent to website
bool willRain = false;
int measuredMoistLevel = 55;
int measuredWaterLevel = 65;

//Variables received from website
bool indoorPlant = false;
float longitude = 59.66;
float latitude = 10.77;
float targetMoistLevel = 0.0;

//WiFi credentials
char ssid[] = "Lassenett"; //Enter SSID of desired network
char pass[] = "lassebass"; //Enter password of desired network

//Uses socket 80 (HTTP) to host website
WiFiServer server(80);

//SD CS pin on ENV shield
#define SD_CS 4   

//lastRainCheck is -3600000 because the arduino will spend 20 seconds at the start to get weather data, after that it will get the data once per hour
unsigned long lastRainCheck = -3600000;
int lastSensorCheck = 0;
int lastWatering = 0;

void setup() {
  //Sets pinMode
  pinMode(motorPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);

  //Initialize Serial
  Serial.begin(115200);
  delay(3000);

  //Connect to WiFi
  Serial.print("Connecting to WiFi...");
  int status = WiFi.begin(ssid, pass);
  

  while (status != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    status = WiFi.begin(ssid, pass);
  }
  delay(5000);
  Serial.println(WiFi.getTime());
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //Init SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed!");
    while (1);
  }
  Serial.println("SD init OK");

  //Start server
  server.begin();
  Serial.println("Server started.");
}
 
//Send static files (HTML, CSS, JS, GIF)
void sendFile(WiFiClient &client, String path) {
  if (path == "/") path = "/INDEX.HTM";
  path.toUpperCase();

  String fullPath = "/TEL100" + path;
  File file = SD.open(fullPath.c_str());
  if (!file) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Connection: close");
    client.println();
    client.println("<h1>404 Not Found</h1>");
    client.println("File not found: " + fullPath);
    return;
  }

  //Tells the browser what type of http response to expect
  String contentType = "text/plain";
  if (path.endsWith(".HTM")) contentType = "text/html";
  else if (path.endsWith(".CSS")) contentType = "text/css";
  else if (path.endsWith(".JS")) contentType = "application/javascript";
  else if (path.endsWith(".GIF")) contentType = "image/gif";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: " + contentType);
  client.println("Connection: close");
  client.println();

  //Uses a buffer write to the website without using all the available memory
  uint8_t buf[64];
  int n;
  while ((n = file.read(buf, sizeof(buf))) > 0) {
    client.write(buf, n);
  }
  file.close();
}

//Sends JSON status
void sendStatus(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  client.print("{\"measuredMoistLevel\":");
  client.print(measuredMoistLevel);
  client.print(",\"measuredWaterLevel\":");
  client.print(measuredWaterLevel);
  client.print(",\"willRain\":");
  client.print(willRain ? "true" : "false");
  client.println("}");
}

//Handle POST / set with JSON body
void handleSet(WiFiClient &client, String body) {
  // Very basic JSON parsing (since Arduino doesn’t have a full JSON lib)
  if (body.indexOf("indoorPlant") >= 0) {
    indoorPlant = body.indexOf("\"indoorPlant\":true") >= 0;
  }
  int idx = body.indexOf("\"longitude\":");
  if (idx >= 0) longitude = body.substring(idx + 12).toFloat();

  idx = body.indexOf("\"latitude\":");
  if (idx >= 0) latitude = body.substring(idx + 11).toFloat();

  idx = body.indexOf("\"targetMoistLevel\":");
  if (idx >= 0) targetMoistLevel = body.substring(idx + 19).toFloat();

  //Serial prints what it read from the website.
  Serial.println("Updated settings:");
  Serial.print("Indoor: "); Serial.println(indoorPlant);
  Serial.print("Lat: "); Serial.println(latitude, 4);
  Serial.print("Lon: "); Serial.println(longitude, 4);
  Serial.print("Target moist: "); Serial.println(targetMoistLevel);

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println("{\"status\":\"ok\"}");
}

void loop() {
  //Gives the arudino 20 seconds to connect to the MET api every hour (It is run in a loop because the method is made to work that way)
  if (millis() - lastRainCheck > 1000 * 60 * 60){
    Serial.println("Checking rain");
    lastRainCheck = millis();
    while ((millis() - lastRainCheck) < 20000){
      willRain = checkForRain(latitude, longitude);
    }
  }
  //Checks sensor values every 3 seconds
  if (millis() - lastSensorCheck > 3000){
    setSensorValues();
  }

  //If indoor plant box is checked on website, ignore weather
  if (indoorPlant){
    willRain = false;
  }
  //Waters the plant every 30 seconds if the moisture is less than wanted moisture and if it is outside
  if (millis() - lastWatering > 30000){
    if ((measuredMoistLevel < targetMoistLevel) && (!willRain)) {
      digitalWrite(motorPin, HIGH);
      delay(1000);
      digitalWrite(motorPin, LOW);
      lastWatering = millis();
    }
  }

  //If a client is connected, handle sending files and reading from website
  WiFiClient webClient = server.available();
  if (webClient) {
    Serial.println("New client connected.");
    String req = webClient.readStringUntil('\n');
    Serial.println("Request: " + req);

    //Parse method and path
    int methodEnd = req.indexOf(' ');
    String method = req.substring(0, methodEnd);
    int pathEnd = req.indexOf(' ', methodEnd + 1);
    String path = req.substring(methodEnd + 1, pathEnd);

    //Handle GET /status
    if (method == "GET" && path.startsWith("/status")) {
      sendStatus(webClient);
    }
    //Handle POST /set
    else if (method == "POST" && path.startsWith("/set")) {
      // Read headers until empty line
      while (webClient.available()) {
        String line = webClient.readStringUntil('\n');
        if (line == "\r" || line == "") break;
      }
      //Read body
      String body = "";
      while (webClient.available()) {
        body += (char)webClient.read();
      }
      handleSet(webClient, body);
    }
    //Serve static files
    else {
      sendFile(webClient, path);
    }

    delay(1);
    webClient.stop();
    Serial.println("Client disconnected.");
  }
}

//Sets sensor values and maps them to their respective percentages to be used on the website
void setSensorValues() {
  delay(10);
  int waterLevel = ultrasonic.read();
  measuredWaterLevel = map(waterLevel, 3, 24, 100, 0);
  measuredMoistLevel = map(analogRead(A6), 400, 800, 100, 0);
}
