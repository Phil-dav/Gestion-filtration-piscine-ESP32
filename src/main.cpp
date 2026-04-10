// --------------------------------------------------------------------------------
// PROGRAMME PRINCIPAL - GESTION DOMOTIQUE PISCINE
//--------------------------------------------------------------------------------
#include "includes.h"
#include "web_utils.h"
#include "rom/rtc.h"   // rtc_get_reset_reason() — registre hardware bas niveau
// Objets externes
extern void setPumpRequest(bool state);
extern bool getPumpRequest();
extern TinyGPSPlus gps;
extern float pumpingDoneToday;
extern int gpsLocalHour;

// État matériel critique — initialisé dans setup(), utilisé dans loop()
static bool pcfOK = false;
// --------------------------------------------------------------------------------
// CONFIGURATION INITIALE (SETUP)
// --------------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(100); // Stabilisation Serial — seul delay() autorisé dans ce projet (setup uniquement)
    // 1. On réveille le stockage d'abord
    initStorage();
    initLogManager(); // Crée le dossier /logs si absent
    // --- LECTURE DE VÉRIFICATION AU DÉMARRAGE ---
    logSystem(INFO, "BOOT", "---------- RÉCUPÉRATION DES LOGS ----------");

    // a. Afficher la valeur numérique sauvegardée (NVS)
    float charge = loadFiltrationProgress();
    logSystem(INFO, "BOOT", "Mémoire NVS : reprise à " + String(charge, 2) + "h (" + String((int)(charge * 60)) + " min)");

    // b. Afficher le journal système (LittleFS)
    File file = LittleFS.open("/systeme.log", FILE_READ);
    if (file)
    {
        logSystem(INFO, "BOOT", "Journal Système (/systeme.log) :");
        if (isDebugEnabled())
        {
            while (file.available()) Serial.write(file.read());
            Serial.println();
        }
        file.close();
    }
    logSystem(INFO, "BOOT", "--------------------------------------------");

    // 2. On charge la progression du jour en cours
    pumpingDoneToday = charge;
    loadFilterSchedule(); // Plage horaire configurable (NVS)
    logToFile("Système redémarré. Reprise filtration à " + String(pumpingDoneToday, 2) + "h");
    // 3. On enregistre le démarrage
    logToFile("Système initialisé - Redémarrage carte");

    // 4. Cause du redémarrage — registre hardware bas niveau (plus fiable qu'esp_reset_reason)
    //    rtc_get_reset_reason(0) lit le registre RTC_CNTL_RESET_CAUSE_PROCPU directement
    //    et distingue POWERON_RESET (coupure secteur) de RTCWDT_RTC_RESET (bouton EN)
    //    contrairement à esp_reset_reason() qui retourne ESP_RST_POWERON dans les deux cas.
    RESET_REASON cpu0 = rtc_get_reset_reason(0);
    String causeReset;
    switch (cpu0) {
        case POWERON_RESET:          causeReset = "COUPURE_COURANT";  break; //  1 - Vbat power on
        case SW_RESET:               causeReset = "RESET_LOGICIEL";   break; //  3 - Software reset digital core
        case OWDT_RESET:                                                      //  4 - Legacy watchdog
        case TG0WDT_SYS_RESET:                                                //  7 - Timer Group 0 watchdog
        case TG1WDT_SYS_RESET:                                                //  8 - Timer Group 1 watchdog
        case RTCWDT_SYS_RESET:                                                //  9 - RTC watchdog digital core
        case TGWDT_CPU_RESET:                                                 // 11 - Timer Group watchdog CPU
        case RTCWDT_CPU_RESET:       causeReset = "WATCHDOG";         break; // 13 - RTC watchdog CPU
        case SW_CPU_RESET:           causeReset = "RESET_LOGICIEL";   break; // 12 - Software reset CPU
        case RTCWDT_BROWN_OUT_RESET: causeReset = "SOUS_TENSION";     break; // 15 - Brownout
        case RTCWDT_RTC_RESET:       causeReset = "BOUTON_RESET";     break; // 16 - Bouton EN (hard reset)
        default:                     causeReset = "AUTRE_" + String((int)cpu0); break;
    }
    logSystem(INFO, "BOOT", "Cause redémarrage : " + causeReset + " (code RTC=" + String((int)cpu0) + ")");

    logSystem(INFO, "BOOT", "--- DEMARRAGE DU SYSTEME ---");

    // 5. Initialisation du Bus I2C (SDA, SCL)
    Wire.begin(I2C_SDA, I2C_SCL);

    // 6. Initialisation du PCF8574 (Relais & Entrées)
    pcfOK = pcf_init();
    if (!pcfOK) {
        logSystem(CRITICAL, "HW", "PCF8574 introuvable sur I2C — relais hors service !");
    } else {
        initPumpManager(); // S'assure que tout est à OFF au départ
    }

    logSystem(INFO, "BOOT", "Mode interrupteur au démarrage : " + getModeString());

    // 7. Interface Humaine (OLED)
    initOLED();
    displayMessage("Initialisation...");

    // 8. Capteur eau (DS18B20)
    initDS18B20();

    // 9. Services Réseau & Temps
    initWiFiAuto();
    startWebServer();
    initTime();

    // Alerte de redémarrage : attendre que NTP fournisse une date valide (max 5s)
    // sans quoi le fichier mensuel serait nommé alertes_0170.csv (epoch 1970)
    {
        struct tm _tNtp;
        for (int i = 0; i < 50; i++) {
            if (getLocalTime(&_tNtp, 100) && _tNtp.tm_year >= 124) break;
            delay(100);
        }
        // Diagnostic dual-core : cpu0=PRO cpu1=APP — EN button peut différer sur cpu1
        RESET_REASON cpu1 = rtc_get_reset_reason(1);
        String alerteReset = causeReset + " [cpu0=" + String((int)cpu0) + " cpu1=" + String((int)cpu1) + "]";
        logAlerte("REDEMARRAGE", alerteReset.c_str());
    }

    // 10. Capteurs environnementaux & GPS
    if (!initAHT10())
        logSystem(WARNING, "HW", "AHT10 introuvable — température/humidité air indisponibles");
    initGPS();

    logSystem(INFO, "BOOT", "Système prêt !");
}

