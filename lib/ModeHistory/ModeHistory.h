#pragma once
#include <Arduino.h>

// Types de segments (doivent correspondre aux couleurs JS dans timeline.js)
// 0 = MODE_OFF (gris)              — attente / inactif
// 1 = AUTO pompe ON (bleu)         — filtration automatique en cours
// 2 = MANU pompe ON (vert)         — forçage manuel marche
// 3 = MANU pompe OFF (orange)      — forçage manuel arrêt
// 4 = BOOST marche forcée (violet) — boost actif
// 5 = BOOST arrêt forcé (rouge)    — boost coupé manuellement

void   mhStart(uint8_t type, float hourFloat); // Ferme le segment précédent, ouvre un nouveau
void   mhReset();                               // Remet à zéro (changement de jour)
String mhToJSON();                              // Retourne le tableau JSON pour l'interface web
