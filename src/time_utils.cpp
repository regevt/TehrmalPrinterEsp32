#include "time_utils.h"

#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000);

time_t getCurrentEpochTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}

String getShortLocationTime() {
  timeClient.update();
  String time = timeClient.getFormattedTime();
  return time.substring(0, 5);
}

String getCurrentDateString() {
  time_t rawTime = getCurrentEpochTime();
  struct tm *timeInfo = localtime(&rawTime);
  
  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", timeInfo);
  return String(dateStr);
}

String getDayOfWeek() {
  time_t rawTime = getCurrentEpochTime();
  struct tm *timeInfo = localtime(&rawTime);
  
  switch(timeInfo->tm_wday) {
    case 0: return "Sunday";
    case 1: return "Monday";
    case 2: return "Tuesday";
    case 3: return "Wednesday";
    case 4: return "Thursday";
    case 5: return "Friday";
    case 6: return "Saturday";
    default: return "Unknown";
  }
}

String getMonthName() {
  time_t rawTime = getCurrentEpochTime();
  struct tm *timeInfo = localtime(&rawTime);
  
  switch(timeInfo->tm_mon) {
    case 0: return "January";
    case 1: return "February";
    case 2: return "March";
    case 3: return "April";
    case 4: return "May";
    case 5: return "June";
    case 6: return "July";
    case 7: return "August";
    case 8: return "September";
    case 9: return "October";
    case 10: return "November";
    case 11: return "December";
    default: return "unknown";
  }
}

String getCurrentDate() {
  time_t rawTime = getCurrentEpochTime();
  struct tm *timeInfo = localtime(&rawTime);
  return String(timeInfo->tm_mday);
}

String getLocationTime() {
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  time_t rawTime = timeClient.getEpochTime();
  struct tm *timeInfo = localtime(&rawTime);
  
  char dateTimeStr[20];
  snprintf(dateTimeStr, sizeof(dateTimeStr), "%02d.%02d.%04d %s", 
           timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_year + 1900, 
           formattedTime.c_str());
  
  return String(dateTimeStr);
}

void initTimeService() {
  timeClient.begin();
  timeClient.setTimeOffset(10800);
  
  Serial.print("⏰ Getting time from NTP server");
  for (int i = 0; i < 10; i++) {
    if (timeClient.update()) {
      Serial.println("\n✅ Time has arrived!");
      break;
    }
    Serial.print(".");
    delay(1000);
  }
}

void tickTimeService() {
  timeClient.update();
}
