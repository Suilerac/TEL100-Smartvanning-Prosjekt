//imports the needed libraries
#include <WiFiNINA.h>
#include <SD.h>
#include <Arduino_MKRENV.h>
#include <SPI.h>
#include <WiFiSSLClient.h>
#include <TimeLib.h>



//SSID and Password of network
char SSID [] = "Lassenett";
char PASS [] = "lassebass";
int status = WL_IDLE_STATUS;

//pin for SD card
const int ChipSelect = 4;

//wifi server socket to host the web server
WiFiServer server(80);
WiFiClient wificlient = server.available();

String readString, pos;

//SD setup
Sd2Card card;
SdVolume volume;
SdFile root;

File html_file;

//Weather API setup
WiFiSSLClient client;
int HTTPS_PORT = 443;
char HOST_NAME[] = "api.met.no";

String PATH_NAME;
String USER_AGENT;

int RFC1123Length = 29; // Length of an RFC1123 formatted date string
String expires = "";
String lastModified = "";
unsigned long expiresUnix = 0;
unsigned long lastModifiedUnix = 0;

unsigned long rateLimitTimeStamp = 0;
bool deprecated = false;

int yrData[24][2];


void setup() {
  //intializes Serial communication
  Serial.begin(9600);
  delay(5000);

  //initializes ENV shield
  if (!ENV.begin()){
    Serial.println("Failed to initialize MKR ENV shield.");
  }

  //Connects to internet
  while (status != WL_CONNECTED){
  Serial.println("attempring to connect to network...");
  status = WiFi.begin(SSID, PASS);
  delay(5000);
  }
  Serial.println("You are connected to the network.");
  printData();

  //checks if SD card is present
  Serial.println();
  Serial.println("Connecting to SD card");
  if (!SD.begin(ChipSelect)) {
  Serial.println("SD.begin() failed!");
  while (true);
}
Serial.println("SD initialized.");

  //prints card data
  Serial.println();
  Serial.print("Card type:         ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }

  File root = SD.open("/");
  listFiles(root, 0);

  server.begin();
}


void loop() {
  wificlient = server.available();
  if (wificlient){
    Serial.println("New client connected");
    String currentline = "";
    String req = "";

    while (wificlient.connected()){
      if (wificlient.available()){
        char c = wificlient.read();  
        req += c;

        if (c == '\n'){
          if (currentline.length() == 0) {

            int firstSpace = req.indexOf(' ');
            int secondSpace = req.indexOf((' ', firstSpace + 1));
            String path = req.substring(firstSpace + 1, secondSpace);

            if (path.startsWith("\?")){
              handleInput(path);
              wificlient.println("HTTP/1.1 200 OK");
              wificlient.println("Content-Type: text/plain");
              wificlient.println("Connection: close");
              wificlient.println();
              wificlient.println("Command received");
            } else {
              sendFile(wificlient, path);
            }
            break;

          }
          currentline =  "";
        }
        else if (c != '\r'){
          currentline += c;
        }
      }
    }
    delay(1);
    client.stop();
    Serial.println("Client Disconnected");
  }  
}

File get_html() {
  if (!SD.exists("TEL100/INDEX.HTM")){
    Serial.println("Couldn't find index.html file");
    while(true);
  }
  html_file = SD.open("INDEX.HTM");

  return html_file;
}

//Function to send files to the client from the SD card
void sendFile(WiFiClient &client, String path) {
  // --- map browser paths to SD card files ---
  path.toUpperCase();
  String fullPath = "/TEL100";

  if (path == "/" || path.equalsIgnoreCase("/index.html")) {
    fullPath += "/INDEX.HTM";
  } else if (path.endsWith(".css")) {
    fullPath += "/INDEX.CSS";
  } else if (path.endsWith(".js")) {
    fullPath += "/INDEX.JS";
  } else if (path.endsWith(".gif")) {
    fullPath += "/PLANTE.GIF";
  } else {
    // if user requests something unknown
    fullPath += path;  
  }

  // --- decide MIME type ---
  String contentType = "text/plain";
  if (fullPath.endsWith(".HTM")) contentType = "text/html";
  else if (fullPath.endsWith(".CSS")) contentType = "text/css";
  else if (fullPath.endsWith(".JS")) contentType = "application/javascript";
  else if (fullPath.endsWith(".GIF")) contentType = "image/gif";

  // --- try to open file ---
  File file = SD.open(fullPath.c_str());
  if (!file) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println("404 File Not Found: " + fullPath);
    return;
  }

  // --- send HTTP headers ---
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: " + contentType);
  client.println("Connection: close");
  client.println();

  // --- send file content ---
  uint8_t buf[64];
  int n;
  while ((n = file.read(buf, sizeof(buf))) > 0) {
    client.write(buf, n);
  }

}



