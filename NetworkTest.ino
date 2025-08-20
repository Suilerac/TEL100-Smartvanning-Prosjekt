#include "arduino_secrets.h"

#include <SPI.h>
#include <WiFiNINA.h>
#include <TimeLib.h>

char ssid[] = SECRET_SSID; // Can be removed if network connection happens in the main file
char pass[] = SECRET_PASS; // Can be removed if network connection happens in the main file
int status = WL_IDLE_STATUS; // Can be removed if network connection happens in the main file

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

// Just connecting to the network, all of it can be removed if you do this already in the main file
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  while (!Serial);

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to network: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }

  Serial.println("You're connected to the network");
  Serial.println("----------------------------------------");
  printData();
  Serial.println("----------------------------------------");
}

// Can be removed if it's not the main file
void loop() {
  // NMBU coordinates: 59.6685, 10.7619
  // Førde coordinates: 61.4535, 5.8503
  checkForRain(61.4535, 5.8503); // Placeholder NMBU coordinates
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

// Should be removed if you removed the other things, or it won't compile
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
