#include "pcf8574_driver.h"
#include "pcf8574_config.h"
#include "config.h"
#include "DebugManager.h"

// 1. DÉFINITION DE L'OBJET (Indispensable pour la compilation)
PCF8574 pcf(PCF8574_ADDRESS);
static bool _pcfReady = false;

bool pcf_init()
{
    // On démarre le bus I2C (Wire.begin() doit être fait dans le main.cpp avant)
    if (!pcf.begin())
    {
        logSystem(CRITICAL, "PCF", "PCF8574 non detecte !");
        _pcfReady = false;
        return false;
    }

    // 2. SÉCURITÉ ET PRÉPARATION
    // On écrit 1 partout (0xFF) :
    // - Éteint tous les relais (logique inversée)
    // - Prépare les pins 4-7 pour la lecture
    pcf.write8(0xFF);

    _pcfReady = true;
    logSystem(INFO, "PCF", "PCF8574 pret - Relais OFF, Entrees parees");
    return true;
}

bool isPcfReady()
{
    return _pcfReady;
}

void setRelay(uint8_t pin, bool active)
{
    // Si active = true, on envoie LOW (0) pour coller le relais
    // Si active = false, on envoie HIGH (1) pour libérer le relais
    pcf.write(pin, active ? LOW : HIGH);
}

int pcf_read(uint8_t pin)
{
    return pcf.read(pin);
}