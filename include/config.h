#pragma once

//#define DEBUG_ENABLED // --- DEBUG : Mettre en commentaire pour désactiver tous les messages série ---

#define I2C_SDA 21
#define I2C_SCL 22

// Adresse I2C du PCF8574 (ajuste selon A0/A1/A2)
#define PCF8574_ADDRESS 0x21
#define DS18B20_PIN 4 // Capteur de température DS18B20 connecté à la broche GPIO4 (D4) de l'ESP32

#define PIN_LEVEL_SENSOR 34 // Détecteur de niveau d'eau (GPIO34)

#define GPS_RX_PIN 16
#define GPS_TX_PIN 17

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_ADDRESS 0x3C

#define TZ_EUROPE_PARIS "CET-1CEST,M3.5.0/2,M10.5.0/3"

// --- SORTIES (0 à 3) ---
// Note : LOW = Activé (Relais collé / LED allumée)
#define PIN_RELAY_POMPE 0              // Pilotage Pompe Filtration (via In1 du module relais)
#define PIN_RELAY_TEMP_EAU 1           // Libre — réservé usage futur alarme température
#define PIN_RELAY_FEEDBACK_POMPE 2     // Relais miroir pompe — commandé en parallèle du relais 0
#define PIN_RELAY_DEFAUT_SYSTEME 3     // Relais signalisation défaut système (niveau, moteur, sécurité)

// --- FEEDBACK HARDWARE POMPE ---
// Contact NO du relais miroir (PCF P2) → GPIO33 ESP32
// Câblage : une borne NO → 3.3V, autre borne → GPIO33
// pull-down interne activé dans initPumpManager()
#define PIN_FEEDBACK_POMPE 33

// --- ENTRÉES (4 à 7) ---
// Note : Grâce aux résistances R2-R5, l'état est HIGH par défaut, LOW si activé
#define PIN_BP_OLED 4       // Bouton SW1 (Ecran 2 LCD)
#define PIN_MODE_MANU 5     // Position MANU de l'interrupteur 5
#define PIN_MODE_AUTO 6     // Position AUTO de l'interrupteur 6
#define PIN_DEFAUT_RELAIS 7 // Entrée J3 (Défaut moteur / commande relais)

// --- SURVEILLANCE MÉMOIRE ---
#define HEAP_MIN_SAFE 20000  // Seuil reboot préventif heap (octets libres)