#ifndef PUMP_MANAGER_H
#define PUMP_MANAGER_H

#include <Arduino.h>

// Initialisation (si besoin de régler des variables au départ)
void initPumpManager();

// La fonction à appeler dans la loop pour gérer la pompe
void updatePumpSystem();

// L'interrupteur logiciel : pour demander à la pompe de s'allumer ou s'éteindre
void setPumpRequest(bool state);

// Coupure directe du relais via le driver PCF, sans passer par updatePumpSystem().
// Utilisé au démarrage et en cas d'alarme sécurité.
void forceStopPump();

// Pour savoir si la pompe tourne réellement (utile pour l'OLED ou le Web)
bool isPumpRunning();
bool getPumpRequest();       // Retourne l'état de la demande de filtration
float getPumpingDoneToday(); // Retourne les heures de filtration cumulées aujourd'hui
int   getDailySessionCount();  // Nombre de sessions pompe depuis minuit
void  resetDailySessionCount(); // Reset à minuit

// --- FEEDBACK HARDWARE ---
// true si le fil GPIO33 est rompu (mismatch > 30s) — mode dégradé, pompe non bloquée
bool isFeedbackFault();
// true si la pompe est temporairement bloquée suite à un claquement détecté
bool isPumpBlocked();
#endif