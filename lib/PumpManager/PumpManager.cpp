#include "includes.h"
// Variable interne pour mémoriser ce que l'utilisateur veut
static bool pumpRequest = false;
// Variable pour savoir si la pompe est réellement active
static bool pumpState = false;
// Etat du relais niveau bas (évite les écritures PCF répétées)
static bool niveauRelaisAllume = false;
// --- Variables de suivi de session (log) ---
static String        _sessionStartTime = "";
static String        _sessionMode      = "";
static unsigned long _sessionStartMs   = 0;
static int           _dailySessionCount = 0;

// --- Variables de gestion de la pompe ---

void initPumpManager()
{
    pumpRequest = false;
    pumpState = false;
    forceStopPump(); // Sécurité au démarrage
}

// Point d'entrée unique pour exprimer une intention (MARCHE/ARRET).
// Ne commande pas directement le relais — c'est updatePumpSystem() qui
// applique la demande après vérification de sécurité.
void setPumpRequest(bool state)
{
    if (state == pumpRequest)
        return; // Rien de nouveau, on sort
    pumpRequest = state;
    logSystem(INFO, "PUMP", state ? "Demande MARCHE" : "Demande ARRET");
}

bool getPumpRequest()
{
    return pumpRequest;
}

float pumpingDoneToday = 0.0;   // Compteur cumulé en heures
unsigned long lastTickPump = 0; // Mémorise le dernier passage (ms)

