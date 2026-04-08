#include "sensors_manager.h"
#include <Wire.h>
#include "config.h"
#include "DebugManager.h"

// Instance statique du capteur AHT10
static Adafruit_AHTX0 aht;

// --- Cache interne ---
// Ces variables sont mises à jour par updateSensorCache() dans la boucle loop()
// Elles permettent au serveur Web de répondre instantanément sans bloquer le bus I2C.
static float _cachedTemp = 0.0f;
static float _cachedHumidity = 0.0f;
static bool _sensorOK = false;

// ------------------------------------------------------------------
// Initialisation du capteur
// ------------------------------------------------------------------
bool initAHT10()
{
  if (!aht.begin())
  {
    logSystem(CRITICAL, "AHT10", "AHT10 non trouve sur le bus I2C !");
    _sensorOK = false;
    return false;
  }
  logSystem(INFO, "AHT10", "AHT10 initialise avec succes");
  _sensorOK = true;
  return true;
}

// ------------------------------------------------------------------
// Mise à jour du cache (À APPELER DANS LE loop() TOUTES LES 5-10 SEC)
// ------------------------------------------------------------------
void updateSensorCache()
{
  if (!_sensorOK)
    return; // Si le capteur a échoué à l'init, on ne fait rien

  sensors_event_t humidity, temp;

  // Une seule lecture I2C pour récupérer les deux valeurs d'un coup
  if (aht.getEvent(&humidity, &temp))
  {
    _cachedTemp = temp.temperature;
    _cachedHumidity = humidity.relative_humidity;
  }
  else
  {
    logSystem(WARNING, "AHT10", "Echec de lecture capteur AHT10");
  }
}

// ------------------------------------------------------------------
// Accesseurs : Ces fonctions sont appelées par web_utils.cpp
// Elles retournent la valeur en mémoire, c'est ultra-rapide.
// ------------------------------------------------------------------
float getInternalTemp()
{
  return _cachedTemp;
}

float getInternalHumidity()
{
  return _cachedHumidity;
}

bool isSensorOK()
{
  return _sensorOK;
}