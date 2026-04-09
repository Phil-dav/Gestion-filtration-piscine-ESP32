#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>

// --- INITIALISATION ---
void initStorage();

// --- GESTION DU COMPTEUR DE FILTRATION (NVS / Preferences) ---
void saveFiltrationProgress(float hours);
float loadFiltrationProgress();
void saveLastDay(int day);   // Sauvegarde le dernier jour connu (anti-faux reset)
int  loadLastDay();          // Restitue le dernier jour (0 = inconnu)

// --- GESTION DES JOURNAUX (LittleFS / Fichiers .txt) ---
void logHistory(String message); // Pour historique.txt (Cycles de pompe) — non utilisée actuellement, réservée
void logToFile(String message);  // Pour systeme.log (Reboots, erreurs soft)
void logAlert(String message);   // Pour alertes.log (Gel, manque d'eau)

// --- MAINTENANCE DES JOURNAUX (à appeler à minuit) ---
// Tronque /systeme.log aux N dernières lignes pour éviter la saturation LittleFS.
// A appeler chaque jour à minuit lors du reset journalier.
void trimSystemLog(int maxLines = 200);

// Supprime les fichiers CSV /logs/*.csv plus vieux que keepMonths mois.
// A appeler chaque jour à minuit (ou au démarrage) lors du reset journalier.
void purgeOldLogs(int keepMonths = 2);

#endif