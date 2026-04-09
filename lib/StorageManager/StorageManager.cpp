#include "includes.h"

// Objet global pour les petites variables (NVS)
Preferences prefs;

// --- FONCTION INTERNE TECHNIQUE ---
// Ne pas déclarer dans le .h, elle ne sert qu'ici
void _writeToFS(const char *path, String msg)
{
    // Horodatage via getGPSPitch() : retourne une chaîne "JJ/MM/AAAA HH:MM:SS" (GPS ou NTP)
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

// --- MAINTENANCE DES JOURNAUX ---

// Tronque /systeme.log aux maxLines dernières lignes.
// Algorithme : lecture pour compter, seek pour sauter les premières, copie dans /sys_tmp.log,
// suppression de l'original, renommage du temporaire. Aucune allocation dynamique.
void trimSystemLog(int maxLines)
{
    const char* path    = "/systeme.log";
    const char* tmpPath = "/sys_tmp.log";

    File f = LittleFS.open(path, FILE_READ);
    if (!f) return;

    // 1. Compter le nombre total de lignes
    int totalLines = 0;
    while (f.available()) {
        if (f.read() == '\n') totalLines++;
    }

    if (totalLines <= maxLines) {
        f.close();
        return; // Fichier suffisamment court, rien à faire
    }

    // 2. Avancer jusqu'à la première ligne à conserver
    int toSkip = totalLines - maxLines;
    f.seek(0);
    int skipped = 0;
    while (f.available() && skipped < toSkip) {
        if (f.read() == '\n') skipped++;
    }

    // 3. Copier le reste dans un fichier temporaire (buffer 256 B)
    File tmp = LittleFS.open(tmpPath, FILE_WRITE);
    if (!tmp) { f.close(); return; }

    uint8_t buf[256];
    while (f.available()) {
        size_t n = f.read(buf, sizeof(buf));
        tmp.write(buf, n);
    }
    f.close();
    tmp.close();

    // 4. Remplacer l'original par le fichier tronqué
    LittleFS.remove(path);
    LittleFS.rename(tmpPath, path);

    logSystem(INFO, "FS", "systeme.log tronqué (" + String(totalLines) + " → " + String(maxLines) + " lignes)");
}

// Supprime les fichiers /logs/*.csv plus vieux que keepMonths mois.
// Format attendu des noms : prefix_MMYY.csv (ex: sessions_0426.csv)
void purgeOldLogs(int keepMonths)
{
    struct tm t;
    if (!getLocalTime(&t) || t.tm_year < 124) return; // Date NTP non disponible

    int currentMM = t.tm_mon + 1;       // 1–12
    int currentYY = t.tm_year - 100;    // ex: 2026 → 26

    File root = LittleFS.open("/logs");
    if (!root || !root.isDirectory()) { root.close(); return; }

    // Collecter les noms à supprimer sans modifier le répertoire pendant l'itération
    const int MAX_TO_DELETE = 20;
    String toDelete[MAX_TO_DELETE];
    int deleteCount = 0;

    File entry = root.openNextFile();
    while (entry && deleteCount < MAX_TO_DELETE) {
        String name = String(entry.name()); // Juste le nom, sans chemin
        entry.close();

        // Chercher le tag MMYY : 4 caractères avant le dernier '.'
        int dotIdx       = name.lastIndexOf('.');
        int underscoreIdx = name.lastIndexOf('_');
        if (underscoreIdx >= 0 && dotIdx == underscoreIdx + 5) {
            // "MMYY" = 4 chars entre '_' et '.'
            String tag = name.substring(underscoreIdx + 1, dotIdx);
            int mm = tag.substring(0, 2).toInt();
            int yy = tag.substring(2, 4).toInt();
            if (mm >= 1 && mm <= 12 && yy >= 0) {
                int ageMonths = (currentYY - yy) * 12 + (currentMM - mm);
                if (ageMonths > keepMonths) {
                    toDelete[deleteCount++] = "/logs/" + name;
                }
            }
        }
        entry = root.openNextFile();
    }
    root.close();

    // Supprimer les fichiers repérés
    for (int i = 0; i < deleteCount; i++) {
        if (LittleFS.remove(toDelete[i])) {
            logSystem(INFO, "FS", "Purge : " + toDelete[i]);
        }
    }
}