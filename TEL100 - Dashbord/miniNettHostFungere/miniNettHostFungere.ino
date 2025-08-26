#include <WiFiNINA.h>
#include <SD.h>
#include <SPI.h>
#include "veret.h"

//desired IP address
IPAddress ip(10,46,41,61);


// WiFi credentials
char ssid[] = "Lassenett";
char pass[] = "lassebass";

WiFiServer server(80);

//variables to fetch from website
bool innendor = false;
float longitude = 0.0000;
float lattitude = 0.0000;
float needMoist = 0.0;

//variables to send to website
bool willRain = false;
int dirtmoist = 0;
int waterLevel = 0;

#define SD_CS 4   //SD CS pin on ENV shield

void setup() {
  //init Serial
  Serial.begin(115200);
  delay(5000);

  //Connect to WiFi
  Serial.print("Connecting to WiFi...");
  int status = WiFi.begin(ssid, pass);
  WiFi.config(ip);

  //Loop until connected to WiFi
  while (status != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    status = WiFi.begin(ssid, pass);
    WiFi.config(ip);
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Init SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed!");
    while (1);
  }
  Serial.println("SD init OK");

  //Start server
  server.begin();
  Serial.println("Server started.");
}

//Method to send static data like the website to client
void sendFile(WiFiClient &client, String path) {
  if (path == "/") path = "/INDEX.HTM";

  //uppercase file names
  path.toUpperCase();

  //To access the folder (arduino uses root)
  String fullPath = "/TEL100" + path;

  //Open file
  File file = SD.open(fullPath.c_str());
  if (!file) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Connection: close");
    client.println();
    client.println("<h1>404 Not Found</h1>");
    Serial.println("File not found: " + fullPath);
    return;
  }

  //If it tries to get a certain file it will get the correct file
  String contentType = "text/plain";
  if (path.endsWith(".HTM")) contentType = "text/html";
  else if (path.endsWith(".CSS")) contentType = "text/css";
  else if (path.endsWith(".JS")) contentType = "application/javascript";
  else if (path.endsWith(".GIF")) contentType = "image/gif";

  //Sends to client
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

//Sends data to browser
void sendStatus(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  client.print("{\"moisture\":");
  client.print(dirtmoist);
  client.print(",\"rain\":");
  client.print(willRain ? "true" : "false");
  client.print(",\"lat\":");
  client.print(lattitude, 4);
  client.print(",\"lon\":");
  client.print(longitude, 4);
  client.print(",\"needMoist\":");
  client.print(needMoist);
  client.print(",\"indoor\":");
  client.print(innendor ? "true" : "false");
  client.println("}");
}

void parseParams(String path) {
  int qIndex = path.indexOf('?');
  if (qIndex < 0) return;
  String query = path.substring(qIndex + 1);

  // Split into key=value pairs
  while (query.length() > 0) {
    int amp = query.indexOf('&');
    String pair;
    if (amp >= 0) {
      pair = query.substring(0, amp);
      query = query.substring(amp + 1);
    } else {
      pair = query;
      query = "";
    }

    int eq = pair.indexOf('=');
    if (eq < 0) continue;
    String key = pair.substring(0, eq);
    String val = pair.substring(eq + 1);

    if (key == "lat") lattitude = val.toFloat();
    else if (key == "lon") longitude = val.toFloat();
    else if (key == "needMoist") needMoist = val.toFloat();
    else if (key == "indoor") innendor = (val == "1" || val == "true");
  }
}

void loop() {
  checkForRain(43.7370, 7.4212);

  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected.");

    String req = client.readStringUntil('\r');
    Serial.println("Request: " + req);
    client.flush();

    //parse GET path
    int start = req.indexOf(' ');
    int end = req.indexOf(' ', start + 1);
    String path = req.substring(start + 1, end);

    //serve file
    parseParams(path);
    sendStatus(client);

    sendFile(client, path);

    delay(1);
    client.stop();
    Serial.println("Client disconnected.");
  }
}
