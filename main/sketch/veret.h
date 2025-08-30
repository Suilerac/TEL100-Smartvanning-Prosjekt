#include <SPI.h>
#include <WiFiNINA.h>
#include <TimeLib.h>

#ifndef VERET_H
#define VERET_H

bool checkForRain(float lat, float lon);
bool checkYrData();
unsigned long convert_RFC1123_to_unix(String rfc1123String);
tmElements_t convert_unix_to_RFC1123(time_t unixTime);
int monthConversion(String month);
void sendLocationForecastRequest();

#endif
