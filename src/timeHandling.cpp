/***************************************************
 Library for time keeping.
 
 License: Creative Common V1. 

 Benjamin Voelker, voelkerb@me.com
 Embedded Systems Engineer
 ****************************************************/

#include "timeHandling.h"


// _________________________________________________________________________
TimeHandler::TimeHandler(char * ntpServerName, int locationOffset, void (*ntpSyncCB)(unsigned int), DST dst, MultiLogger * logger) {
#if defined(ESP32)
  _ntpTaskHandle = NULL;
#endif
  _ntpServerName = ntpServerName;
  _locationOffset = locationOffset;
  _localNTPPort = 2390;
  _ntpValidMillis = 0;
  _ntpConfidence = 10000;
  _currentTime.seconds = 0;
  _currentTime.milliSeconds = 0;
  _ntpTime.seconds = 0;
  _ntpTime.milliSeconds = 0;
  _lastNTPTry = millis();
  _ntpSyncCB = ntpSyncCB;
  _dst = dst;
  this->logger = logger;
}


// _________________________________________________________________________
unsigned long TimeHandler::utc_seconds() {
  update();
  // return _currentSeconds;
  return _currentTime.seconds;
}

// _________________________________________________________________________
unsigned long TimeHandler::milliseconds() {
  update();
  // return _currentMilliseconds;
  return _currentTime.milliSeconds;
}

// _________________________________________________________________________
Timestamp TimeHandler::timestamp() {
  update();
  // _currentTime.seconds = _currentSeconds;
  // _currentTime.milliSeconds = _currentMilliseconds;
  return _currentTime;
}

// _________________________________________________________________________
uint8_t TimeHandler::year() {
  update();
  return timeZoneDST(_currentTime.seconds).year();
}
// _________________________________________________________________________
uint8_t TimeHandler::month() {
  update();
  return timeZoneDST(_currentTime.seconds).month();
}
// _________________________________________________________________________
uint8_t TimeHandler::date() {
  update();
  return timeZoneDST(_currentTime.seconds).day();
}
// _________________________________________________________________________
uint8_t TimeHandler::hour() {
  update();
  return timeZoneDST(_currentTime.seconds).hour();
}
// _________________________________________________________________________
uint8_t TimeHandler::minute() {
  update();
  return _currentTime.seconds/60%60;
}
// _________________________________________________________________________
uint8_t TimeHandler::second() {
  update();
  return _currentTime.seconds%60;
}

// _________________________________________________________________________
#if defined(ESP32)
bool TimeHandler::updateNTPTime(bool wait) {
  // NOTE: can anything bad happen if we do this multiple times?
  _udpNtp.begin(_localNTPPort);

  if (wait) {
    return _updateNTPTime();
  } else {
    if (_ntpTaskHandle == NULL) {
      // let it check on second core (not in loop)
      TimeHandler *obj = this;
      xTaskCreatePinnedToCore(
                _startUpdateNTPTime,
                // _updateNTPTime,   /* Function to implement the task */
                "_updateNTPTime", /* Name of the task */
                10000,      /* Stack size in words */
                (void*) obj,       /* Task input parameter */
                2,          /* Priority of the task */
                &_ntpTaskHandle,       /* Task handle. */
                0);  /* Core where the task should run */
    }
  }
  return true;
}
#else
bool TimeHandler::updateNTPTime() {
  // NOTE: can anything bad happen if we do this multiple times?
  _udpNtp.begin(_localNTPPort);
  return _updateNTPTime();
}
#endif

// _________________________________________________________________________
void TimeHandler::update() {
  int32_t delta = millis()-_ntpValidMillis;
  if (delta > NTP_UPDATE_INTV && millis()-_lastNTPTry > NTP_TRY_INTV) {
    _lastNTPTry = millis();
    if (WiFi.status() == WL_CONNECTED and WiFi.isConnected()){
      // if (Network::connected and not Network::apMode) {
#if defined(ESP32)
      updateNTPTime(false);
#else
      updateNTPTime();
#endif
    }
  }
  if (_ntpTime.seconds != 0) {
    _currentTime.milliSeconds = _ntpTime.milliSeconds + delta%1000;
    _currentTime.seconds = _ntpTime.seconds + delta/1000 + _currentTime.milliSeconds/1000;
    _currentTime.milliSeconds = _currentTime.milliSeconds%1000;
    
  }
}

#if defined(ESP32)
// _________________________________________________________________________
// Decorator function since we cannot use non static member function in freertos' createTask 
void TimeHandler::_startUpdateNTPTime(void * pvParameters) {
  ((TimeHandler*)pvParameters)->_updateNTPTime();
  vTaskDelete(NULL);
}
#endif

