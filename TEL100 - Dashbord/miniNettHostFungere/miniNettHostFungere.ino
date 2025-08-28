#include <WiFiNINA.h>
#include <SD.h>
#include <SPI.h>
#include "veret.h"
#include <Arduino.h>

// Desired IP address
IPAddress ip(10, 46, 41, 61);
IPAddress dns(8, 8, 8, 8);
IPAddress subnet(255, 255, 255, 0);

// WiFi credentials
char ssid[] = "Lassenett";
char pass[] = "lassebass";

WiFiServer server(80);

// Variables received from website
bool indoorPlant = false;
float longitude = 59.66;
float latitude = 10.77;
float targetMoistLevel = 0.0;

// Variables sent to website
bool willRain = false;
int measuredMoistLevel = 55;
int measuredWaterLevel = 65;

// SD CS pin on ENV shield
#define SD_CS 4   

unsigned long lastRainCheck = -3600000;
const unsigned long rainCheckInterval = 0.5 * 60 * 1000; //checking every half minute

extern int __heap_start, *__brkval;


void setup() {
  Serial.begin(115200);
  delay(3000);

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.config(ip);
  int status = WiFi.begin(ssid, pass);
  

  while (status != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    WiFi.config(ip);
    status = WiFi.begin(ssid, pass);
  }
  delay(5000);
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  // Init SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed!");
    while (1);
  }
  Serial.println("SD init OK");

  // Start server
  server.begin();
  Serial.println("Server started.");
}
 
// Send static files (HTML, CSS, JS, GIF)
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

  String contentType = "text/plain";
  if (path.endsWith(".HTM")) contentType = "text/html";
  else if (path.endsWith(".CSS")) contentType = "text/css";
  else if (path.endsWith(".JS")) contentType = "application/javascript";
  else if (path.endsWith(".GIF")) contentType = "image/gif";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: " + contentType);
  client.println("Connection: close");
  client.println();

  uint8_t buf[64];
  int n;
  while ((n = file.read(buf, sizeof(buf))) > 0) {
    client.write(buf, n);
  }
  file.close();
}

// Send JSON status
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

// Handle POST /set with JSON body
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
  //checkForRain(43.7370, 7.4212);
  Serial.println("Waiting for NTP time...");
  while (WiFi.getTime() == 0) {
      delay(1000); // allows WiFiNINA to sync
  }
  Serial.println("Time synchronized!");

  if (millis() - lastRainCheck > 1000 * 60 * 60){
    Serial.println("Checking rain");
    lastRainCheck = millis();
    while ((millis() - lastRainCheck) < 30000){
      willRain = checkForRain(latitude, longitude);
    }
  }

  

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
