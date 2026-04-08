# ESP32 — Automatisme Piscine Modulaire

Contrôleur de filtration piscine embarqué sur ESP32, avec interface web, journalisation LittleFS, protection gel, et gestion multi-sources de l'heure (GPS + NTP).

---

## Fonctionnalités actuelles

### Gestion de la pompe

- 3 modes physiques via interrupteur 3 positions (PCF8574) : **AUTO / OFF / MANU**
- **Mode AUTO** : calcul automatique des heures de filtration selon la température de l'eau (règle T°/2)
- **Mode MANU** : commande manuelle via bouton web
- **Boost web** : surclasse le mode AUTO temporairement (marche ou arrêt forcé)
- Verrouillage de sécurité 10 secondes au démarrage
- Anti-rebond matériel sur le relais 2 (miroir, GPIO33)

### Protection et sécurité

- **Anti-gel prioritaire** : pompe forcée sous 2°C, même en MODE_OFF
- **Surveillance niveau d'eau** (GPIO34) : arrêt pompe + alarme relais si manque
- **Défaut moteur / disjoncteur** : entrée PCF8574 IN7, verrou avec réarmement web requis
- 4 relais de signalisation via PCF8574 : pompe, défaut T° eau, alerte gel, défaut système

### Modes thermiques automatiques

| Température eau | Mode       | Comportement                        |
|-----------------|------------|-------------------------------------|
| < 2°C           | Anti-gel   | Pompe forcée même en MODE_OFF       |
| 2°C – 4°C       | Gel        | 0 h objectif (pompe arrêtée)        |
| 4°C – 15°C      | Hiver      | Filtration réduite                  |
| 15°C – 28°C     | Normal     | T°/2 heures par jour                |
| > 28°C          | Canicule   | Filtration continue 24h/24          |

### Sources de temps (heure et date)

