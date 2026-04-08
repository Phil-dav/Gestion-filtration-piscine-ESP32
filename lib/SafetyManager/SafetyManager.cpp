#include "SafetyManager.h"
#include "WaterLevel.h"
#include "pcf8574_driver.h"
#include "pcf8574_config.h"

static String        lastError          = "RAS";
static bool          motorFaultLatched  = false; // Verrou : reste vrai jusqu'au réarmement manuel
static unsigned long motorFaultSince    = 0;     // Horodatage 1ère détection (anti-rebond)
static bool          motorFaultConfirmed = false; // true = défaut confirmé après délai

// Délai de confirmation avant verrouillage (élimine glitches I2C et bruit électrique)
static const unsigned long MOTOR_FAULT_DEBOUNCE_MS = 200;

bool isSystemSafe() {
    // Si le PCF est absent, aucune lecture possible : on ne peut ni détecter ni signaler de défaut
    if (!isPcfReady()) return true;

    // --- DÉTECTION PERMANENTE du défaut moteur (s'exécute toujours, avant tout return) ---
    // Contact NF (Normalement Fermé) :
    //   - Fonctionnement normal  → contact FERMÉ → PCF lit LOW
    //   - Défaut (disjoncteur déclenché OU fil coupé) → contact OUVERT → PCF lit HIGH
    // Anti-rebond : le défaut doit être stable pendant MOTOR_FAULT_DEBOUNCE_MS avant verrouillage.
    if (pcf_read(PIN_DEFAUT_RELAIS) == HIGH) {
        if (motorFaultSince == 0) {
            motorFaultSince = millis(); // Première détection : démarre le chrono
        }
        if (millis() - motorFaultSince >= MOTOR_FAULT_DEBOUNCE_MS) {
            motorFaultLatched   = true; // Défaut stable confirmé : verrouillage
            motorFaultConfirmed = true;
        }
    } else {
        // Contact fermé (état normal) : réinitialise le chrono anti-rebond
        motorFaultSince    = 0;
        motorFaultConfirmed = false;
    }

    // 1. Vérification du Niveau d'eau (D34)
    if (!isWaterLevelOk()) {
        lastError = "MANQUE EAU";
        return false;
    }

    // 2. Défaut moteur confirmé
    if (motorFaultConfirmed) {
        lastError = "DEFAUT MOTEUR";
        return false;
    }

    // 3. Verrou maintenu même si le contact s'est refermé
    // La pompe reste bloquée jusqu'au réarmement manuel via resetMotorFault()
    if (motorFaultLatched) {
        lastError = "DEFAUT MOTEUR (REARMEMENT REQUIS)";
        return false;
    }

    // Si on arrive ici, tout est vert
    lastError = "OK";
    return true;
}

String getSafetyStatusMessage() {
    return lastError;
}

// Réarmement manuel : appelé depuis l'interface web ou un bouton physique
void resetMotorFault() {
    motorFaultLatched   = false;
    motorFaultConfirmed = false;
    motorFaultSince     = 0;
}

// true = défaut confirmé OU verrou en attente de réarmement
// N'effectue PAS de lecture PCF ici (appelé depuis Core 0 / web handler)
bool isMotorFaultActive() {
    return motorFaultConfirmed || motorFaultLatched;
}

bool isMotorFaultLatched() {
    return motorFaultLatched;
}