// _________________________________________________________________________
bool TimeHandler::_updateNTPTime() {
  bool success = _getTimeNTP();
  int32_t delta = millis()-_ntpValidMillis;

  _currentTime.milliSeconds = _ntpTime.milliSeconds + delta%1000;
  _currentTime.seconds = _ntpTime.seconds + delta/1000 + _currentTime.milliSeconds/1000;
  _currentTime.milliSeconds = _currentTime.milliSeconds%1000;
  DateTime ntpTime(_currentTime.seconds + _locationOffset);

  // Make sanity check here.
  if (ntpTime.year() < 2018 || ntpTime.year() > 2100 || ntpTime.month() > 12 || ntpTime.day() > 31) {
    success = false;
  }
  // Only on success do sth with the time
  if (success) {
    if (logger != NULL) logger->log(DEBUG, "Success NTP Time");
    // Calculate new boundaries for DST
    // TODO: Is this correct?
    int startDay = _dst.startAfterDay + (7 + _dst.startdayOfWeek - _dow(ntpTime.year(), _dst.startMon, _dst.startAfterDay))%7;
    int stopDay = _dst.startAfterDay + (7 + _dst.stopdayOfWeek - _dow(ntpTime.year(), _dst.stopMon, _dst.stopAfterDay))%7;
    _tsD1 = DateTime(ntpTime.year(), _dst.startMon, startDay, 1, 0, 0).unixtime();
    _tsD2 = DateTime(ntpTime.year(), _dst.stopMon, stopDay, 1, 0, 0).unixtime();

    // Call synced callback if there is any
    if (_ntpSyncCB != NULL) _ntpSyncCB(_ntpConfidence);
  }

  return success;
}

// _________________________________________________________________________
bool TimeHandler::_getTimeNTP() {
  WiFi.hostByName(_ntpServerName, _timeServerIP);
  if (logger != NULL) logger->log(DEBUG, "Sending NTP packet... %s, ip: %s", _ntpServerName, _timeServerIP.toString().c_str());
  // Reset all bytes in the buffer to 0
  memset(_ntpBuffer, 0, PACKET_SIZE_NTP);
  // Initialize values needed to form NTP requestjvbasd
  _ntpBuffer[0] = 0b11100011; // LI, Version, Mode
  _ntpBuffer[1] = 0; // Stratum, or type of clock
  _ntpBuffer[2] = 6; // Polling Interval
  _ntpBuffer[3] = 0xEC; // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  _ntpBuffer[12]  = 49;
  _ntpBuffer[13]  = 0x4E;
  _ntpBuffer[14]  = 49;
  _ntpBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:

  unsigned long start = millis();
  _udpNtp.beginPacket(_timeServerIP, 123); //NTP requests are to port 123
  _udpNtp.write(_ntpBuffer, PACKET_SIZE_NTP);
  _udpNtp.endPacket();// should flush
  // Wait for packet to arrive with timeout of 2 second
  int cb = _udpNtp.parsePacket();
  while(!cb) {
    if (start + 200 < millis()) break;
    // if (start + 100 < millis()) break;
    cb = _udpNtp.parsePacket();
    delay(1);
    #if defined(ESP32)
    yield();
    #endif
  }
  if (!cb) {
    if (logger != NULL) logger->log(WARNING, "No NTP response yet");
    return false;
  }

  _ntpValidMillis = millis();
  uint16_t tripDelayMs = (_ntpValidMillis-start)/2;
  _ntpValidMillis -= tripDelayMs;
  //Serial.print("Info:Received NTP packet, length=");
  //Serial.println(cb);
  // We've received a packet, read the data from it
  _udpNtp.read(_ntpBuffer, PACKET_SIZE_NTP);
  // read the packet into the buf
  unsigned long highWord = word(_ntpBuffer[40], _ntpBuffer[41]);
  unsigned long lowWord = word(_ntpBuffer[42], _ntpBuffer[43]);
  // combine the four bytes (two words) into a long integer
  // this is NTP time (seconds since Jan 1 1900):
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  uint32_t frac  = (uint32_t) _ntpBuffer[44] << 24
                 | (uint32_t) _ntpBuffer[45] << 16
                 | (uint32_t) _ntpBuffer[46] <<  8
                 | (uint32_t) _ntpBuffer[47] <<  0;
  uint16_t mssec = ((uint64_t) frac *1000) >> 32;
  // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
  // subtract seventy years:
  unsigned long epoch = secsSince1900 - 2208988800UL;
  // Add the time we waited
  _ntpTime.seconds = epoch;
  _ntpTime.milliSeconds = mssec;
  _ntpConfidence = tripDelayMs;
  if (logger != NULL) logger->log(INFO, "NTP Time: %s, td %u",  timeStr(_ntpTime.seconds, _ntpTime.milliSeconds), tripDelayMs);
  return true;
}

bool TimeHandler::valid() {
  if (_currentTime.seconds == 0) return false;

  DateTime ntpTime(_currentTime.seconds + _locationOffset);
  // Make sanity check here.
  if (ntpTime.year() < 2018 || ntpTime.year() > 2100 || ntpTime.month() > 12 || ntpTime.day() > 31) {
    return false;
  }

  return true;
}

