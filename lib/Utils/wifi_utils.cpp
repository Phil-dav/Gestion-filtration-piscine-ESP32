#include "includes.h"
#include "secrets.h"
#include "wifi_utils.h"
#include <Preferences.h>

struct WiFiCredential
{
    const char *ssid;
    const char *password;
};

// Liste des réseaux connus
WiFiCredential knownNetworks[] = {
    {WIFI_SSID_1, WIFI_PASSWORD_1},
    {WIFI_SSID_2, WIFI_PASSWORD_2}
    // Ajoute ici d'autres réseaux si nécessaire
};

// IP fixe souhaitée sur le réseau local
static const IPAddress STATIC_IP(192, 168, 1, 20);
static const IPAddress GATEWAY(192, 168, 1, 1);
static const IPAddress SUBNET(255, 255, 255, 0);
static const IPAddress DNS(192, 168, 1, 1);

// Connexion WiFi + IP fixe appliquée APRES connexion (pile TCP déjà prête)
static bool tryConnect(const char* ssid, const char* password)
{
    WiFi.begin(ssid, password);
    uint8_t retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20)
    {
       delay(500); 
        yield();
        retries++;
    }
    if (WiFi.status() != WL_CONNECTED) return false;
    WiFi.config(STATIC_IP, GATEWAY, SUBNET, DNS);
    delay(200);
    return true;
}

void initWiFiAuto()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    Preferences prefs;
    prefs.begin("wifi", true);
    String lastSSID = prefs.getString("last_ssid", "");
    prefs.end();

    int nbReseaux = sizeof(knownNetworks) / sizeof(knownNetworks[0]);

    // TENTATIVE RAPIDE sur le dernier réseau utilisé (si connu)
    if (lastSSID.length() > 0)
    {
        for (int i = 0; i < nbReseaux; i++)
        {
            if (lastSSID == knownNetworks[i].ssid)
            {
                logSystem(INFO, "WIFI", "Tentative rapide sur : " + lastSSID);
                if (tryConnect(knownNetworks[i].ssid, knownNetworks[i].password))
                {
                    logSystem(INFO, "WIFI", "Connecte a : " + lastSSID);
                    logSystem(INFO, "WIFI", "IP fixe : " + WiFi.localIP().toString());
                    return;
                }
                logSystem(WARNING, "WIFI", "Echec sur : " + lastSSID);
                break;
            }
        }
    }

    // SCAN COMPLET de tous les réseaux connus
    logSystem(INFO, "WIFI", "Scan de tous les reseaux connus...");
    for (int i = 0; i < nbReseaux; i++)
    {
        String ssid = String(knownNetworks[i].ssid);
        logSystem(INFO, "WIFI", "Essai : " + ssid);
        if (tryConnect(knownNetworks[i].ssid, knownNetworks[i].password))
        {
            Preferences prefs;
            prefs.begin("wifi", false);
            prefs.putString("last_ssid", ssid);
            prefs.end();

            logSystem(INFO, "WIFI", "Connecte a : " + ssid);
            logSystem(INFO, "WIFI", "IP fixe : " + WiFi.localIP().toString());
            return;
        }
        logSystem(WARNING, "WIFI", "Echec sur : " + ssid);
        WiFi.disconnect(true);
        delay(100);
    }

    logSystem(WARNING, "WIFI", "Aucun reseau disponible - mode hors ligne");
}
