/***************************************************
 Example file for using the time keeping library.
 
 License: Creative Common V1. 

 Benjamin Voelker, voelkerb@me.com
 Embedded Systems Engineer
 ****************************************************/
#include "timeHandling.h"
#include "multiLogger.h"

#if defined(ESP32)
#include "WiFi.h"
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

// Germany is +1 Hour (DST is handled in library)
#define LOCATION_TIME_OFFSET 3600 
char * timeServer = "time.google.com";

// Wifi credentials
char* SSID = "YourNetworkName";
char* PWD =  "YourPassword";

// Printed to console in front of log text to indicate logging
char * LOG_PREFIX_SERIAL = " - ";

// Create singleton here
MultiLogger& logger = MultiLogger::getInstance();
StreamLogger serialLog((Stream*)&Serial, &timeStr, &LOG_PREFIX_SERIAL[0], DEBUG);

// DST start in Germany at first Saturday (day 7 of week) in March (3) after the 25th.
// DST stops at first Saturday (day 7 of week) in October (10) after the 25th.
// DST Shift is 1hour = 3600s
// {active, startdayOfWeek, startMon, startAfterDay, stopdayOfWeek, stopMon, stopAfterDay, seconds}
DST dstGermany = { true, 7, 3, 25, 7, 10, 25, 3600};

// callback function if NTP sync happened
void ntpSynced(unsigned int confidence);
TimeHandler myTime(timeServer, LOCATION_TIME_OFFSET, NULL, &ntpSynced, dstGermany);
Timestamp then;

void setup() {
  Serial.begin(9600);
  // Add seriallogger to multilogger
  logger.addLogger(&serialLog);
  // Init the logging modules
  logger.setTimeGetter(&timeStr);
  
  // Connect to WiFi
  WiFi.begin(SSID, PWD);
  logger.append("Connecting to WiFi.");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    logger.append(".");
  }
  logger.flushAppended(INFO);
  logger.log(INFO, "Connected to the WiFi network: %s with IP: %s", SSID, WiFi.localIP().toString().c_str());

  // Initial NTP update after connecting to WiFi
  // Actively wait for finish
  myTime.updateNTPTime();
  then = myTime.timestamp();
  logger.log(INFO, "Current time: %s", myTime.timeStr());
}

void loop() {
  Timestamp now = myTime.timestamp();
  if ((now.seconds-then.seconds) >= 10) {
    logger.log(INFO, "Another 10s passed");
    logger.log(INFO, "Current time: %s", myTime.timeStr());
    then = now;
  }
}

/****************************************************
 * Callback when NTP syncs happened and
 * it's estimated confidence in ms
 ****************************************************/
void ntpSynced(unsigned int confidence) {
  logger.log(DEBUG, "NTP synced with conf: %lu", confidence);
}

/****************************************************
 * Used for multilogger instance to produce time stamp
 ****************************************************/
char * timeStr() {
  return myTime.timeStr(true);
}
