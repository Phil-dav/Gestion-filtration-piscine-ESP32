// lib/OLED_Manager/oled_display.cpp
#include "includes.h"
#include "oled_display.h"

extern TinyGPSPlus gps; // défini dans GPS_Manager.cpp

static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// =========================================================================
// Navigation — 5 pages (0 à 4)
// =========================================================================
static const int      PAGE_COUNT        = 5;
static const uint32_t PAGE_TIMEOUT_MS   = 30000; // retour auto page 0 après 30s
static int            currentPage       = 0;
static uint32_t       lastPageChange    = 0;

// =========================================================================
// Gestion bouton (PIN_BP_OLED sur PCF8574 — LOW quand appuyé)
// =========================================================================
static int      lastBtnState  = HIGH;
static uint32_t btnPressStart = 0;
static bool     btnHandled    = false;

static const uint32_t LONG_PRESS_MS = 800; // maintien ≥ 800ms → retour page 0
static const uint32_t DEBOUNCE_MS   = 50;  // ignore les rebonds < 50ms

// =========================================================================
// Veille écran
// =========================================================================
static const uint32_t SLEEP_TIMEOUT_MS = 300000; // 5 min sans activité → écran off
static uint32_t       lastActivity     = 0;       // Initialisé dans initOLED()
static bool           oledAsleep       = false;

static void oledSleep() {
    if (!oledAsleep) {
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        oledAsleep = true;
    }
}

static void oledWake() {
    if (oledAsleep) {
        display.ssd1306_command(SSD1306_DISPLAYON);
        oledAsleep = false;
    }
}

// =========================================================================
// Bilan journalier T° min/max eau
// =========================================================================
static float dailyTempMin =  99.0f;
static float dailyTempMax = -99.0f;

// =========================================================================
// Clignotement alerte
// =========================================================================
static bool     alertVisible   = true;
static uint32_t lastAlertBlink = 0;

// =========================================================================
// Helpers internes
// =========================================================================

// Formate un nombre d'heures float en "XhYY"  ex: 2.25 → "2h15"
static String fmtHM(float hours) { int total = (int)(hours * 60.0f + 0.5f);
    char buf[8];
    snprintf(buf, sizeof(buf), "%dh%02d", total / 60, total % 60);
    return String(buf);
}

// Formate une borne horaire en "Xh" ou "Xh30" — uniquement les demi-heures exactes (0.5, 1.5…)
// ex: 6.5 → "6h30", 18.0 → "18h". Une valeur comme 6.33 affichera "6h" (pas de quart d'heure).
static String fmtH(float h) {
    int m = (int)(h * 60.0f + 0.5f) % 60;
    return String((int)h) + (m > 0 ? "h30" : "h");
}

// Formate le temps écoulé depuis le démarrage  ex: "2j14h" ou "3h45m"
static String uptimeStr() {
    uint32_t s = millis() / 1000;
    uint32_t d = s / 86400; s %= 86400;
    uint32_t h = s / 3600;  s %= 3600;
    uint32_t m = s / 60;
    char buf[12];
    if (d > 0) snprintf(buf, sizeof(buf), "%uj%uh%um", (unsigned)d, (unsigned)h, (unsigned)m);
    else       snprintf(buf, sizeof(buf), "%uh%02um",  (unsigned)h, (unsigned)m);
    return String(buf);
}

// Affiche 4 lignes aux positions y = 0, 16, 32, 48
static void show4(const String &l1, const String &l2,
                  const String &l3, const String &l4) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,  0); display.print(l1);
    display.setCursor(0, 16); display.print(l2);
    display.setCursor(0, 32); display.print(l3);
    display.setCursor(0, 48); display.print(l4);
    display.display();
}

