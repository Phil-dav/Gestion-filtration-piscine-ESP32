#ifndef SAFETY_MANAGER_H
#define SAFETY_MANAGER_H

#include <Arduino.h>

/**
 * @brief Vérifie toutes les conditions de sécurité (Niveau, Moteur, etc.)
 * @return true si tout est OK, false si un danger est détecté
 */
bool isSystemSafe();

/**
 * @brief Retourne la cause exacte du dernier défaut (pour affichage OLED)
 */
String getSafetyStatusMessage();

/**
 * @brief Retourne true si le contact disjoncteur moteur indique un défaut (contact NF ouvert)
 *        ou si le verrou est actif en attente de réarmement
 */
bool isMotorFaultActive();

/**
 * @brief Retourne true si le verrou défaut moteur est actif (réarmement requis)
 */
bool isMotorFaultLatched();

/**
 * @brief Réarme le défaut moteur — à appeler après vérification par l'opérateur
 */
void resetMotorFault();

#endif