bool checkForRain(float lat, float lon) {
  // If we're rate limited we wait 10 minutes before trying a new request
  // We also wait until we pass the time marked by the expires header in the previous request
  // If the API is deprecated, it won't go through with any more requests
  PATH_NAME = "/weatherapi/locationforecast/2.0/compact?lat=" + String(lat)+ "&lon=" + String(lon);
  USER_AGENT = "NMBU_TEL100_StudentGruppeProsjekt andreas.carelius.brustad@nmbu.no";

  if (
    WiFi.getTime() > expiresUnix 
    && WiFi.getTime() > rateLimitTimeStamp + 600 
    && !deprecated
    ) {
    // Read response from MET Norge
    sendLocationForecastRequest();

    // Various String data variables
    // The JSON response is too big for the arduino, so we can't store the entire response, and as such can't parse the data as regular JSON
    String data = "";
    int responseCode;

    // Bools to check if we are in the correct part of the response
    bool checkRain = true;;
    bool correctTimeFrame = false;

    int hourIndex = 0; // Resets at 23 inclusively

    while (client.connected()) {

      if (client.available() > 0) {
        char c = client.read();
        data += c;

        // Response from MET Norge is too big for the arduinos memory, so at some point old data has to be removed as new data is added
        // The limit was decided arbitrarily, can likely be increased or decreased if needed
        while (data.length() > 50) {
          data.remove(0);
        }

        if (data.endsWith("HTTP/1.1 ")) {
          String responseCodeString = "";
          responseCodeString += (char)client.read();
          responseCodeString += (char)client.read();
          responseCodeString += (char)client.read();
          responseCode = responseCodeString.toInt();
        }

        // Handling of all response codes other than 200: Success before moving on to the rest of the method
        switch (responseCode) {
          case 203:
            Serial.println("HTTP Response Code 203: Deprecated Product");
            deprecated = true;
            return checkYrData();
          case 204:
            Serial.println("HTTP Response Code 204: No Content");
            return checkYrData();
          case 304:
            Serial.println("HTTP Response Code 304: Data Unmodified Since Last Request");
            return checkYrData();
          case 400:
            Serial.println("HTTP Response Code 400: Bad Request");
            return checkYrData();
          case 401:
            Serial.println("HTTP Response Code 401: Unauthorized");
            return checkYrData();
          case 403:
            Serial.println("HTTP Response Code 403: Forbidden");
            return checkYrData();
          case 404:
            Serial.println("HTTP Response Code 404: Not Found");
            return checkYrData();
          case 422:
            Serial.println("HTTP Response Code 422: Unprocessable Entity");
            return checkYrData();
          case 429:
            Serial.println("HTTP Response Code 429: Too Many Requests");
            rateLimitTimeStamp = WiFi.getTime();
            return checkYrData();
          case 500:
            Serial.println("HTTP Response Code 500: Internal Server Error");
            return checkYrData();
          case 502:
            Serial.println("HTTP Response Code 502: Bad Gateway");
            return checkYrData();
          case 503:
            Serial.println("HTTP Response Code 503: Service unavailable");
            return checkYrData();
          case 504:
            Serial.println("HTTP Response Code 504: Gateway Timeout");
            return checkYrData();
        }

        // If it got through the switch case without a return, the only remaining option is 200: Success, and we can move on

        if (data.endsWith("Expires: ")) {
          for (int i = 0; i < RFC1123Length; i++) {
            expires += (char)client.read();
          }
        }
        if (data.endsWith("Last-Modified: ")) {
          for (int i = 0; i < RFC1123Length; i++) {
            lastModified += (char)client.read();
          }
        }
        tmElements_t yrTime;
        bool newTime = false;
        // You get various data for different times. Here we go through each one of those times, and move forward with the one we want based on the current time
        if (data.endsWith("\"time\":\"") && checkRain) {
          String yrTimeString = "";
          for (int i = 0; i < 20; i++) { // The format for time is length 20. Example: 2025-08-18T10:00:00Z
            yrTimeString += (char)client.read();
          }
          int yrYear, yrMonth, yrDay, yrHour, yrMinute, yrSecond;
          sscanf(yrTimeString.c_str(), "%4d-%2d-%2dT%2d:%2d:%2dZ", &yrYear, &yrMonth, &yrDay, &yrHour, &yrMinute, &yrSecond);

          yrTime.Year = yrYear;
          yrTime.Month = yrMonth;
          yrTime.Day = yrDay;
          yrTime.Hour = yrHour;
          yrTime.Minute = yrMinute;
          yrTime.Second = yrSecond;

          yrData[hourIndex][0] = (int)yrTime.Hour;

          if (hourIndex == 23) {
            checkRain = false; // Stop checking data past the first 24 hours
            hourIndex = 0;
          } else {
            hourIndex++;
          }
          newTime = true;
        }
        // If we should check rain in this part of the data, we then check if we're in the correct timeframe
        if (data.endsWith("next_12_hours") && checkRain) {
          correctTimeFrame = true;
        } else if (data.endsWith("hours")) { // If it's not the next 12 hours, then it's the incorrect timeframe
          correctTimeFrame = false;
        }
        // Check if we've reached the symbol code in the correct timeframe
        if (data.endsWith("symbol_code") && checkRain && correctTimeFrame) {
          // There are a lot of weather icons for rain with varying lengths, so I parse the entire weather icon name and then check if it contains "rain"
          String weatherIcon = "";
          c = client.read();
          while (c != '}') {
            weatherIcon += c;
            c = client.read();
          }
          if (weatherIcon.indexOf("rain") >= 0) {
            yrData[hourIndex - 1][1] = 1; // hourIndex is updated before reaching this code, so we subtract one
          } else {
            yrData[hourIndex - 1][1] = 0; // hourIndex is updated before reaching this code, so we subtract one
          }
        }
        newTime = false;
      }
    }

    data = ""; // Due to the dynamic nature of this string, all data handling needs to be done in the while loop, and so I can free up the unnecessary space this string is taking for the rest of the method

    if (!client.connected()) {
      Serial.println("Disconnected from MET Norge");
      client.stop();
    }

    // Maximum value for unsigned long. Testing variable so we don't get rate limited and potentially blocked if we read the expires token wrong
    // If the field gets read correctly, this will be corrected anyway
    expiresUnix = 4294967295;

    // Variables needed to handle the next request optimally
    expiresUnix = convert_RFC1123_to_unix(expires);
    lastModifiedUnix = convert_RFC1123_to_unix(lastModified);

    Serial.print("Will rain: ");
    Serial.println(checkYrData());
  }
  return checkYrData();
}