// --------------------------------------------------------------------------------
// BOUCLE PRINCIPALE (LOOP)
// --------------------------------------------------------------------------------
void loop()
{
    // 1. Récupération du temps actuel
    uint32_t now = millis();
    static uint32_t lastDisplay = 0;
    static uint32_t lastWifiCheck = 0;
    static uint32_t lastSensorUpdate = 0; // Timer AHT10 (toutes les 5s)
    static uint32_t lastDS18Request = 0;  // Timer demande DS18B20
    static bool ds18Requested = false;    // Attente de lecture DS18B20
    static int currentHour = 12;            // Heure réelle utilisée (GPS ou NTP valide), partagée avec le log
    static bool surFiltrationLogged = false; // One-shot log dépassement objectif filtration
    static bool gelLogOnce = false;          // One-shot log alerte gel

    char dateTime[25] = "Recherche...";
    char sourceLabel[20] = "Hors ligne";

    // ----------------------------------------------------------------------------
    // PRIORITÉ 1 : FLUX TEMPS RÉEL (Capteurs & Commandes)
    // ----------------------------------------------------------------------------

    // A. Bouton OLED — lecture à chaque cycle pour réactivité maximale
    handleOledButton();

    // B. Lecture constante du GPS pour ne pas perdre de données
    updateGPS();

    // C. Lecture de l'interrupteur physique (MANU / OFF / AUTO)
    OperationMode modePhysique = getCurrentMode();

    // --- DETECTION CHANGEMENT DE MODE ---
    static OperationMode dernierEtatPhysique = MODE_OFF;
    static String        _offStartTime       = "";
    static unsigned long _offStartMs         = 0;
    static bool          mhInitDone          = false;

    // Initialisation ModeHistory au premier tick avec une heure valide
    if (!mhInitDone) {
        float dH = getDecimalHour();
        if (dH >= 0.0f) {
            uint8_t mhT = (modePhysique == MODE_AUTO) ? 1
                        : (modePhysique == MODE_MANU) ? 3
                        : 0;
            mhStart(mhT, dH);
            mhInitDone = true;
        }
    }

    if (modePhysique != dernierEtatPhysique)
    {
        // Quitter MODE_OFF → loguer la période d'arrêt volontaire dans sessions
        if (dernierEtatPhysique == MODE_OFF && _offStartMs > 0) {
            int dureeMin = (int)((millis() - _offStartMs) / 60000UL);
            logSession(_offStartTime, logGetTimeStr(), dureeMin,
                       getWaterTemp(), "OFF",
                       ("RETOUR_" + getModeString(modePhysique)).c_str(), false);
            _offStartMs = 0;
        }
        // Entrer en MODE_OFF → mémoriser le début de la période
        if (modePhysique == MODE_OFF) {
            _offStartTime = logGetTimeStr();
            _offStartMs   = millis();
        }
        dernierEtatPhysique = modePhysique;
        logToFile("Changement mode : " + getModeString(modePhysique));
        logSystem(INFO, "MODE", "Passage en mode " + getModeString(modePhysique));

        // Historique des modes pour la timeline web
        float dH = getDecimalHour();
        if (dH >= 0.0f) {
            uint8_t mhT = (modePhysique == MODE_AUTO) ? 1
                        : (modePhysique == MODE_MANU) ? 3
                        : 0;
            mhStart(mhT, dH);
        }
    }

    bool demandeFinal = false;

    // --- DÉTERMINATION DU JOUR POUR LE RESET ---
    // Chargé depuis NVS au 1er passage : évite le faux reset au reboot
    static int dernierJour = loadLastDay();
    int jourPourPompe = -1;

    // Source de la date : GPS si signal frais (< 5s), sinon NTP via WiFi
    if (gps.date.isValid() && gps.date.age() < 5000)
    {
        jourPourPompe = gps.date.day();
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo) && timeinfo.tm_year >= 124)
        {
            jourPourPompe = timeinfo.tm_mday;
        }
    }

    // --- REMISE À ZÉRO À MINUIT (uniquement si le jour a vraiment changé) ---
    // Premier démarrage (NVS vierge) : on initialise sans déclencher de reset
    if (jourPourPompe != -1 && dernierJour == -1)
    {
        dernierJour = jourPourPompe;
        saveLastDay(dernierJour);
        logSystem(INFO, "BOOT", "Premier démarrage : jour initialisé à " + String(jourPourPompe));
    }
    else if (jourPourPompe != -1 && jourPourPompe != dernierJour)
    {
        // Bilan du jour écoulé avant reset
        float targetLog = calculateTargetHours(getWaterTemp());
        logDaily(targetLog, pumpingDoneToday,
                 getDailySessionCount(), getModeString(modePhysique).c_str());
        resetDailySessionCount();
        resetDailyAlertCount();

        pumpingDoneToday = 0.0;
        saveFiltrationProgress(0.0);
        dernierJour = jourPourPompe;
        saveLastDay(dernierJour);
        resetDailyOledStats(); // Reset T° min/max affichées page bilan
        mhReset();             // Efface l'historique des modes (nouveau jour)
        mhInitDone = false;    // Réinitialise au prochain tick valide
        trimSystemLog(200);    // Garde les 200 dernières lignes — évite saturation LittleFS
        purgeOldLogs(2);       // Supprime les CSV /logs/ vieux de plus de 2 mois
        logSystem(INFO, "PUMP", "Nouveau jour (" + String(jourPourPompe) + ") - reset compteur filtration");
    }

    // D. Logique de décision de la pompe (Verrouillage 10s au démarrage)
    if (now > 10000)
    {
        float waterTemp   = getWaterTemp();
        float targetHours = calculateTargetHours(waterTemp);

        // --- PRIORITÉ ABSOLUE : ANTI-GEL (actif même en MODE_OFF) ---
        // En dessous de 2°C : on force la pompe quel que soit le mode physique
        // Guard isTempValid() : évite les fausses alertes gel au démarrage (-127°C)
        if (isTempValid() && waterTemp < 2.0f)
        {
            if (!gelLogOnce) {
                logSystem(WARNING, "GEL", "T° eau < 2°C — pompe forcée (protection gel)");
                logToFile("ALERTE GEL : T° eau = " + String(waterTemp, 1) + "°C — pompe forcée");
                logAlerte("ANTI_GEL", String(waterTemp, 1).c_str());
                gelLogOnce = true;
            }
            demandeFinal = true;
        }
        else
        {
            gelLogOnce = false; // Réinitialise le log one-shot quand on sort du gel

            // --- DÉTERMINATION DE L'HEURE (toujours active, quel que soit le mode) ---
            bool gpsValide = gps.time.isValid() && gps.satellites.value() >= 4 && gps.time.age() < 5000;
            currentHour = gpsValide ? gpsLocalHour : -1;

            if (currentHour == -1 && WiFi.status() == WL_CONNECTED)
            {
                struct tm t;
                // t.tm_year >= 124 = année >= 2024 : rejette l'heure epoch (01/01/1970)
                // retournée par getLocalTime() avant que NTP ait terminé sa sync
                if (getLocalTime(&t) && t.tm_year >= 124) currentHour = t.tm_hour;
            }
            if (currentHour == -1) currentHour = 12;

            if (modePhysique == MODE_MANU)
            {
                // On récupère l'état (Vrai ou Faux) stocké par le bouton Web
                demandeFinal = getPumpRequest();
            }
            else if (modePhysique == MODE_AUTO)
            {
                // --- PLAGE HORAIRE DYNAMIQUE SELON LE MODE THERMIQUE ---
                float heureDebut = getFilterStartHour();
                float heureFin   = getFilterEndHour();

                // --- DÉCISION ---
                bool dansPlage = (currentHour >= heureDebut && currentHour < heureFin);
                bool boostNow  = isBoostActive();
                // Détection expiration naturelle du boost → retour AUTO normal sur la timeline
                static bool _boostWasActive = false;
                if (_boostWasActive && !boostNow) {
                    float dH = getDecimalHour();
                    if (dH >= 0.0f) mhStart(1, dH);
                }
                _boostWasActive = boostNow;
                if (boostNow)
                    demandeFinal = isBoostForceOn(); // Marche ou arrêt forcé web (inverse de l'état actuel)
                else if (pumpingDoneToday < targetHours && dansPlage)
                    demandeFinal = true;
                else
                    demandeFinal = false;
            }
            else // MODE_OFF : arrêt total volontaire
            {
                demandeFinal = false;
            }
        }
    }
    else
    {
        demandeFinal = false; // Sécurité totale pendant les 10 premières secondes
    }

    // E. Application de l'ordre au gestionnaire de pompe (uniquement si hardware présent)
    if (pcfOK) {
        setPumpRequest(demandeFinal);
        updatePumpSystem(); // La sentinelle vérifie si l'ordre est sûr avant d'agir
    }

    // ----------------------------------------------------------------------------
    // PRIORITÉ 2 : CAPTEUR DS18B20 — Non-bloquant (demande toutes les 5s)
    // ----------------------------------------------------------------------------
    if (now - lastDS18Request >= 5000)
    {
        requestDS18Temperatures();
        ds18Requested = true;
        lastDS18Request = now;
    }
    if (ds18Requested && (now - lastDS18Request >= 800))
    {
        updateDS18Cache();
        ds18Requested = false;
    }

    // ----------------------------------------------------------------------------
    // PRIORITÉ 3 : CAPTEUR AHT10 — Toutes les 5 secondes
    // ----------------------------------------------------------------------------
    if (now - lastSensorUpdate >= 5000)
    {
        lastSensorUpdate = now;
        updateSensorCache();
    }

    // ----------------------------------------------------------------------------
    // PRIORITÉ 4 : LOGIQUE CADENCÉE (Toutes les 1 seconde)
    // ----------------------------------------------------------------------------
    if (now - lastDisplay >= 1000)
    {
        lastDisplay = now;

        float tempActuelle = getWaterTemp();
        float currentTarget = calculateTargetHours(tempActuelle);
        float resteAFiltrer = currentTarget - pumpingDoneToday;
        if (resteAFiltrer < 0) {
            if (!surFiltrationLogged) {
                logSystem(INFO, "PUMP", "Objectif dépassé : " + String(pumpingDoneToday, 2) + "h filtrés / " + String(currentTarget, 1) + "h cible");
                surFiltrationLogged = true;
            }
            resteAFiltrer = 0;
        } else {
            surFiltrationLogged = false; // Reset au nouveau jour (quand pumpingDoneToday repasse à 0)
        }

        bool niveauOK = isWaterLevelOk();
        const char *statusEau = niveauOK ? "OK" : "MANQUE !";

        int minutesActuelles = 0;
        if (gps.time.isValid() && gps.time.age() < 5000)
            minutesActuelles = gps.time.minute();
        else if (WiFi.status() == WL_CONNECTED) {
            struct tm _tMin;
            if (getLocalTime(&_tMin) && _tMin.tm_year >= 124) minutesActuelles = _tMin.tm_min;
        }

        float faitEnMinutes = pumpingDoneToday * 60.0;
        float resteEnMinutes = resteAFiltrer * 60.0;

        String modeThermique = isAntiGelActif()  ? "GEL"
                             : isCaniculeActif() ? "CANICULE"
                             : "NORMAL";

        logSystem(INFO, "LOOP",
            "[" + String(currentHour) + "h" + String(minutesActuelles) + "]"
            " Eau:" + String(tempActuelle, 1) + "C"
            " Niv:" + String(statusEau) +
            " Mode:" + getModeString(modePhysique) +
            " Therm:" + modeThermique +
            " Obj:" + String(currentTarget, 1) + "h"
            " Fait:" + String(faitEnMinutes, 0) + "min"
            " Reste:" + String(resteEnMinutes, 0) + "min"
            " Pompe:" + String(isPumpRunning()));

        int nbSat = gps.satellites.value();
        uint32_t ageGPS = gps.time.age();

        if (gps.time.isValid() && nbSat >= 4 && ageGPS < 5000)
        {
            strncpy(dateTime, getGPSPitch().c_str(), sizeof(dateTime) - 1);
            dateTime[sizeof(dateTime) - 1] = '\0';
            snprintf(sourceLabel, sizeof(sourceLabel), "GPS (%d)", nbSat);
        }
        else if (WiFi.status() == WL_CONNECTED)
        {
            strncpy(dateTime, getFormattedTime().c_str(), sizeof(dateTime) - 1);
            dateTime[sizeof(dateTime) - 1] = '\0';
            strncpy(sourceLabel, "WiFi", sizeof(sourceLabel) - 1);
            sourceLabel[sizeof(sourceLabel) - 1] = '\0';
        }

        updateOledDisplay(sourceLabel, dateTime, getInternalTemp(), getInternalHumidity(), getWaterTemp());
    }

    // ----------------------------------------------------------------------------
    // PRIORITÉ 5 : MAINTENANCE RÉSEAU (Toutes les 30 secondes)
    // ----------------------------------------------------------------------------
    if (now - lastWifiCheck > 30000)
    {
        lastWifiCheck = now;
        wl_status_t wifiSt = WiFi.status();
        if (wifiSt != WL_CONNECTED) {
            logSystem(WARNING, "WIFI", "WiFi perdu (code=" + String(wifiSt) + ") - reconnexion...");
            WiFi.begin();
        }
    }
}
