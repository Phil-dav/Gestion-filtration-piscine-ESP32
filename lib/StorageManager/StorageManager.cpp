#include "includes.h"

// Objet global pour les petites variables (NVS)
Preferences prefs;

// --- FONCTION INTERNE TECHNIQUE ---
// Ne pas déclarer dans le .h, elle ne sert qu'ici
void _writeToFS(const char *path, String msg)
{
    // Appel de ta fonction GPS pour dater l'entrée
    String timestamp = getGPSPitch();

    File file = LittleFS.open(path, FILE_APPEND);
    if (!file)
    {
        logSystem(CRITICAL, "FS", "Impossible d'ouvrir " + String(path));
        return;
    }
    file.printf("[%s] %s\n", timestamp.c_str(), msg.c_str());
    file.close();
}
void readFromFS(const char *path)
{
    // 1. Ouvrir le fichier en mode LECTURE (FILE_READ)
    File file = LittleFS.open(path, FILE_READ);

    if (!file)
    {
        logSystem(CRITICAL, "FS", "Impossible de lire " + String(path));
        return;
    }

    logSystem(INFO, "FS", "--- CONTENU DE " + String(path) + " ---");

    String line = "";
    while (file.available())
    {
        char c = file.read();
        if (c == '\n') { logSystem(INFO, "FS", line); line = ""; }
        else line += c;
    }
    if (line.length() > 0) logSystem(INFO, "FS", line);

    logSystem(INFO, "FS", "--- FIN DU FICHIER ---");

    // 3. Toujours refermer le fichier
    file.close();
}
// --- JOUR COURANT (anti-faux reset au reboot) ---
void saveLastDay(int day)
{
    prefs.begin("piscine", false);
    prefs.putInt("lastDay", day);
    prefs.end();
}

int loadLastDay()
{
    prefs.begin("piscine", true);
    int val = prefs.getInt("lastDay", -1); // -1 = NVS vierge (premier flash)
    prefs.end();
    return val;
}

// --- LOGIQUE D'INITIALISATION ---
void initStorage()
{
    // Montage du système de fichiers
    if (!LittleFS.begin(true))
    {
        logSystem(CRITICAL, "FS", "Echec montage LittleFS");
    }

    // Test de la zone Preferences
    prefs.begin("piscine", false);
    prefs.end();

    logSystem(INFO, "FS", "NVS et LittleFS operationnels");
}

// --- COMPTEUR DE FILTRATION (double créneau, résilience coupure secteur) ---
// Principe : écriture alternée dans deux créneaux (fait0 / fait1).
// Le créneau inactif conserve toujours la valeur précédente.
// En cas de coupure pendant l'écriture, l'indicateur n'est pas mis à jour
// → le créneau précédent valide est utilisé au redémarrage.
void saveFiltrationProgress(float hours)
{
    prefs.begin("piscine", false);
    int nextSlot = 1 - prefs.getInt("activeSlot", 0); // bascule 0→1 ou 1→0
    if (nextSlot == 0) prefs.putFloat("fait0", hours);
    else               prefs.putFloat("fait1", hours);
    prefs.putInt("activeSlot", nextSlot);              // validé en dernier
    prefs.end();
}

float loadFiltrationProgress()
{
    prefs.begin("piscine", true);
    int   slot = prefs.getInt("activeSlot", -1); // -1 = jamais initialisé (première fois)
    float val;
    if (slot == -1) {
        // Migration depuis l'ancienne clé unique "faitToday"
        val = prefs.getFloat("faitToday", 0.0);
    } else {
        val = (slot == 0) ? prefs.getFloat("fait0", 0.0)
                          : prefs.getFloat("fait1", 0.0);
    }
    prefs.end();
    return val;
}

// --- ENREGISTREMENT DES ÉVÉNEMENTS ---
void logHistory(String message) { _writeToFS("/historique.txt", message); }
void logToFile(String message) { _writeToFS("/systeme.log", message); }
void logAlert(String message) { _writeToFS("/alertes.log", message); }