void updatePumpSystem()
{
    // -------------------------------------------------------------------------
    // 1. CALCUL DU TEMPS ÉCOULÉ (CHRONOMÉTRAGE)
    // -------------------------------------------------------------------------
    unsigned long now = millis();

    // On calcule la différence de temps depuis le dernier tour de boucle
    if (lastTickPump > 0)
    {
        // On calcule l'écart en secondes (float pour la précision)
        float secondsElapsed = (now - lastTickPump) / 1000.0;

        // SI LA POMPE TOURNE : On ajoute ce temps au compteur journalier
        // Division par 3600.0 pour convertir les secondes en fraction d'heure
        if (pumpState == true)
        {
            pumpingDoneToday += (secondsElapsed / 3600.0);
        }
    }
    lastTickPump = now; // Mise à jour du repère pour le prochain tour

    // -------------------------------------------------------------------------
    // 2. VÉRIFICATION DE LA SÉCURITÉ (SENTINELLE)
    // -------------------------------------------------------------------------
    bool safe = isSystemSafe();

    if (safe)
    {
        // --- CAS : TOUT VA BIEN (SYSTÈME OPÉRATIONNEL) ---

        // Si l'intention (pumpRequest) diffère de la réalité (pumpState)
        if (pumpRequest != pumpState)
        {
            static unsigned long lastRelayChange = 0;
            unsigned long tNow = millis();
            unsigned long delai = tNow - lastRelayChange;

            bool wasRunning = pumpState;
            pumpState = pumpRequest;
            setRelay(PIN_RELAY_POMPE, pumpState);

            // Historique des modes (MANU uniquement — AUTO géré dans main.cpp, bloc "HISTORIQUE AUTO" en loop())
            if (getCurrentMode() == MODE_MANU) {
                float dH = getDecimalHour();
                if (dH >= 0.0f) mhStart(pumpState ? 2 : 3, dH);
            }

            // ── Tracking session log ──────────────────────────────────────────
            if (pumpState) {
                // Pompe vient de démarrer : mémorise le début et le mode actif
                _sessionStartTime = logGetTimeStr();
                _sessionMode      = isBoostActive() ? "BOOST" : getModeString();
                _sessionStartMs   = millis();
                _dailySessionCount++;
            } else if (wasRunning && _sessionStartMs > 0) {
                // Pompe vient de s'arrêter (arrêt contrôlé)
                int    dureeMin = (int)((millis() - _sessionStartMs) / 60000UL);
                String fin      = logGetTimeStr();
                // Cause dynamique : arrêt forcé boost, fin de boost, ou arrêt normal
                String cause = "NORMAL";
                if (isBoostActive() && !isBoostForceOn()) cause = "ARRET_BOOST";
                else if (_sessionMode == "BOOST")          cause = "FIN_BOOST";
                logSession(_sessionStartTime, fin, dureeMin,
                           getWaterTemp(), _sessionMode.c_str(), cause.c_str(), true);
                _sessionStartMs = 0;
                // Sauvegarde NVS immédiate à chaque arrêt pompe (minimise la perte en cas de coupure)
                saveFiltrationProgress(pumpingDoneToday);
                struct tm _tmStop;
                if (getLocalTime(&_tmStop) && _tmStop.tm_year >= 124) saveLastDay(_tmStop.tm_mday);
            }
            // ─────────────────────────────────────────────────────────────────

            // PCF : LOW(0)=relais collé(pompe ON), HIGH(1)=relais ouvert(pompe OFF)
            int pcfEtat = pumpState ? 0 : 1;
            logSystem(INFO, "RELAY",
                "PCF pin" + String(PIN_RELAY_POMPE) + "=" + String(pcfEtat) +
                " (" + String(pumpState ? "MARCHE" : "ARRET") + ")" +
                " | delai depuis dernier chgt: " + String(delai) + "ms");

            lastRelayChange = tNow;
        }

        // Sécurité : éteindre la LED défaut uniquement si état a changé
        if (niveauRelaisAllume) {
            setRelay(PIN_RELAY_DEFAUT_SYSTEME, false);
            niveauRelaisAllume = false;
        }
    }
    else
    {
        // --- CAS : DANGER DÉTECTÉ (NIVEAU D'EAU BAS OU AUTRE) ---

        // Si la pompe tourne encore, on coupe tout immédiatement
        if (pumpState == true)
        {
            // Arrêt forcé : log la session avant de couper
            if (_sessionStartMs > 0) {
                int    dureeMin = (int)((millis() - _sessionStartMs) / 60000UL);
                String fin      = logGetTimeStr();
                logSession(_sessionStartTime, fin, dureeMin,
                           getWaterTemp(), _sessionMode.c_str(),
                           getSafetyStatusMessage().c_str(), true);
                logAlerte(getSafetyStatusMessage().c_str(),
                          String(getWaterTemp(), 1).c_str());
                _sessionStartMs = 0;
                // Sauvegarde NVS immédiate à chaque arrêt forcé
                saveFiltrationProgress(pumpingDoneToday);
                struct tm _tmForce;
                if (getLocalTime(&_tmForce) && _tmForce.tm_year >= 124) saveLastDay(_tmForce.tm_mday);
            }
            forceStopPump();
            logSystem(CRITICAL, "PUMP", "Arrêt forcé — " + getSafetyStatusMessage());
        }

        // Signalisation visuelle du défaut uniquement si pas déjà allumée
        if (!niveauRelaisAllume) {
            setRelay(PIN_RELAY_DEFAUT_SYSTEME, true);
            niveauRelaisAllume = true;
        }
    }

    static unsigned long lastSave = 0;
    if (millis() - lastSave >= 300000) { // Sauvegarde toutes les 5 minutes
        lastSave = millis();
        saveFiltrationProgress(pumpingDoneToday);
        struct tm _tmloc;
        if (getLocalTime(&_tmloc) && _tmloc.tm_year >= 124) {
            saveLastDay(_tmloc.tm_mday);
        }
        // [RETIRÉ] logToFile("Sauvegarde auto : ...") — supprimé volontairement.
        // Raison : cette ligne écrivait dans /systeme.log toutes les 5 min → 288 lignes/jour
        // → saturation LittleFS en 2-3 mois. La vraie sauvegarde est dans la NVS (saveFiltrationProgress).
        // Le logSystem() ci-dessous conserve l'affichage sur le port série en direct.
        logSystem(INFO, "PUMP", "Sauvegarde auto : " + String(pumpingDoneToday, 2) + "h (" + String((int)(pumpingDoneToday * 60)) + " min)");
    }

}

void forceStopPump()
{
    // Action directe sur le matériel via le driver PCF
    setRelay(PIN_RELAY_POMPE, false);
    pumpState = false;
}

bool isPumpRunning()
{
    return pumpState;
}

float getPumpingDoneToday()
{
    return pumpingDoneToday;
}

int getDailySessionCount()   { return _dailySessionCount; }
void resetDailySessionCount() { _dailySessionCount = 0; }