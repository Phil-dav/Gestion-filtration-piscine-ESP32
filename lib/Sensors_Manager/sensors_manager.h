#ifndef SENSORS_MANAGER_H
#define SENSORS_MANAGER_H

#include <Adafruit_AHTX0.h>

/**
 * @brief Initialise le capteur AHT10 sur le bus I2C.
 * @return true si le capteur est détecté, false sinon.
 */
bool initAHT10();

/**
 * @brief Lit physiquement le capteur et met à jour les variables en mémoire (cache).
 * IMPORTANT : À appeler uniquement depuis la boucle principale loop() de main.cpp,
 * idéalement toutes les 5 à 10 secondes pour ne pas saturer le bus I2C.
 */
void updateSensorCache();

/**
 * @brief Retourne la dernière température stockée en mémoire.
 * @return float Valeur de la température en °C.
 */
float getInternalTemp();

/**
 * @brief Retourne la dernière humidité stockée en mémoire.
 * @return float Valeur de l'humidité relative en %.
 */
float getInternalHumidity();

/**
 * @brief Vérifie si le capteur est opérationnel.
 * @return true si l'initialisation a réussi, false sinon.
 */
bool isSensorOK();

#endif // SENSORS_MANAGER_H