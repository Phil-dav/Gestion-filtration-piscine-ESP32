#ifndef DEBUG_MANAGER_H
#define DEBUG_MANAGER_H

#include <Arduino.h>
#include "config.h"

// Définition des niveaux de log pour la cohérence du projet
enum LogLevel {
    INFO,
    WARNING,
    CRITICAL
};

/**
 * @brief Initialise le gestionnaire (vide pour l'instant)
 */
void initDebugManager();

/**
 * @brief État du debug (toujours actif pour tes tests)
 */
bool isDebugEnabled();

/**
 * @brief Envoie un message sur le port série
 */
void logSystem(LogLevel level, String module, String message);

#endif