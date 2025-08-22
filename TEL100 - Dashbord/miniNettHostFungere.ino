#include <WiFiNINA.h>
#include <SD.h>
#include <SPI.h>

// WiFi credentials
char ssid[] = "Lassenett";
char pass[] = "lassebass";

WiFiServer server(80);

#define SD_CS 4   // SD CS pin on ENV shield

void setup() {
  Serial.begin(115200);
  delay(5000);

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  int status = WiFi.begin(ssid, pass);
  while (status != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    status = WiFi.begin(ssid, pass);
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

  // Start server
  server.begin();
  Serial.println("Server started.");
}

void sendFile(WiFiClient &client, String path) {
  if (path == "/") path = "/INDEX.HTM";

  // uppercase file names
  path.toUpperCase();

  // prepend folder
  String fullPath = "/TEL100" + path;

  File file = SD.open(fullPath.c_str());
  if (!file) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Connection: close");
    client.println();
    client.println("<h1>404 Not Found</h1>");
    Serial.println("File not found: " + fullPath);
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

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected.");

    String req = client.readStringUntil('\r');
    Serial.println("Request: " + req);
    client.flush();

    // parse GET path
    int start = req.indexOf(' ');
    int end = req.indexOf(' ', start + 1);
    String path = req.substring(start + 1, end);

    // serve file
    sendFile(client, path);

    delay(1);
    client.stop();
    Serial.println("Client disconnected.");
  }
}
