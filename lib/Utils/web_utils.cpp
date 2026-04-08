#include "includes.h"
#include "web_utils.h"
#include "sensors_manager.h"
#include "WaterTempManager.h"
#include "PumpManager.h"

extern TinyGPSPlus gps;

static AsyncWebServer server(80);

// ── MARCHE / ARRÊT FORCÉ (BOOST) ────────────────────────────────────────────
static unsigned long boostEndMs    = 0;
static bool          _boostForceOn = true; // true = marche forcée, false = arrêt forcé

bool isBoostActive() {
    return (boostEndMs > 0 && millis() < boostEndMs);
}

bool isBoostForceOn() {
    return _boostForceOn;
}

// Durée configurée en NVS (namespace "schedule", clé "boostMin") — paliers de 30 min
static int _getBoostDurationMinutes() {
    Preferences prefs;
    prefs.begin("schedule", true);
    int val = prefs.getInt("boostMin", 60); // défaut 60 min
    prefs.end();
    return val;
}

int getBoostDurationMinutes() {
    return _getBoostDurationMinutes();
}

bool setBoostDurationMinutes(int minutes) {
    // Multiple de 30, entre 30 et 480 min (8h max)
    if (minutes < 30 || minutes > 480 || (minutes % 30) != 0) return false;
    Preferences prefs;
    prefs.begin("schedule", false);
    prefs.putInt("boostMin", minutes);
    prefs.end();
    return true;
}
// ─────────────────────────────────────────────────────────────────────────────

