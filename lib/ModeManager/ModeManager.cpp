#include "ModeManager.h"
#include "pcf8574_driver.h"
#include "pcf8574_config.h"

OperationMode getCurrentMode() {
    // Anti-rebond logiciel : exige 3 lectures identiques consécutives avant d'accepter un changement.
    // Protège contre les glitches PCF lors d'écritures simultanées.
    // 3 lectures = compromis réactivité/stabilité (appelé à chaque loop ~50ms → délai ~150ms max).
    static OperationMode confirmedMode = MODE_OFF;
    static OperationMode candidate     = MODE_OFF;
    static uint8_t stableCount         = 0;

    // Si le PCF est absent, retourner MODE_OFF (sécurité)
    if (!isPcfReady()) return MODE_OFF;

    uint8_t pins = pcf.read8();
    OperationMode reading;
    if (!(pins & (1 << PIN_MODE_MANU))) reading = MODE_MANU;
    else if (!(pins & (1 << PIN_MODE_AUTO))) reading = MODE_AUTO;
    else reading = MODE_OFF;

    if (reading == candidate) {
        if (stableCount < 3) stableCount++;
        if (stableCount >= 3) confirmedMode = reading;
    } else {
        candidate   = reading;
        stableCount = 1;
    }
    return confirmedMode;
}

String getModeString(OperationMode m) {
    switch (m) {
        case MODE_MANU: return "MANU";
        case MODE_AUTO: return "AUTO";
        default:        return "OFF";
    }
}

String getModeString() {
    return getModeString(getCurrentMode());
}