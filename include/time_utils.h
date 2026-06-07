#pragma once

#include <Arduino.h>

void initTimeService();
void tickTimeService();

time_t getCurrentEpochTime();
String getShortLocationTime();
String getCurrentDateString();
String getDayOfWeek();
String getMonthName();
String getCurrentDate();
String getLocationTime();
