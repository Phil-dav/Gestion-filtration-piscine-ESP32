#include "WaterLevel.h"

// Variables statiques pour garder l'état entre deux appels de loop()
static bool filteredState = true; 
static uint32_t lastChangeTime = 0;
static bool targetState = true;

bool isWaterLevelOk() {
    int val = analogRead(PIN_LEVEL_SENSOR); //
    uint32_t now = millis();

    // 1. DÉTERMINATION DE L'ÉTAT BRUT (Hystérésis)
    // Si on est dans la zone "morte" (entre 1800 et 2200), on ne change rien à l'état brut
    bool rawState = targetState; 
    if (val < THRESHOLD_LOW) {
        rawState = false; // Trop bas
    } else if (val > THRESHOLD_HIGH) {
        rawState = true;  // OK
    }

    // 2. FILTRAGE TEMPOREL (Anti-vagues)
    if (rawState != targetState) {
        // Le niveau vient de franchir un seuil, on lance le chrono
        targetState = rawState;
        lastChangeTime = now;
    }

    // On vérifie si le nouvel état est stable depuis assez longtemps
    uint32_t requiredDelay = (targetState == false) ? DELAY_SAFETY_STOP : DELAY_SAFETY_OK;

    if ((now - lastChangeTime) >= requiredDelay) {
        // L'état est stable, on valide le changement
        filteredState = targetState;
    }

    return filteredState;
}