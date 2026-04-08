#ifndef WATER_TEMP_MANAGER_H
#define WATER_TEMP_MANAGER_H

/**
 * Calcule le nombre d'heures de filtration nécessaires selon la température.
 * Inclut les modes hiver, standard, canicule et anti-gel.
 * @param temp Température de l'eau (°C)
 * @return Heures cible (float) — 0.0 indique mode gel/canicule continu
 */
float calculateTargetHours(float temp);

/**
 * Retourne true si le mode anti-gel est actif (T° < 4°C).
 * En dessous de 2°C, la pompe s'active même en MODE_OFF.
 */
bool isAntiGelActif();

/**
 * Retourne true si le mode canicule est actif (T° > 28°C).
 * La filtration devient continue.
 */
bool isCaniculeActif();

/**
 * Retourne la plage horaire de début de filtration selon le mode actif.
 */
float getFilterStartHour();

/**
 * Retourne la plage horaire de fin de filtration selon le mode actif.
 */
float getFilterEndHour();

/**
 * Retourne les valeurs configurées pour le mode standard (indépendant du mode actif).
 * Utilisé par l'interface web pour afficher/éditer la plage.
 */
float getConfiguredStartHour();
float getConfiguredEndHour();

/**
 * Charge la plage horaire depuis la NVS — à appeler une fois dans setup().
 */
void loadFilterSchedule();

/**
 * Applique et sauvegarde une nouvelle plage horaire en NVS.
 * Validation : 0 <= start <= 23.5, 0.5 <= end <= 24, start < end, multiple de 0.5h.
 * @return true si valide et sauvegardé, false si rejeté.
 */
bool setFilterSchedule(float start, float end);

#endif