bool checkYrData() {
  tmElements_t currentTime = convert_unix_to_RFC1123(WiFi.getTime());
  for (int i = 0; i < 24; i++) {
    int hour = yrData[i][0];
    int willRain = yrData[i][1]; // 1 if yes, 0 if no
    // Sometimes the code misinterprets and thereby misses some hours, so we check for several as a failsafe
    if (
      hour == (int)currentTime.Hour 
      || hour == (int)currentTime.Hour + 1 
      || hour == (int)currentTime.Hour + 2 
      || hour == (int)currentTime.Hour + 3
      ) {
      if (willRain == 1) {
        return true;
      } else {
        return false;
      }
    }
  }
}

unsigned long convert_RFC1123_to_unix(String RFC1123String) {
  // RFC1123 will always be set up with the same number of elements in the same positions, so we can seperate them easily
  char weekday[4], monthString[4];
  int day, year, hour, minute, second;
  // Format example: Mon, 18 Aug 2025 16:14:41 GMT
  sscanf(RFC1123String.c_str(), "%3s, %2d %3s %4d %2d:%2d:%2d GMT", weekday, &day, monthString, &year, &hour, &minute, &second);
  
  int month = monthConversion((String)monthString);

  if (month == 0) { // Unproperly formatted month, returns max value so we don't get rate limited
    Serial.println("Malformed month char array");
    return 4294967295;
    }

  tmElements_t tm;
  tm.Year = year;
  tm.Month = month;
  tm.Day = day;
  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;
  return (unsigned long)makeTime(tm);
}

