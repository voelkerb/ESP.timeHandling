# ESP.timeHandling
NTP and RTC time sync for ESP8266 and ESP32 with Arduino Environment

Requirements:
- [RTC](https://github.com/voelkerb/ESP.DS3231_RTC/)

Supports:
- [multiLogger](http://github.com/voelkerb/ESP.multiLogger) (-> see ```advanced.ino```)

```C++
#include "timeHandling.h"

// CB for successful ntp syncs
void ntpSynced(unsigned int confidence);

TimeHandler myTime(config.timeServer, LOCATION_TIME_OFFSET, NULL, &ntpSynced);

Timestamp then;

void setup() {
  Serial.begin(9600);
  // Connect to WiFi
  ...
  // Initial NTP update after connecting to WiFi
  myTime.updateNTPTime();
  then = myTime.timestamp();
}

void loop() {
  Timestamp now = myTime.timestamp();
  if ((now.seconds-then.seconds) >= 10) {
    Serial.println("Another 10s passed");
    Serial.print("Current time: ");
    Serial.print(myTime.timeStr());
    then = now;
  }
  ...
}

/****************************************************
 * Callback when NTP syncs happened and
 * it's estimated confidence in ms
 ****************************************************/
void ntpSynced(unsigned int confidence) {
  Serial.print("NTP synced with conf: ");
  Serial.println(confidence);
}

```



This class can also be used with the an RTC.
The current time will then be gathered from the rtc object. After each successfull NTP request, the RTC time is updated. 

```C++
#include "timeHandling.h"
#include "rtc.h"
...
Rtc rtc(RTC_INT, SDA_PIN, SCL_PIN);
TimeHandler myTime(config.timeServer, LOCATION_TIME_OFFSET, &rtc, &ntpSynced);
...

```

You can also use it to get log time strings for a [multiLogger](https://github.com/voelkerb/ESP.multiLogger/) instance. See advanced example:
 ```advanced.ino```.

```bash
 - [I]02/06 08:28:16: Connecting to WiFi..
 - [I]02/06 08:28:16: Connected to the WiFi network: ******** with IP: *********
 - [D]02/06 08:28:16: Sending NTP packet...
 - [I]02/25 12:34:04: NTP Time: 02/25/2021 12:34:04.597, td 17
 - [D]02/25 12:34:04: Success NTP Time
 - [D]02/25 12:34:04: NTP synced with conf: 17
 - [I]02/25 12:34:04: Current time: 02/25/2021 12:34:04.728
 - [I]02/25 12:34:14: Another 10s passed
 - [I]02/25 12:34:14: Current time: 02/25/2021 12:34:14.000
 - [I]02/25 12:34:24: Another 10s passed
 - [I]02/25 12:34:24: Current time: 02/25/2021 12:34:24.000
```
