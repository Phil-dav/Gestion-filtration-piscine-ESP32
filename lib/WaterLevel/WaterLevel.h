#ifndef WATER_LEVEL_H
#define WATER_LEVEL_H

#include <Arduino.h>
#include <config.h>


// --- HYSTÉRÉSIS ANALOGIQUE ---
#define THRESHOLD_LOW  1800  // En dessous de ça = Danger certain
#define THRESHOLD_HIGH 2200  // Au dessus de ça = OK certain

// --- TEMPORISATION (ms) ---
#define DELAY_SAFETY_STOP 3000 // 3s de confirmation avant coupure
#define DELAY_SAFETY_OK   5000 // 5s de confirmation avant réarmement

bool isWaterLevelOk();

#endif