tmElements_t convert_unix_to_RFC1123(time_t unixTime) {
  tmElements_t tm;
  breakTime(unixTime, tm);
  return tm;
}

int monthConversion(String month) {
  if (month == "Jan") { return 1; }
  else if (month == "Feb") { return 2; }
  else if (month == "Mar") { return 3; }
  else if (month == "Apr") { return 4; }
  else if (month == "May") { return 5; }
  else if (month == "Jun") { return 6; }
  else if (month == "Jul") { return 7; }
  else if (month == "Aug") { return 8; }
  else if (month == "Sep") { return 9; }
  else if (month == "Oct") { return 10; }
  else if (month == "Nov") { return 11; }
  else if (month == "Dec") { return 12; }
  else {return 0;} // Unproperly formatted month
}


void sendLocationForecastRequest() {
  if (client.connect(HOST_NAME, HTTPS_PORT)) {
    Serial.print("Connected to server: ");
    Serial.println(String(HOST_NAME));

    // Send HTTPS request header
    client.println("GET " + PATH_NAME + " HTTP/1.1");
    client.println("Host: " + String(HOST_NAME));
    client.println("User-Agent: " + USER_AGENT);

    if (lastModified != "") { // Subsequent requests can save time by having this header
      client.println("If-Modified-Since" + lastModified);
    }
    
    client.println("Accept: application/json");
    client.println("Connection: close");
    client.println(); // End HTTPS request header
  } else {
    Serial.println("Connection failed");
  }
}


void printData() {
  Serial.println("Board Information:");
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  Serial.println();
  Serial.println("Network Information:");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.println(rssi);
}


void handleInput(String path) {
  if (path.indexOf("pump=on") > 0) {
    Serial.println("Turning pump ON");
    digitalWrite(7, HIGH);  // example pin
  }
  else if (path.indexOf("pump=off") > 0) {
    Serial.println("Turning pump OFF");
    digitalWrite(7, LOW);
  }
  else if (path.indexOf("led=toggle") > 0) {
    Serial.println("Toggling LED");
    digitalWrite(6, !digitalRead(6));
  }
}


void listFiles(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    for (uint8_t i=0; i<numTabs; i++) Serial.print('\t');
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      listFiles(entry, numTabs+1);
    } else {
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}


String mapPath(String path) {
  // Default page
  if (path.endsWith("/") || path.equalsIgnoreCase("/index.html")) {
    return "/INDEX.HTM";
  }

  // CSS
  if (path.endsWith(".css")) {
    return "/INDEX.CSS";
  }

  // JS
  if (path.endsWith(".js")) {
    return "/INDEX.JS";
  }

  // Image
  if (path.endsWith(".gif")) {
    return "/PLANTE.GIF";
  }

  // Return unchanged if no mapping needed
  return path;
}


void handleRequest(String path, WiFiClient &client) {
  String sdPath = mapPath(path);

  File file = SD.open(sdPath);
  if (!file) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("File not found: " + sdPath);
    return;
  }

  // send headers
  client.println("HTTP/1.1 200 OK");
  if (sdPath.endsWith(".HTM")) client.println("Content-Type: text/html");
  else if (sdPath.endsWith(".CSS")) client.println("Content-Type: text/css");
  else if (sdPath.endsWith(".JS")) client.println("Content-Type: application/javascript");
  else if (sdPath.endsWith(".GIF")) client.println("Content-Type: image/gif");
  client.println();

  // send content
  while (file.available()) {
    client.write(file.read());
  }
  file.close();
}

