#include "DebugManager.h"

void initDebugManager()
{
    // Aucune configuration de broche physique
}

bool isDebugEnabled()
{
#ifdef DEBUG_ENABLED
    return true;
#else
    return false;
#endif
}

void logSystem(LogLevel level, String module, String message)
{
    if (!isDebugEnabled()) return;

    String prefix;
    switch (level)
    {
        case CRITICAL: prefix = "!!! [CRITICAL] "; break;
        case WARNING:  prefix = "  ! [WARNING]  "; break;
        default:       prefix = "    [INFO]     "; break;
    }

    Serial.print(prefix);
    Serial.print(module);
    Serial.print(" : ");
    Serial.println(message);
}