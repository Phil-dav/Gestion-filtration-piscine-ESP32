#ifndef DS18_MANAGER_H
#define DS18_MANAGER_H

#include <OneWire.h>
#include <DallasTemperature.h>

/**
 * @brief Initialise le bus OneWire et la sonde DS18B20.
 *        Active le mode non-bloquant (setWaitForConversion false).
 */
void initDS18B20();

/**
 * @brief Lance la demande de conversion de température SANS attendre.
 *        À appeler dans loop(), puis attendre 800ms avant updateDS18Cache().
 */
void requestDS18Temperatures();

/**
 * @brief Lit la température depuis la sonde et met à jour le cache.
 *        À appeler 800ms APRÈS requestDS18Temperatures().
 */
void updateDS18Cache();

/**
 * @brief Récupère la température de l'eau depuis le cache.
 * @return float Température en °C.
 */
float getWaterTemp();

/**
 * @brief Indique si la sonde a fourni au moins une lecture valide (> -20°C).
 *        Faux au démarrage jusqu'à la première mesure correcte.
 */
bool isTempValid();

#endif
