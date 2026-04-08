#include "includes.h"

// ── États mémorisés entre les appels (hystérésis) ─────────────────────────────
static bool modeHiver    = true;   // Actif par défaut au démarrage (sécurité)
static bool modeCanicule = false;
static bool modeAntiGel  = false;

// ── Plage horaire configurable (mode standard uniquement) ──────────────────────
static float configStart = 8.0f;   // Défaut : 8h
static float configEnd   = 20.0f;  // Défaut : 20h

// ── Seuils d'hystérésis ───────────────────────────────────────────────────────
// Hiver       : entrée < 9.5°C  / sortie > 10.5°C
// Canicule    : entrée > 28.5°C / sortie < 27.5°C
// Anti-gel    : entrée < 4.0°C  / sortie > 5.0°C

float calculateTargetHours(float temp)
{
    // ── 1. Anti-gel (priorité absolue) ────────────────────────────────────────
    if (!modeAntiGel && temp < 4.0f)  modeAntiGel = true;
    if ( modeAntiGel && temp > 5.0f)  modeAntiGel = false;

    if (modeAntiGel) {
        // Retourne 24h pour forcer filtration continue dans main.cpp
        return 24.0f;
    }

    // ── 2. Canicule ───────────────────────────────────────────────────────────
    if (!modeCanicule && temp > 28.5f) modeCanicule = true;
    if ( modeCanicule && temp < 27.5f) modeCanicule = false;

    if (modeCanicule) {
        return 24.0f; // Filtration continue
    }

    // ── 3. Hiver ──────────────────────────────────────────────────────────────
    if ( modeHiver && temp > 10.5f) modeHiver = false;
    if (!modeHiver && temp <  9.5f) modeHiver = true;

    if (modeHiver) {
        return 2.0f; // Filtration fixe hivernage
    }

    // ── 4. Eau chaude (24 – 28.5°C) : T/2 + 1h, arrondi à 0.5h ─────────────
    float raw;
    if (temp >= 24.0f) {
        raw = (temp / 2.0f) + 1.0f;
    } else {
        // ── 5. Standard (10.5 – 24°C) : règle T/2, arrondi à 0.5h ───────────
        raw = temp / 2.0f;
    }

    // Arrondi à la demi-heure la plus proche (ex: 11.3h → 11.5h, 11.1h → 11.0h)
    return roundf(raw * 2.0f) / 2.0f;
}

bool isAntiGelActif()  { return modeAntiGel;  }
bool isCaniculeActif() { return modeCanicule; }

float getFilterStartHour()
{
    if (modeAntiGel || modeCanicule) return 0.0f;       // Continu
    if (modeHiver)                   return 10.0f;      // 10h en hiver
    return configStart;                                 // Standard : configurable
}

float getFilterEndHour()
{
    if (modeAntiGel || modeCanicule) return 24.0f;      // Continu
    if (modeHiver)                   return 16.0f;      // 16h en hiver
    return configEnd;                                   // Standard : configurable
}

float getConfiguredStartHour() { return configStart; }
float getConfiguredEndHour()   { return configEnd;   }

void loadFilterSchedule()
{
    Preferences prefs;
    prefs.begin("schedule", true); // lecture seule
    configStart = prefs.getFloat("filtStart", 8.0f);
    configEnd   = prefs.getFloat("filtEnd",  20.0f);
    prefs.end();
    logSystem(INFO, "SCHED", "Plage chargee : " + String(configStart, 1) + "h-" + String(configEnd, 1) + "h");
}

bool setFilterSchedule(float start, float end)
{
    if (start < 0.0f || start > 23.5f) return false;
    if (end   < 0.5f || end   > 24.0f) return false;
    if (start >= end)                   return false;

    configStart = start;
    configEnd   = end;

    Preferences prefs;
    prefs.begin("schedule", false);
    prefs.putFloat("filtStart", start);
    prefs.putFloat("filtEnd",   end);
    prefs.end();

    logSystem(INFO, "SCHED", "Plage modifiee : " + String(start, 1) + "h-" + String(end, 1) + "h");
    return true;
}