- **GPS (TinyGPSPlus)** prioritaire si signal valide (≥ 4 satellites, donnée < 5 s)
- **NTP WiFi** en secours (fuseau Europe/Paris avec heure d'été automatique)
- Heure par défaut à 12h si aucune source disponible

### Interface web (ESPAsyncWebServer)

- Dashboard temps réel via **Server-Sent Events (SSE)**
- Commandes pompe (MANU), boost, réarmement défaut moteur
- Configuration de la plage horaire de filtration (sauvegardée en NVS)
- Visualisation **timeline des modes** de la journée (AUTO/MANU/OFF/BOOST)
- Téléchargement des journaux CSV depuis l'interface

### Journalisation (LittleFS)

| Fichier                        | Contenu                                          |
|-------------------------------|--------------------------------------------------|
| `/logs/sessions_AAAAMM.csv`   | Chaque session pompe (début, fin, durée, T° eau) |
| `/logs/daily_AAAAMM.csv`      | Bilan journalier (objectif, fait, nb sessions)   |
| `/logs/alertes_AAAAMM.csv`    | Alertes horodatées (gel, défaut, redémarrage)    |
| `/systeme.log`                | Événements système (reboot, mode, erreurs soft)  |

### Persistence NVS

- Reprise du compteur de filtration après redémarrage
- Dernier jour connu (anti-faux reset au reboot)
- Plage horaire configurable persistante

### Diagnostic au démarrage

- Cause du redémarrage via registres RTC bas niveau (`rtc_get_reset_reason`)  
  → distingue coupure secteur / bouton EN / watchdog / sous-tension
- Affichage du journal `/systeme.log` sur port série au boot
- Reprise de la progression de filtration depuis NVS

### Affichage OLED (SSD1306 128×64)

- Pages cycliques via bouton physique (PCF8574 IN4)
- Informations : heure, source (GPS/WiFi), T° eau, T° air, humidité, niveau, mode, progression filtration, min/max journalières

---

## Architecture des modules

```
Architecture-modulaire-avec-WiFi-et-capteurs-ESP32-1/
├── src/
│   └── main.cpp              ← Boucle principale, logique de décision pompe
├── include/
│   ├── config.h              ← GPIO, adresses I2C, fuseau horaire
│   ├── includes.h            ← En-tête centralisé (toutes les libs)
│   └── secrets.h             ← SSID/Password WiFi (non versionné)
├── lib/
│   ├── DebugManager/         ← Logs série niveaux INFO / WARNING / CRITICAL
│   ├── GPS_manager/          ← Réception NMEA, heure locale GPS
│   ├── LogManager/           ← Journaux CSV sessions/daily/alertes (LittleFS)
│   ├── ModeHistory/          ← Timeline JSON des modes pour l'interface web
│   ├── ModeManager/          ← Lecture interrupteur 3 positions via PCF8574
│   ├── OLED_Manager/         ← Affichage SSD1306 multi-pages
│   ├── PCF8574_Manager/      ← Driver I2C relais (sorties) et entrées logiques
│   ├── PumpManager/          ← Commande relais pompe, compteurs, sessions
│   ├── SafetyManager/        ← Sentinelle sécurité (niveau, défaut moteur)
│   ├── Sensors_Manager/      ← AHT10 (T° air + humidité) non-bloquant
│   ├── StorageManager/       ← NVS (Preferences) + LittleFS
│   ├── Utils/
│   │   ├── time_utils        ← NTP, formatage heure, heure décimale
│   │   ├── web_utils         ← Routes HTTP, SSE, API JSON
│   │   └── wifi_utils        ← Connexion WiFi auto + surveillance 30s
│   ├── WaterLevel/           ← Détecteur niveau d'eau GPIO34
│   ├── WaterTempManager/     ← Calcul heures cible, modes thermiques, plages horaires
│   └── ds18_manager/         ← DS18B20 non-bloquant (requête séparée de la lecture)
├── data/
│   ├── index.html            ← Interface web (dashboard responsive)
│   └── script.js             ← Logique frontend (SSE, timeline, graphes)
└── platformio.ini
```

---

## Câblage (GPIO ESP32)

| Signal              | GPIO ESP32 | Remarque                         |
|---------------------|-----------|----------------------------------|
| I2C SDA             | 21        | Bus PCF8574 + OLED               |
| I2C SCL             | 22        | Bus PCF8574 + OLED               |
| DS18B20             | 4         | Température eau (OneWire)        |
| Niveau eau          | 34        | Entrée seule (pas de pull-up HW) |
| GPS RX (→ ESP32)    | 16        | UART2                            |
| GPS TX (← ESP32)    | 17        | UART2                            |

### PCF8574 (adresse 0x21)

| Broche PCF | Rôle                     | Direction |
|-----------|--------------------------|-----------|
| P0        | Relais pompe filtration  | Sortie    |
| P1        | Relais défaut T° eau     | Sortie    |
| P2        | Relais alerte gel        | Sortie    |
| P3        | Relais défaut système    | Sortie    |
| P4        | Bouton OLED (SW1)        | Entrée    |
| P5        | Interrupteur MANU        | Entrée    |
| P6        | Interrupteur AUTO        | Entrée    |
| P7        | Défaut moteur/disjoncteur| Entrée    |

> Les relais sont actifs à l'état BAS (LOW = relais collé). Les entrées sont HIGH par défaut (résistances de tirage R2–R5).

---

## Dépendances (platformio.ini)

| Bibliothèque                  | Version   |
|-------------------------------|-----------|
| ESPAsyncWebServer (mathieucarbou) | v3.3.23 |
| AsyncTCP (mathieucarbou)      | v3.2.0    |
| Adafruit SSD1306              | ^2.5.7    |
| Adafruit GFX Library          | ^1.11.5   |
| DallasTemperature             | ^3.9.1    |
| OneWire                       | ^2.3.8    |
| Adafruit AHTX0                | ^2.0.5    |
| TinyGPSPlus                   | ^1.0.3    |
| PCF8574 (robtillaart)         | ^0.3.7    |

- Système de fichiers : **LittleFS**
- Framework : **Arduino** sur ESP32

---

## Configuration

### secrets.h (à créer, non versionné)
```cpp
#define WIFI_SSID "votre_réseau"
#define WIFI_PASSWORD "votre_mot_de_passe"
```

### Debug (config.h)
```cpp
#define DEBUG_ENABLED  // Commenter pour désactiver tous les messages série
```

Ou via `platformio.ini` :
```ini
build_flags = -I include  ; -D DEBUG_ENABLED  ← décommenter pour activer
```

---

## Logique de la boucle principale

```
loop() — priorités d'exécution :
  1. Bouton OLED + GPS (chaque cycle — temps réel)
  2. Lecture interrupteur + décision mode (chaque cycle)
  3. Logique anti-gel et calcul demande pompe (chaque cycle après 10s)
  4. Application commande pompe via SafetyManager + PCF8574
  5. DS18B20 — requête toutes les 5 s, lecture 800 ms après
  6. AHT10 — mise à jour toutes les 5 s
  7. Affichage OLED + log système — toutes les 1 s
  8. Vérification WiFi — toutes les 30 s
```

---

## Projets futurs envisagés

- **Protection pompe par courant** : capteur SCT-013 ou ACS712 sur l'alimentation 220 V du moteur pour détecter les anomalies (blocage, surconsommation)
- **Journalisation CSV avancée** : bilans mensuels téléchargeables, alertes horodatées enrichies
- **Accès extérieur** : tunnel Raspberry Pi (ngrok/Cloudflare) — la box Orange bloque les ports entrants
- **Distinction reset / coupure courant** : solution hardware (GPIO + condensateur) pour différencier les deux cas, car `rtc_get_reset_reason()` retourne le même code pour les deux CPU dans les deux situations

---

Développé par **Philippe (Phil-dav)** — ESP32 + PlatformIO + VS Code