// =========================================================================
// Gestion du bouton — appelée à chaque itération de loop()
// =========================================================================
void handleOledButton() {
    if (!isPcfReady()) return; // PCF absent : bouton inutilisable
    int      btnNow = pcf_read(PIN_BP_OLED); // LOW = appuyé
    uint32_t now    = millis();

    // Front descendant : début d'appui
    if (btnNow == LOW && lastBtnState == HIGH) {
        btnPressStart = now;
        btnHandled    = false;
    }

    // Écran en veille : le relâchement du bouton réveille sans changer de page
    if (oledAsleep) {
        if (btnNow == HIGH && lastBtnState == LOW) {
            oledWake();
            lastActivity = now;
            btnHandled   = true; // Appui consommé par le réveil
        }
        lastBtnState = btnNow;
        return;
    }

    // Bouton maintenu : détection appui long → retour page 0
    if (btnNow == LOW && !btnHandled) {
        if (now - btnPressStart >= LONG_PRESS_MS) {
            currentPage    = 0;
            lastPageChange = now;
            lastActivity   = now;
            btnHandled     = true;
        }
    }

    // Front montant : fin d'appui court → page suivante
    if (btnNow == HIGH && lastBtnState == LOW && !btnHandled) {
        uint32_t dur = now - btnPressStart;
        if (dur >= DEBOUNCE_MS && dur < LONG_PRESS_MS) {
            currentPage    = (currentPage + 1) % PAGE_COUNT;
            lastPageChange = now;
            lastActivity   = now;
        }
    }

    lastBtnState = btnNow;

    // Retour automatique à la page principale après inactivité
    if (currentPage != 0 && (now - lastPageChange >= PAGE_TIMEOUT_MS)) {
        currentPage = 0;
    }
}

// =========================================================================
// Rendu des pages
// =========================================================================

// --- Page 0 : Vue principale — source temps, heure, air, eau ---
static void renderPage0(const String &source, const String &time,
                        float tAir, float hAir, float tEau) {
    String air = "Air:" + String(tAir, 1) + "C " + String(hAir, 0) + "%";
    String eau = (tEau <= -100.0f) ? "Eau:ERREUR SONDE"
                                   : "Eau:" + String(tEau, 1) + "C";
    show4(source, time, air, eau);
}

// --- Page 1 : Pompe & filtration ---
static void renderPage1(float tEau) {
    String etat = isPumpRunning() ? "ON" : "OFF";
    String mode = isBoostActive() ? "BOOST" : getModeString();
    String l1   = "Pompe:" + etat + " [" + mode + "]";

    float  done   = getPumpingDoneToday();
    float  target = calculateTargetHours(tEau);
    String l2 = "Fait:" + fmtHM(done) + " / " + fmtHM(target);

    String l3;
    if (isAntiGelActif())       l3 = "Mode: ANTI-GEL";
    else if (isCaniculeActif()) l3 = "Mode: CANICULE";
    else l3 = "Plage:" + fmtH(getFilterStartHour()) + "-" + fmtH(getFilterEndHour());

    String l4 = "Sessions: " + String(getDailySessionCount());
    show4(l1, l2, l3, l4);
}

// --- Page 2 : Sécurités & uptime ---
static void renderPage2() {
    String l1 = String("Niveau:") + (isWaterLevelOk()      ? "OK" : "MANQUE !");
    String l2 = String("Moteur:") + (!isMotorFaultActive() ? "OK"
                                    : (isMotorFaultLatched() ? "VERROU !" : "DEFAUT !"));
    String l3 = String("Secure:") + (isSystemSafe()         ? "OK" : "ALERTE !");
    String l4 = "Uptime:" + uptimeStr();
    show4(l1, l2, l3, l4);
}

// --- Page 3 : Réseau & synchronisation ---
static void renderPage3() {
    bool   wifi = (WiFi.status() == WL_CONNECTED);
    String l1   = wifi ? "WiFi: Connecte" : "WiFi: Hors ligne";
    String l2   = wifi ? WiFi.localIP().toString() : "Pas d'IP";
    String l3   = wifi ? ("Signal:" + String(WiFi.RSSI()) + "dBm") : "---";

    String l4;
    if (gps.time.isValid() && gps.satellites.value() >= 4 && gps.time.age() < 5000)
        l4 = "Sync:GPS(" + String(gps.satellites.value()) + " sat)";
    else if (wifi)
        l4 = "Sync: NTP/WiFi";
    else
        l4 = "Sync: aucune";

    show4(l1, l2, l3, l4);
}

