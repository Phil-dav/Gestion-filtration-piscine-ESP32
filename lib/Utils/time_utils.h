#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>

// Déclarations pour time_utils.cpp
void  initTime();
String getFormattedTime();
float getDecimalHour(); // Heure courante en décimal (ex: 8h30 → 8.5), -1 si indisponible

#endif