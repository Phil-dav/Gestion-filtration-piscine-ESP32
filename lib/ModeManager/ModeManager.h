 #ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <Arduino.h>

// Définition des 3 états possibles de l'interrupteur
enum OperationMode { 
    MODE_OFF,   // Position centrale
    MODE_MANU,  // Position MANU 5 cpf8574
    MODE_AUTO   // Position AUTO 6 cpf8574
};

/**
 * @brief Lit l'état physique de l'interrupteur via le PCF8574
 * @return OperationMode L'état actuel (OFF, MANU ou AUTO)
 */
OperationMode getCurrentMode();

/**
 * @brief Retourne le nom du mode en texte depuis une valeur deja lue
 */
String getModeString(OperationMode m);

/**
 * @brief Retourne le nom du mode en texte (lit le PCF8574)
 */
String getModeString();

#endif