// --- Page 4 : Bilan journalier ---
static void renderPage4(float tEau, const String &time) {
    // Mise à jour des températures min/max du jour
    if (tEau > -100.0f) {
        if (tEau < dailyTempMin) dailyTempMin = tEau;
        if (tEau > dailyTempMax) dailyTempMax = tEau;
    }

    // Extrait "JJ/MM/AAAA" depuis "JJ/MM/AAAA HH:MM:SS" (ou affiche tel quel)
    String dateOnly = (time.length() >= 10) ? time.substring(0, 10) : time;
    float  done     = getPumpingDoneToday();
    float  target   = calculateTargetHours(tEau);

    String l1 = "Bilan " + dateOnly;
    String l2 = "Filtr:" + fmtHM(done) + "/" + fmtHM(target);
    String l3 = (dailyTempMin <  90.0f) ? "T.min:" + String(dailyTempMin, 1) + "C" : "T.min: --";
    String l4 = (dailyTempMax > -90.0f) ? "T.max:" + String(dailyTempMax, 1) + "C" : "T.max: --";

    show4(l1, l2, l3, l4);
}

// --- Page alerte : s'impose sur toutes les pages, clignote ---
static void renderAlert() {
    uint32_t now = millis();
    if (now - lastAlertBlink >= 600) {
        lastAlertBlink = now;
        alertVisible   = !alertVisible;
    }

    if (!alertVisible) {
        display.clearDisplay();
        display.display();
        return;
    }

    String cause1 = !isWaterLevelOk()    ? "Niveau: MANQUE !"  : "";
    String cause2 = isMotorFaultActive() ? (isMotorFaultLatched()
                                            ? "Moteur: VERROU !"
                                            : "Moteur: DEFAUT !") : "";
    String msg    = getSafetyStatusMessage();

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(24,  0); display.print("** ALERTE **");
    display.setCursor( 0, 16); display.print(msg.substring(0, 21));
    if (cause1.length()) { display.setCursor(0, 32); display.print(cause1); }
    if (cause2.length()) { display.setCursor(0, 48); display.print(cause2); }
    display.display();
}

// =========================================================================
// API publique
// =========================================================================

void initOLED() {
    lastActivity = millis(); // Démarre le compteur de veille dès l'init
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        logSystem(WARNING, "OLED", "OLED non detecte");
        return;
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("OLED OK");
    display.display();
}

void displayMessage(const String &message) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(message);
    display.display();
}

void displayTwoLines(const String &l1, const String &l2) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,  0); display.println(l1);
    display.setCursor(0, 16); display.println(l2);
    display.display();
}

void displayThreeLines(const String &l1, const String &l2, const String &l3) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,  0); display.print(l1);
    display.setCursor(0, 12); display.print(l2);
    display.setCursor(0, 24); display.print(l3);
    display.display();
}

void displayFourLines(const String &l1, const String &l2,
                      const String &l3, const String &l4) {
    show4(l1, l2, l3, l4);
}

void resetDailyOledStats() {
    dailyTempMin =  99.0f;
    dailyTempMax = -99.0f;
}

void updateOledDisplay(String source, String time,
                       float tAir, float hAir, float tEau) {
    uint32_t now = millis();

    // Alerte critique : réveille l'écran et s'impose (on ne dort pas sur une alerte)
    if (!isWaterLevelOk() || isMotorFaultActive() || !isSystemSafe()) {
        oledWake();
        lastActivity = now; // Maintient l'écran allumé tant que l'alerte est active
        renderAlert();
        return;
    }

    // Mise en veille après inactivité (hors alerte)
    if (!oledAsleep && (now - lastActivity >= SLEEP_TIMEOUT_MS)) {
        oledSleep();
        return;
    }
    if (oledAsleep) return; // En veille : rien à afficher

    switch (currentPage) {
        case 0: renderPage0(source, time, tAir, hAir, tEau); break;
        case 1: renderPage1(tEau);                           break;
        case 2: renderPage2();                               break;
        case 3: renderPage3();                               break;
        case 4: renderPage4(tEau, time);                     break;
        default:
            currentPage = 0;
            renderPage0(source, time, tAir, hAir, tEau);
            break;
    }
}
