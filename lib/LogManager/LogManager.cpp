#include "LogManager.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <time.h>
#include <TinyGPSPlus.h>

extern TinyGPSPlus gps;
extern int gpsLocalHour; // Heure locale corrigée (UTC + offset été/hiver) — défini dans GPS_Manager.cpp

static int _dailyAlertCount = 0;

// ── Helpers temps ─────────────────────────────────────────────────────────────

static bool _getLocalTm(struct tm& t) {
    // Priorité NTP (WiFi) — t.tm_year >= 124 rejette l'epoch (01/01/1970) avant sync NTP
    if (WiFi.status() == WL_CONNECTED && getLocalTime(&t, 100) && t.tm_year >= 124) return true;
    // Fallback GPS — données fraîches uniquement (age < 5s)
    if (gps.date.isValid() && gps.time.isValid() && gps.date.age() < 5000 && gps.time.age() < 5000) {
        memset(&t, 0, sizeof(t));
        t.tm_year = gps.date.year() - 1900;
        t.tm_mon  = gps.date.month() - 1;
        t.tm_mday = gps.date.day();
        t.tm_hour = gpsLocalHour;        // heure locale corrigée (pas UTC brut)
        t.tm_min  = gps.time.minute();
        t.tm_sec  = gps.time.second();
        return true;
    }
    return false;
}

static String _dateStr() {
    struct tm t;
    if (!_getLocalTm(t)) return "00/00/0000";
    char buf[11];
    strftime(buf, sizeof(buf), "%d/%m/%Y", &t);
    return String(buf);
}

String logGetTimeStr() {
    struct tm t;
    if (!_getLocalTm(t)) return "00:00:00";
    char buf[9];
    strftime(buf, sizeof(buf), "%H:%M:%S", &t);
    return String(buf);
}

static String _monthTag() {
    struct tm t;
    if (!_getLocalTm(t)) return "0000";
    char buf[5];
    strftime(buf, sizeof(buf), "%m%y", &t);
    return String(buf);
}

// ── Chemins fichiers ──────────────────────────────────────────────────────────

String getSessionLogPath() { return "/logs/sessions_" + _monthTag() + ".csv"; }
String getDailyLogPath()   { return "/logs/daily_"    + _monthTag() + ".csv"; }
String getAlerteLogPath()  { return "/logs/alertes_"  + _monthTag() + ".csv"; }

// ── Initialisation ────────────────────────────────────────────────────────────

void initLogManager() {
    if (!LittleFS.exists("/logs")) {
        LittleFS.mkdir("/logs");
    }
}

// ── Écriture CSV (crée l'en-tête si nouveau fichier) ─────────────────────────

static void _writeCSV(const String& path, const String& header, const String& line) {
    bool needsHeader = !LittleFS.exists(path);
    File f = LittleFS.open(path, FILE_APPEND);
    if (!f) return;
    if (needsHeader) f.println(header);
    f.println(line);
    f.close();
}

// ── Sessions pompe ────────────────────────────────────────────────────────────

void logSession(const String& debut, const String& fin,
                int dureeMin, float tEau,
                const char* mode, const char* causeFin, bool pompeOn) {
    const String header = "date,debut,fin,duree_min,T_eau,mode,pompe,cause_fin";
    String line = _dateStr()
                + "," + debut
                + "," + fin
                + "," + String(dureeMin)
                + "," + String(tEau, 1)
                + "," + String(mode)
                + "," + String(pompeOn ? "ON" : "OFF")
                + "," + String(causeFin);
    _writeCSV(getSessionLogPath(), header, line);
}

// ── Bilan journalier ──────────────────────────────────────────────────────────

void logDaily(float objectifH, float faitH, int nbSessions, const char* modeJour) {
    const String header = "date,objectif_h,fait_h,nb_sessions,nb_alertes,mode_jour";
    String line = _dateStr()
                + "," + String(objectifH, 1)
                + "," + String(faitH, 2)
                + "," + String(nbSessions)
                + "," + String(_dailyAlertCount)
                + "," + String(modeJour);
    _writeCSV(getDailyLogPath(), header, line);
}

// ── Alertes ───────────────────────────────────────────────────────────────────

void logAlerte(const char* type, const char* valeur) {
    _dailyAlertCount++;
    const String header = "date,heure,type,valeur";
    String line = _dateStr()
                + "," + logGetTimeStr()
                + "," + String(type)
                + "," + String(valeur);
    _writeCSV(getAlerteLogPath(), header, line);
}

void resetDailyAlertCount() { _dailyAlertCount = 0; }
int  getDailyAlertCount()   { return _dailyAlertCount; }