void startWebServer()
{
    // 1. Pages HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });

    // 2. Endpoint /sensors (Données, états pompe, niveau et mode)
    server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // ── CORRECTION POINT 2 ────────────────────────────────────────────────
        // On utilise directement getCurrentMode() et getModeString() au lieu de
        // lire la variable globale currentMode via extern.
        // Cela supprime le couplage fragile entre main.cpp et web_utils.cpp,
        // et garantit que le mode affiché correspond toujours à l'état physique réel.
        // Note : MODE_OFF=0, MODE_MANU=1, MODE_AUTO=2 (voir ModeManager.h)
        String modeStr = getModeString();
        // ────────────────────────────────────────────────────────────────────────

        String json;
        if (!isSensorOK()) {
            json = "{\"error\":\"sensor_unavailable\"}";
        } else {
            float t  = getInternalTemp();
            float h  = getInternalHumidity();
            float tw = getWaterTemp();
            bool  pompeActive  = isPumpRunning();
            bool  niveauOk     = isWaterLevelOk();
            float filtFait     = getPumpingDoneToday();
            float filtObjectif = calculateTargetHours(tw);
            float heureDebut   = getFilterStartHour();
            float heureFin     = getFilterEndHour();

            json  = "{";
            json += "\"temperature\":"     + String(t, 1)  + ",";
            json += "\"humidity\":"         + String(h, 1)  + ",";
            json += "\"waterTemperature\":" + String(tw, 1) + ",";
            json += "\"pumpActive\":"       + String(pompeActive ? "true" : "false") + ",";
            json += "\"waterLevel\":"       + String(niveauOk    ? "true" : "false") + ",";
            json += "\"mode\":\""           + modeStr + "\",";
            json += "\"antiGel\":"          + String(isAntiGelActif()  ? "true" : "false") + ",";
            json += "\"canicule\":"         + String(isCaniculeActif() ? "true" : "false") + ",";
            json += "\"filtFait\":"         + String(filtFait, 2)     + ",";
            json += "\"filtObjectif\":"     + String(filtObjectif, 2) + ",";
            json += "\"filtDebut\":"        + String(heureDebut)      + ",";
            json += "\"filtFin\":"           + String(heureFin)                           + ",";
            json += "\"motorFault\":"        + String(isMotorFaultActive()  ? "true" : "false") + ",";
            json += "\"motorFaultLatched\":" + String(isMotorFaultLatched() ? "true" : "false") + ",";
            bool gpsOk = gps.time.isValid() && gps.satellites.value() >= 4 && gps.time.age() < 5000;
            json += "\"gpsSats\":"           + String(gps.satellites.isValid() ? (int)gps.satellites.value() : 0) + ",";
            json += "\"gpsOk\":"             + String(gpsOk ? "true" : "false") + ",";
            bool boostOn = isBoostActive();
            unsigned long boostRem = boostOn ? ((boostEndMs - millis()) / 1000UL) : 0UL;
            json += "\"boostActive\":"    + String(boostOn ? "true" : "false") + ",";
            json += "\"boostForceOn\":"   + String(isBoostForceOn() ? "true" : "false") + ",";
            json += "\"boostRemaining\":" + String(boostRem) + ",";
            json += "\"boostDuration\":"  + String(getBoostDurationMinutes()) + ",";
            json += "\"modeHistory\":"    + mhToJSON();
            json += "}";
        }
        request->send(200, "application/json", json);
    });

    // 3. Route /reset_motor_fault (Réarmement manuel défaut moteur)
    server.on("/reset_motor_fault", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (isMotorFaultLatched()) {
            // Le verrou est actif : on réarme (la vérification du contact physique
            // se fera au prochain tour de loop via l'anti-rebond)
            logSystem(INFO, "SAFETY", "Demande de rearmement defaut moteur reçue");
        }
        resetMotorFault();
        logSystem(INFO, "SAFETY", "Defaut moteur rearmé manuellement depuis interface web");
        request->send(200, "text/plain", "OK");
    });

    // 4. Route /pump (Boutons Marche/Arrêt depuis la page web)
    server.on("/pump", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // ── CORRECTION POINT 2 ────────────────────────────────────────────────
        // Utilisation de getCurrentMode() et de l'enum MODE_MANU
        // au lieu de comparer un int à une valeur littérale.
        if (getCurrentMode() != MODE_MANU)
        {
            logSystem(WARNING, "WEB", "Commande refusee : passez en mode MANUEL");
            request->send(403, "text/plain", "Commande refusee : passez en mode MANUEL");
            return;
        }
        // ────────────────────────────────────────────────────────────────────────

        if (request->hasParam("status")) {
            String status = request->getParam("status")->value();
            logSystem(INFO, "WEB", "Commande recue : " + status);

            if (status == "on") {
                logSystem(INFO, "WEB", "Allumage Pompe (Mode Manuel)");
                setPumpRequest(true);
            } else {
                logSystem(INFO, "WEB", "Arret Pompe (Mode Manuel)");
                setPumpRequest(false);
            }
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Parametre manquant");
        }
    });

    
    // 5. Route /boost (Marche forcée 1h — MODE AUTO uniquement)
    server.on("/boost", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (getCurrentMode() != MODE_AUTO) {
            request->send(403, "text/plain", "Boost disponible uniquement en mode AUTO");
            return;
        }
        if (!request->hasParam("action")) {
            request->send(400, "text/plain", "Parametre manquant");
            return;
        }
        String action = request->getParam("action")->value();
        if (action == "start") {
            int durMin    = _getBoostDurationMinutes();
            _boostForceOn = !isPumpRunning(); // inverse l'état actuel : ON→arrêt forcé, OFF→marche forcée
            boostEndMs    = millis() + (unsigned long)(durMin * 60000);
            logSystem(INFO, "WEB",
                String(_boostForceOn ? "Marche" : "Arret") + " force " + String(durMin) + "min demarre");
            // Timeline : type 4 = marche forcée (violet), type 5 = arrêt forcé (rouge)
            float dH = getDecimalHour();
            if (dH >= 0.0f) mhStart(_boostForceOn ? 4 : 5, dH);
        } else if (action == "stop") {
            // Retour en AUTO normal sur la timeline
            float dH = getDecimalHour();
            if (dH >= 0.0f) mhStart(1, dH);
            boostEndMs = 0;
            logSystem(INFO, "WEB", "Marche forcee annulee");
        } else {
            request->send(400, "text/plain", "Action invalide");
            return;
        }
        request->send(200, "text/plain", "OK");
    });

    // 6. Routes lecture des journaux CSV
    auto serveLog = [](AsyncWebServerRequest* req, String (*pathFn)()) {
        String path = pathFn();
        if (LittleFS.exists(path)) {
            req->send(LittleFS, path, "text/plain; charset=utf-8");
        } else {
            req->send(200, "text/plain; charset=utf-8", "(aucun enregistrement ce mois-ci)");
        }
    };
    server.on("/log/sessions", HTTP_GET, [serveLog](AsyncWebServerRequest* req) {
        serveLog(req, getSessionLogPath);
    });
    server.on("/log/daily", HTTP_GET, [serveLog](AsyncWebServerRequest* req) {
        serveLog(req, getDailyLogPath);
    });
    server.on("/log/alertes", HTTP_GET, [serveLog](AsyncWebServerRequest* req) {
        serveLog(req, getAlerteLogPath);
    });

    // 7. Route /schedule — lecture de la plage horaire configurée
    server.on("/schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"start\":" + String(getConfiguredStartHour())
                    + ",\"end\":"   + String(getConfiguredEndHour()) + "}";
        request->send(200, "application/json", json);
    });

    // 8. Route /set-schedule?start=X&end=Y — modification de la plage horaire
    server.on("/set-schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("start") || !request->hasParam("end")) {
            request->send(400, "text/plain", "Parametres manquants (start, end)");
            return;
        }
        float s = request->getParam("start")->value().toFloat();
        float e = request->getParam("end")->value().toFloat();
        if (!setFilterSchedule(s, e)) {
            request->send(400, "text/plain", "Valeurs invalides (0<=debut<=23, 1<=fin<=24, debut<fin)");
            return;
        }
        request->send(200, "text/plain", "OK");
    });

    // 9. Route /clear-logs — Suppression des journaux du mois courant
    server.on("/clear-logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        int deleted = 0;
        String p;
        p = getSessionLogPath(); if (LittleFS.exists(p)) { LittleFS.remove(p); deleted++; }
        p = getDailyLogPath();   if (LittleFS.exists(p)) { LittleFS.remove(p); deleted++; }
        p = getAlerteLogPath();  if (LittleFS.exists(p)) { LittleFS.remove(p); deleted++; }
        if (LittleFS.exists("/systeme.log")) { LittleFS.remove("/systeme.log"); deleted++; }
        logSystem(INFO, "WEB", "Journaux effaces depuis interface web (" + String(deleted) + " fichiers)");
        request->send(200, "text/plain", "OK");
    });

    // 10. Route /set-boost-duration?minutes=X — durée du boost (multiple de 30, 30–480 min)
    server.on("/set-boost-duration", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("minutes")) {
            request->send(400, "text/plain", "Parametre manquant (minutes)");
            return;
        }
        int m = request->getParam("minutes")->value().toInt();
        if (!setBoostDurationMinutes(m)) {
            request->send(400, "text/plain", "Valeur invalide (multiple de 30, entre 30 et 480)");
            return;
        }
        logSystem(INFO, "WEB", "Duree boost : " + String(m) + " min");
        request->send(200, "text/plain", "OK");
    });

    server.serveStatic("/", LittleFS, "/");
    server.begin();
    logSystem(INFO, "WEB", "Serveur Web pret");
}
