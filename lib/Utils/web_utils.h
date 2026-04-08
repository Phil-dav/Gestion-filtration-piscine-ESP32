#ifndef WEB_UTILS_H
#define WEB_UTILS_H

#include <Arduino.h>

// Déclarations pour web_utils.cpp
void startWebServer();
bool isBoostActive();
bool isBoostForceOn();
int  getBoostDurationMinutes();
bool setBoostDurationMinutes(int minutes);

#endif