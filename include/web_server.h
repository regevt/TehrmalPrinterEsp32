#pragma once

#include <Arduino.h>

extern unsigned long lastWifiCheck;
extern const unsigned long WIFI_CHECK_INTERVAL;
extern bool wifiConnected;
extern int wifiReconnectAttempts;
extern const int MAX_RECONNECT_ATTEMPTS;
void initFileSystem();
void initWebServer();
