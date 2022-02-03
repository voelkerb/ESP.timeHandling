/***************************************************
 Library for time keeping.
 
 License: Creative Common V1. 

 Benjamin Voelker, voelkerb@me.com
 Embedded Systems Engineer
 ****************************************************/

#ifndef TIME_HANDLING_h
#define TIME_HANDLING_h

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <time.h> 
#include <RTClib.h> 
#if defined(ESP32)
#include <WiFi.h>
// #include <FreeRTOS.h>
#else
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif
// Go back to src folder of sketch
#include "../../multiLogger/src/multiLogger.h"

#define _MAX_TIME_STR_LENGTH 42
#define _MAX_DOW_STR_LENGTH 10
// NTP time stamp is in the first 48 bytes
#define PACKET_SIZE_NTP 48 

// Update interval to get new NTP time
#define NTP_UPDATE_INTV 120000
// Try NTP only if we have not tried last 5s
#define NTP_TRY_INTV 30000

struct DST {
  bool active;
  uint8_t startdayOfWeek;
  uint8_t startMon;
  uint8_t startAfterDay;
  uint8_t stopdayOfWeek;
  uint8_t stopMon;
  uint8_t stopAfterDay;
  uint32_t seconds;
} ;


struct Timestamp {
  uint32_t seconds;
  uint32_t milliSeconds;
} ;


enum class Weekday{SUNDAY, MONDAY, TUESDAY, WEDNESDAY, THRUSDAY, FRIDAY, SATURDAY};

static const char * const WEEKDAYS[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
static const char * const WEEKDAYS_SHORT[] = {
    "Su.", "Mo.", "Tu.", "We.", "Th.", "Fr.", "Sa."
};

class TimeHandler {

  public:
    // Constructor
    TimeHandler(char * ntpServerName, int locationOffset, 
                void (*ntpSyncCB)(unsigned int)=NULL, // optional ntp sync callback
                DST dst={ true, 7, 3, 25, 7, 10, 25, 3600}, MultiLogger * logger=NULL ); // optional Daylight saving time struct. Default: Germany

    // Function to call if NTP should be updated
    // If wait is false, the update is performed in a background task
#if defined(ESP32)
    bool updateNTPTime(bool wait=true);
#else
    bool updateNTPTime();
#endif

    // Various function to time to string with milliseconds support
    char * timeStr(bool shortForm=false);
    char * timeStr(Timestamp ts, bool shortForm=false);
    char * timeStr(unsigned long s, unsigned long ms, bool shortForm=false);
    char * timeStr(DateTime dt, unsigned long ms, bool shortForm=false);

    // Get a string of unix timestamp "<unix_seconds>.<ms>"
    char * timestampStr(bool shortForm=false);
    char * timestampStr(Timestamp ts, bool shortForm=false);
    char * timestampStr(unsigned long s, unsigned long ms, bool shortForm=false);

    uint8_t year();
    uint8_t month();
    uint8_t date();
    uint8_t hour();
    uint8_t minute();
    uint8_t second();
    Timestamp timestamp();

    // get current seconds
    unsigned long utc_seconds();
    // get current milliseconds
    unsigned long milliseconds();
    // update _currentMilliseconds and _currentSeconds
    void update();
    // if ntp time is valid
    bool valid();
    DateTime timeZoneDST(unsigned long s);

    Weekday dayOfTheWeek();
    Weekday dayOfTheWeek(Timestamp ts);
    Weekday dayOfTheWeek(DateTime dt);
    char * dayOfTheWeekStr(bool shortForm=false);
    char * dayOfTheWeekStr(Timestamp ts, bool shortForm=false);
    char * dayOfTheWeekStr(DateTime dt, bool shortForm=false);

  private:
    // Get day of week based on day given
    int _dow(int y, int m, int d);

#if defined(ESP32)
    // Decorator function (static function required for freertos create task)
    // Effectively, this function calls _updateNTPTime() 
    static void _startUpdateNTPTime(void * pvParameters);
#endif

    // Updates ntp time, makes sanity check and updates rtc
    bool _updateNTPTime();

    // Try to get accurate time over NTP
    bool _getTimeNTP();

#if defined(ESP32)
    // Task handle for the background sync
    TaskHandle_t _ntpTaskHandle;
#endif

    // Millis at which the last ntp try happened
    unsigned long _lastNTPTry;
    // Time string stored, used by the tostring methods
    char _timeString[_MAX_TIME_STR_LENGTH];
    char _timeStampStr[_MAX_TIME_STR_LENGTH];
    char _dayOfWeekStr[_MAX_DOW_STR_LENGTH];
    // IP address the time server is resolved to
    IPAddress _timeServerIP;
    // Use NTP server with hopefully a small ping
    char* _ntpServerName;
    // local port to listen for UDP packets
    unsigned int _localNTPPort;
    // buffer to hold incoming and outgoing udp data
    byte _ntpBuffer[PACKET_SIZE_NTP]; 
    // A UDP instance to let us send and receive packets over UDP
    WiFiUDP _udpNtp;
    // Offset of device location to standard time (utc?)
    int _locationOffset;
    // The internal clock ms at which the last ntp ready was valid
    unsigned long _ntpValidMillis;
    // The current time updated with _ntpTime + _ntpValidMillis
    Timestamp _currentTime;
    // The last valid ntp time 
    Timestamp _ntpTime;
    unsigned int _ntpConfidence;

    // NOTE: In order to be accurate and to represent the current time, 
    // the update() or timeStr() method must be called
    // The current time in seconds (not localtime but the same timezone as ntp)
    // Daylight saving time
    DST _dst;
    // utc timestamp between DST is active
    uint32_t _tsD1;
    uint32_t _tsD2;

    MultiLogger * logger; 

    // A callback on NTP success called
    void (*_ntpSyncCB)(unsigned int confidence);
};

#endif
