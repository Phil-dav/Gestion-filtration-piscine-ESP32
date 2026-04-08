#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>

// Initialisation (crée le dossier /logs si absent)
void initLogManager();

// Retourne l'heure courante "HH:MM" — utilisable par les autres managers
String logGetTimeStr();

// ── Enregistrements ──────────────────────────────────────────────────────────

// Session pompe : appelé quand la pompe s'arrête
void logSession(const String& debut, const String& fin,
                int dureeMin, float tEau,
                const char* mode, const char* causeFin, bool pompeOn);

// Bilan journalier : appelé à minuit avant le reset du compteur
void logDaily(float objectifH, float faitH, int nbSessions, const char* modeJour);

// Alerte / défaut : appelé à chaque nouveau défaut détecté
void logAlerte(const char* type, const char* valeur);

// ── Compteur alertes du jour (reset à minuit) ─────────────────────────────────
void resetDailyAlertCount();
int  getDailyAlertCount();

// ── Chemins fichiers du mois courant ─────────────────────────────────────────
String getSessionLogPath();
String getDailyLogPath();
String getAlerteLogPath();

#endif
