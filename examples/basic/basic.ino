/***************************************************
 Example file for using the time keeping library.
 
 License: Creative Common V1. 

 Benjamin Voelker, voelkerb@me.com
 Embedded Systems Engineer
 ****************************************************/

#include "timeHandling.h"
#if defined(ESP32)
#include "WiFi.h"
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

// Wifi credentials
char* SSID = "YourNetworkName";
char* PWD =  "YourPassword";

#define LOCATION_TIME_OFFSET 3600 // Germany is +1 Hour (DST is handled in library)
char * timeServer = "time.google.com";

TimeHandler myTime(timeServer, LOCATION_TIME_OFFSET, NULL, NULL);
Timestamp then;

void setup() {
  Serial.begin(9600);
  // Connect to WiFi
  WiFi.begin(SSID, PWD);
  Serial.print("Connecting to WiFi.");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to the WiFi network: ");
  Serial.print(SSID);
  Serial.print(" with IP: ");
  Serial.println(WiFi.localIP());

  // Initial NTP update after connecting to WiFi
  // Actively wait for finish
  myTime.updateNTPTime();
  then = myTime.timestamp();
  Serial.print("Current time: ");
  Serial.println(myTime.timeStr());
}

void loop() {
  Timestamp now = myTime.timestamp();
  if ((now.seconds-then.seconds) >= 10) {
    Serial.println("Another 10s passed");
    Serial.print("Current time: ");
    Serial.println(myTime.timeStr());
    then = now;
  }
}