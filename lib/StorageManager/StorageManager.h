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

#endif