// _________________________________________________________________________
char * TimeHandler::timeStr(bool shortForm) {
  update();
  return timeStr(_currentTime.seconds, _currentTime.milliSeconds, shortForm);
}

// _________________________________________________________________________
char * TimeHandler::timeStr(Timestamp ts, bool shortForm) {
  return timeStr(ts.seconds, ts.milliSeconds, shortForm);
}

// _________________________________________________________________________
char * TimeHandler::timeStr(DateTime dt, unsigned long ms, bool shortForm) {
  if (shortForm) {
    snprintf(_timeString, _MAX_TIME_STR_LENGTH, "%02u/%02u %02u:%02u:%02u", dt.month(), dt.day(),
            dt.hour(), dt.minute(), dt.second());
  } else {
    snprintf(_timeString, _MAX_TIME_STR_LENGTH, "%02u/%02u/%04u %02u:%02u:%02u.%03u", dt.month(), dt.day(), dt.year(),
            dt.hour(), dt.minute(), dt.second(), (unsigned int)ms);
  }
  return _timeString;
}


// _________________________________________________________________________
int TimeHandler::_dow(int y, int m, int d) { 
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  // Return day of week here with 0=Sun - 6=Sat
  return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7; 
}


Weekday TimeHandler::dayOfTheWeek(DateTime dt) {
  return (Weekday)(_dow(dt.year(), dt.month(), dt.day()));
}

Weekday TimeHandler::dayOfTheWeek() {
  update();
  return dayOfTheWeek(DateTime(_currentTime.seconds + _locationOffset));
}

Weekday TimeHandler::dayOfTheWeek(Timestamp ts) {
  return dayOfTheWeek(DateTime(ts.seconds + _locationOffset));
}

char * TimeHandler::dayOfTheWeekStr(DateTime dt, bool shortForm) {
  int dow = _dow(dt.year(), dt.month(), dt.day())%7;
  if (shortForm) snprintf(_dayOfWeekStr, _MAX_DOW_STR_LENGTH, "%s", WEEKDAYS_SHORT[dow]);
  else snprintf(_dayOfWeekStr, _MAX_DOW_STR_LENGTH, "%s", WEEKDAYS[dow]);
  return &_dayOfWeekStr[0];
}

char * TimeHandler::dayOfTheWeekStr(bool shortForm) {
  update();
  return dayOfTheWeekStr(DateTime(_currentTime.seconds+_locationOffset), shortForm);
}

char * TimeHandler::dayOfTheWeekStr(Timestamp ts, bool shortForm) {
  return dayOfTheWeekStr(DateTime(ts.seconds+_locationOffset), shortForm);
}


// _________________________________________________________________________
DateTime TimeHandler::timeZoneDST(unsigned long s) {
  // Add location offset to utc time
  DateTime ttime(s + _locationOffset);
  // If we want to be sensititve to DST (DST boarders recalculated on ntp request)
  if (_dst.active) {
    // int startDay = dst.startAfterDay + (7 + startdayOfWeek - _dow(ttime.year(), _dst.startMon, dst.startAfterDay))%7;
    // int stopDay = dst.startAfterDay + (7 + stopdayOfWeek - _dow(ttime.year(), _dst.stopMon, dst.stopAfterDay))%7;
    // // int startDay = 31 - _dow(ttime.year(), _dst.startMon, 31);
    // // int stopDay = 31 - _dow(ttime.year(), _dst.stopMon, 31);
    // DateTime d1(ttime.year(), _dst.startMon, startDay, 1, 0, 0);
    // DateTime d2(ttime.year(), _dst.stopMon, stopDay, 1, 0, 0);
    // uint32_t tsD1 = d1.unixtime();
    // uint32_t tsD2 = d2.unixtime();
    if (s >= _tsD1 && s <= _tsD2) {
      return DateTime(s + _locationOffset + _dst.seconds);
    }
  }
  return ttime;
}


// _________________________________________________________________________
char * TimeHandler::timeStr(unsigned long s, unsigned long ms, bool shortForm) {
  // If time not valid yet, look if rtc is connected and use its time
  return timeStr(timeZoneDST(s), ms, shortForm);
}


// _________________________________________________________________________
char * TimeHandler::timestampStr(Timestamp ts, bool shortForm) {
  return timestampStr(ts.seconds, ts.milliSeconds, shortForm);
}


// _________________________________________________________________________
char * TimeHandler::timestampStr(bool shortForm) {
  update();
  return timestampStr(_currentTime.seconds, _currentTime.milliSeconds, shortForm);
}

// _________________________________________________________________________
char * TimeHandler::timestampStr(unsigned long s, unsigned long ms, bool shortForm) {
  if (shortForm) {
    snprintf(&_timeStampStr[0], _MAX_TIME_STR_LENGTH, "%u", (uint32_t)s);
  } else {
    snprintf(&_timeStampStr[0], _MAX_TIME_STR_LENGTH, "%u.%03u", (uint32_t)s, (uint32_t)ms);
  }
  return &_timeStampStr[0];
}