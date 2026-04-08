// --------------------------------------------------------------------------------
// GESTION DU TEMPS ET SYNCHRONISATION
// --------------------------------------------------------------------------------
#include <Arduino.h>     // Toujours en premier pour le type String et uint32_t
#include "includes.h"    // Ton fichier de regroupement qui contient time_utils.h
#include <WiFi.h>
#include <TinyGPSPlus.h>

extern TinyGPSPlus gps;
extern int gpsLocalHour;

void initTime() {
    configTzTime(TZ_EUROPE_PARIS, "pool.ntp.org", "time.google.com");
}

String getFormattedTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0) || timeinfo.tm_year < 124) return "NTP ?";
    char buf[25];
    // %d = jour, %m = mois, %Y = année (4 chiffres), %H:%M:%S = heure
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
    return String(buf);
}

// Retourne l'heure courante en décimal (ex: 8h30 → 8.5). -1 si source indisponible.
float getDecimalHour() {
    // Priorité NTP
    if (WiFi.status() == WL_CONNECTED) {
        struct tm t;
        if (getLocalTime(&t, 0) && t.tm_year >= 124)
            return t.tm_hour + t.tm_min / 60.0f + t.tm_sec / 3600.0f;
    }
    // Fallback GPS (données fraîches < 5s)
    if (gps.time.isValid() && gps.time.age() < 5000)
        return gpsLocalHour + gps.time.minute() / 60.0f + gps.time.second() / 3600.0f;
    return -1.0f;
}
