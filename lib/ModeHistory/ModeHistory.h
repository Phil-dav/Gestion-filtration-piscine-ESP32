#pragma once
#include <Arduino.h>

// Types de segments (doivent correspondre aux couleurs JS)
// 0 = MODE_OFF (gris), 1 = AUTO (bleu), 2 = MANU pompe ON (vert), 3 = MANU pompe OFF (orange)
// 4 = BOOST marche forcée (violet), 5 = BOOST arrêt forcé (rouge)

void   mhStart(uint8_t type, float hourFloat); // Ferme le segment précédent, ouvre un nouveau
void   mhReset();                               // Remet à zéro (changement de jour)
String mhToJSON();                              // Retourne le tableau JSON pour l'interface web
