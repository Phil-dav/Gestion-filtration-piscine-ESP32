#include "config.h"
#include "ds18_manager.h"
#include "DebugManager.h"

static OneWire oneWire(DS18B20_PIN);
static DallasTemperature sensors(&oneWire);
static float _waterTempCache = -127.0f; // Valeur d'erreur par défaut du DS18B20
static bool  _tempValid      = false;   // Faux jusqu'à la première lecture physiquement plausible

// ----------------------------------------------------------------------------
// INITIALISATION
// ----------------------------------------------------------------------------
void initDS18B20()
{
    sensors.begin();

    // Mode non-bloquant : requestTemperatures() rend la main immédiatement
    // sans attendre la fin de la conversion (~750ms).
    // C'est le main.cpp qui gère le délai d'attente de 800ms.
    sensors.setWaitForConversion(false);

    logSystem(INFO, "DS18", "Initialisation du bus OneWire...");
}

// ----------------------------------------------------------------------------
// DEMANDE DE MESURE (non-bloquante)
// Lance la conversion — retour immédiat, pas d'attente
// ----------------------------------------------------------------------------
void requestDS18Temperatures()
{
    sensors.requestTemperatures();
}

// ----------------------------------------------------------------------------
// LECTURE DU RÉSULTAT (à appeler 800ms après requestDS18Temperatures)
// ----------------------------------------------------------------------------
void updateDS18Cache()
{
    // Lit la température de l'index 0 (première sonde sur le bus OneWire)
    float temp = sensors.getTempCByIndex(0);

    // On ne met à jour le cache que si la lecture est physiquement plausible (> -20°C)
    // -127°C = valeur d'erreur DS18B20 au démarrage ou sonde déconnectée
    if (temp > -20.0f && temp < 85.0f)
    {
        _waterTempCache = temp;
        _tempValid      = true;
    }
    else
    {
        // Sonde déconnectée ou erreur : invalider le cache pour éviter une valeur périmée
        _waterTempCache = -127.0f;
        _tempValid      = false;
        logSystem(WARNING, "DS18", "Lecture ignorée : " + String(temp, 1) + "°C — sonde hors plage ou déconnectée");
    }
}

// ----------------------------------------------------------------------------
// ACCESSEUR — retourne la dernière valeur en cache
// ----------------------------------------------------------------------------
float getWaterTemp()
{
    return _waterTempCache;
}

bool isTempValid()
{
    return _tempValid;
}
