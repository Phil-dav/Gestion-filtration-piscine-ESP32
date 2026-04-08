#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <Arduino.h>
#include <TinyGPS++.h>

void initGPS();
// void readGPSTrame();
void updateGPS();     // Remplace readGPSTrame
String getGPSPitch(); // Pour tester si on a un signal
extern int gpsLocalHour;
#endif