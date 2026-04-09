# 🏊 Documentation Technique — Automate Piscine ESP32

> Généré le 2026-04-09 par le skill `generate_pool_docs`

---

## 📋 Introduction

Ce document décrit l'ensemble du système domotique de gestion de piscine développé sur microcontrôleur **ESP32**.

Le projet est né d'une volonté d'automatiser intelligemment la filtration d'une piscine privée, en allant au-delà d'une simple minuterie. L'automate adapte la durée de filtration journalière à la température réelle de l'eau, protège l'installation contre le gel et la canicule, surveille le bon fonctionnement de la pompe par retour hardware, et conserve un historique complet des événements.

L'ensemble est piloté par un ESP32 connecté au réseau WiFi local, qui expose une **interface web embarquée** accessible depuis n'importe quel appareil de la maison. Aucun cloud, aucune dépendance extérieure : le système fonctionne en autonomie complète.

**Points forts de l'architecture :**

- Logique temps-réel **entièrement non-bloquante** — aucun `delay()` dans la boucle principale
- Architecture **modulaire** en managers indépendants (15 modules dans `lib/`)
- **Double source de temps** : GPS (prioritaire) ou NTP via WiFi
- **Persistance multi-niveaux** : NVS flash (compteurs), LittleFS (journaux et CSV mensuels)
- **Sécurité intégrée** : anti-gel prioritaire absolu, feedback hardware de la pompe, verrou défaut moteur

---

## 1. 🔭 Vue d'ensemble

Automate de gestion de filtration de piscine basé sur un **ESP32** (framework Arduino / PlatformIO).
Le système pilote une pompe de filtration via un relais, surveille la température de l'eau, le niveau d'eau, l'environnement (air), et expose une interface web locale.

**Fonctionnalités principales :**

- Calcul automatique de la durée de filtration journalière selon la température de l'eau (T°/2)
- Trois modes de fonctionnement : AUTO / MANU / OFF (interrupteur physique)
- Protection anti-gel (pompe forcée si T° eau < 2°C)
- Mode canicule (filtration continue si T° eau > 28°C)
- Feedback hardware de la pompe via relais miroir + GPIO33 (détection claquements et fil rompu)
- Horodatage double source : GPS (prioritaire, ≥ 4 satellites) ou NTP/WiFi
- Journalisation multi-niveaux : NVS, LittleFS (`/systeme.log`, CSV mensuels `/logs/`)
- Interface web asynchrone (ESPAsyncWebServer)
- Affichage OLED 128×64

---

## 2. 🔧 Matériel & Configuration PlatformIO

| Paramètre            | Valeur              |
|----------------------|---------------------|
| Carte                | ESP32 Dev Module    |
| Framework            | Arduino (PlatformIO)|
| Vitesse monitor      | 115 200 baud        |
| Vitesse upload       | 921 600 baud        |
| Système de fichiers  | LittleFS            |
| Mode LDF             | `deep+`             |

**Bibliothèques externes :**

| Bibliothèque                           | Usage                                       |
|----------------------------------------|---------------------------------------------|
| mathieucarbou/ESPAsyncWebServer v3.3.23 | Serveur web asynchrone                     |
| mathieucarbou/AsyncTCP v3.2.0          | TCP asynchrone (base ESPAsyncWebServer)     |
| Adafruit SSD1306 ^2.5.7               | Écran OLED                                  |
| Adafruit GFX Library ^1.11.5          | Graphiques OLED                             |
| milesburton/DallasTemperature ^3.9.1  | Sonde DS18B20                               |
| paulstoffregen/OneWire ^2.3.8         | Bus OneWire (DS18B20)                       |
| Adafruit AHTX0 ^2.0.5                 | Capteur température / humidité air          |
| mikalhart/TinyGPSPlus ^1.0.3          | Module GPS (NMEA)                           |
| robtillaart/PCF8574 ^0.3.7            | Expandeur I/O I2C (relais + entrées)        |

---

## 3. 📌 Carte des GPIO

### GPIO directs ESP32

| GPIO    | Direction         | Rôle                                                              |
|---------|-------------------|-------------------------------------------------------------------|
| GPIO4   | Entrée/Sortie     | Bus OneWire — Sonde DS18B20 (température eau)                     |
| GPIO16  | Entrée (RX)       | UART GPS — Réception NMEA (GPS_RX_PIN)                            |
| GPIO17  | Sortie (TX)       | UART GPS — Émission (GPS_TX_PIN)                                  |
| GPIO21  | I2C SDA           | Bus I2C — SDA (OLED, PCF8574, AHT10)                             |
| GPIO22  | I2C SCL           | Bus I2C — SCL (OLED, PCF8574, AHT10)                             |
| GPIO33  | Entrée (pull-down)| Feedback hardware pompe — Contact NO du relais miroir PCF P2      |
| GPIO34  | Entrée            | Détecteur de niveau d'eau (lecture analogique/digitale)           |

### PCF8574 — Expandeur I/O I2C (adresse 0x21)

> Convention : **LOW = activé** (relais collé / entrée active)
> Entrées P4–P7 : résistances pull-up R2–R5, état HIGH par défaut

| Broche PCF | Nom define                  | Direction | Rôle                                              |
|------------|-----------------------------|-----------|---------------------------------------------------|
| P0         | `PIN_RELAY_POMPE`           | Sortie    | Relais pompe filtration (In1 module relais)        |
| P1         | `PIN_RELAY_TEMP_EAU`        | Sortie    | Libre — réservé alarme température future          |
| P2         | `PIN_RELAY_FEEDBACK_POMPE`  | Sortie    | Relais miroir pompe (commandé en parallèle de P0)  |
| P3         | `PIN_RELAY_DEFAUT_SYSTEME`  | Sortie    | Relais signalisation défaut système                |
| P4         | `PIN_BP_OLED`               | Entrée    | Bouton SW1 — navigation écran OLED                 |
| P5         | `PIN_MODE_MANU`             | Entrée    | Interrupteur position MANU                         |
| P6         | `PIN_MODE_AUTO`             | Entrée    | Interrupteur position AUTO                         |
| P7         | `PIN_DEFAUT_RELAIS`         | Entrée    | Entrée J3 — défaut moteur / disjoncteur            |

### Adresses I2C

| Périphérique               | Adresse            |
|----------------------------|--------------------|
| PCF8574 (expandeur relais) | 0x21               |
| OLED SSD1306 128×64        | 0x3C               |
| AHT10 (temp/humidité air)  | Auto (lib Adafruit)|

---

## 4. 🏗️ Architecture logicielle

Le projet adopte une **architecture modulaire** : chaque fonctionnalité est encapsulée dans un manager indépendant dans `lib/`.

| Module                   | Fichier                   | Rôle                                                                                          |
|--------------------------|---------------------------|-----------------------------------------------------------------------------------------------|
| `PumpManager`            | `lib/PumpManager/`        | Pilote le relais pompe, cumule le temps de filtration, gère le feedback hardware (GPIO33), détecte claquements et fil rompu |
| `WaterTempManager`       | `lib/WaterTempManager/`   | Calcule les heures cibles (T°/2), gère modes GEL/CANICULE, plage horaire configurable en NVS |
| `SafetyManager`          | `lib/SafetyManager/`      | Vérifie niveau d'eau et défaut moteur, verrou latché (réarmement requis)                      |
| `ModeManager`            | `lib/ModeManager/`        | Lit l'interrupteur physique (OFF/MANU/AUTO) via PCF8574, expose `OperationMode`               |
| `StorageManager`         | `lib/StorageManager/`     | Persistance NVS (progression filtration, dernier jour) + LittleFS (logs, CSV) + maintenance  |
| `LogManager`             | `lib/LogManager/`         | Journalisation multi-niveaux : `logSystem()`, `logToFile()`, `logSession()`, `logAlerte()`   |
| `ModeHistory`            | `lib/ModeHistory/`        | Timeline des changements de mode pour l'interface web (mhStart, mhReset)                     |
| `ds18_manager`           | `lib/ds18_manager/`       | Lecture non-bloquante du DS18B20 (demande/lecture découplées, cache 800 ms)                  |
| `Sensors_Manager`        | `lib/Sensors_Manager/`    | Lecture AHT10 (température et humidité air), mise en cache toutes les 5 s                    |
| `WaterLevel`             | `lib/WaterLevel/`         | Lecture du détecteur de niveau d'eau (GPIO34)                                                 |
| `GPS_Manager`            | `lib/GPS_manager/`        | Décode les trames NMEA via TinyGPSPlus, expose heure locale GPS                              |
| `PCF8574_Manager`        | `lib/PCF8574_Manager/`    | Driver bas niveau PCF8574 (init, setRelay, lecture entrées)                                   |
| `OLED_Manager`           | `lib/OLED_Manager/`       | Affichage multi-pages OLED, navigation bouton, stats bilan journalier                         |
| `DebugManager`           | `lib/DebugManager/`       | Active/désactive les traces série (flag NVS `DEBUG_ENABLED`)                                  |
| `Utils` (wifi, web, time)| `lib/Utils/`              | Connexion WiFi auto, serveur web asynchrone, synchronisation NTP, formatage heure             |

---

## 5. 🔁 Logique de Filtration

### Formule de base

```
Durée cible (h) = Température eau (°C) ÷ 2
```

Exemple : eau à 26°C → filtration journalière cible = **13 heures**.

### Modes thermiques

| Mode          | Condition          | Comportement                                                        |
|---------------|--------------------|---------------------------------------------------------------------|
| **Anti-gel**  | T° eau < 2°C       | Pompe forcée ON quel que soit le mode physique (même MODE_OFF)      |
| **Gel**       | T° eau < 4°C       | Plage horaire étendue (filtration maximisée)                        |
| **Normal**    | 4°C ≤ T° ≤ 28°C   | Formule T°/2, plage horaire configurable                            |
| **Canicule**  | T° eau > 28°C      | Filtration continue (pompe ON en permanence dans la plage)          |

### Machine à états — Mode AUTO

```
[Démarrage]
      │
      ▼ (verrouillage sécurité 10 s)
[Lecture T° eau + heure]
      │
      ├─ T° < 2°C ──────────────────────► [POMPE FORCÉE ON — anti-gel]
      │
      ├─ Boost web actif ───────────────► [POMPE = isBoostForceOn()]
      │
      ├─ Dans plage horaire ET pumpingDoneToday < targetHours ──► [POMPE ON]
      │
      └─ Sinon ─────────────────────────► [POMPE OFF]
```

### Mode MANU

La pompe suit l'état booléen de la **demande web** (`setPumpRequest` / `getPumpRequest`).

### Mode OFF

Arrêt total volontaire. Seul l'anti-gel (T° < 2°C) peut forcer la pompe.

### Reset journalier (minuit)

- Remise à zéro de `pumpingDoneToday`
- Sauvegarde en NVS du nouveau jour
- Bilan journalier logué (`logDaily`)
- Tronquage de `/systeme.log` à 200 lignes
- Purge des CSV `/logs/` vieux de plus de 2 mois
- Reset des statistiques OLED (T° min/max)
- Reset de la timeline des modes (`mhReset`)

---

## 6. ⚙️ Fonctions clés

| Fonction                          | Module           | Rôle                                                    |
|-----------------------------------|------------------|---------------------------------------------------------|
| `calculateTargetHours(float temp)`| WaterTempManager | Retourne les heures de filtration cibles selon T°        |
| `updatePumpSystem()`              | PumpManager      | Sentinelle : vérifie sécurités avant d'agir sur le relais|
| `setPumpRequest(bool)`            | PumpManager      | Demande logique d'activation pompe                      |
| `isSystemSafe()`                  | SafetyManager    | Vérifie niveau d'eau + défaut moteur                    |
| `getCurrentMode()`                | ModeManager      | Lit l'interrupteur physique (PCF8574 P5/P6)             |
| `loadFiltrationProgress()`        | StorageManager   | Reprend la progression en NVS après un reboot           |
| `trimSystemLog(200)`              | StorageManager   | Garde les 200 dernières lignes de `/systeme.log`        |
| `purgeOldLogs(2)`                 | StorageManager   | Supprime les CSV plus vieux de 2 mois                   |
| `logSystem(level, tag, msg)`      | LogManager       | Log multi-niveaux (INFO / WARNING / CRITICAL)           |
| `requestDS18Temperatures()`       | ds18_manager     | Déclenche la conversion température (non-bloquant)      |
| `updateDS18Cache()`               | ds18_manager     | Lit le résultat 800 ms après la demande                 |
| `isFeedbackFault()`               | PumpManager      | True si fil GPIO33 rompu (mode dégradé)                 |
| `isPumpBlocked()`                 | PumpManager      | True si pompe bloquée après claquement détecté          |

---

## 7. 🛡️ Règles de sécurité

1. **Aucun `delay()` dans la boucle principale** — tout est piloté par `millis()`.
2. **Verrouillage 10 secondes au démarrage** — aucune commande pompe pendant les 10 premières secondes.
3. **Anti-gel prioritaire absolu** — actif même en MODE_OFF, même si PCF8574 non répondu.
4. **Feedback hardware pompe** :
   - Relais miroir P2 → GPIO33 (pull-down interne)
   - Mismatch > 30 s → CRITICAL `FEEDBACK_ROMPU` → mode dégradé (pompe non bloquée)
   - ≥ 5 transitions en 10 s → CRITICAL `CLAQUEMENT` → blocage pompe 5 minutes
5. **Défaut moteur latché** — `SafetyManager` verrou qui nécessite un réarmement manuel.
6. **Double source de temps** — GPS prioritaire (≥ 4 satellites, signal < 5 s), NTP en secours.
7. **Rejet de l'epoch 1970** — filtre `tm_year >= 124` sur toutes les lectures `getLocalTime()`.

---

## 8. 📝 Décisions techniques récentes (extrait journal)

### 2026-04-09 — Feedback hardware pompe (relais miroir + GPIO33)

**Problème :** aucune confirmation que le relais pompe a physiquement commuté — risque de claquements non détectés.

**Solution :** le relais P2 (PCF8574) est commandé en miroir du relais pompe (P0). Son contact NO est câblé sur GPIO33 (pull-down interne). `updatePumpSystem()` lit GPIO33 après 500 ms de stabilisation :

- Mismatch stable > 30 s → mode dégradé (CRITICAL logué, pompe non bloquée)
- ≥ 5 transitions en 10 s → claquement détecté → blocage pompe 5 min + alerte CSV

**Résultat :** en l'absence de câblage GPIO33, le comportement est sûr (mode dégradé après 30 s, pompe continue).

---

### 2026-04-09 — Gestion de la saturation de /systeme.log

**Problème :** `logToFile("Sauvegarde auto")` appelé toutes les 5 min → **~15 KB/jour** → saturation LittleFS en 2–3 mois.

**Solution :**

- Suppression du `logToFile()` dans `PumpManager.cpp` (log port série conservé via `logSystem()`)
- Ajout de `trimSystemLog(200)` dans `StorageManager` — appelé chaque nuit à minuit
- Ajout de `purgeOldLogs(2)` dans `StorageManager` — supprime les CSV > 2 mois

**Résultat :** `/systeme.log` ne grossit plus qu'avec les vrais événements (reboots, alertes, changements de mode) → 3 à 10 lignes/jour maximum.

---

*Documentation générée automatiquement depuis le code source et le journal des modifications.*
*Pour mettre à jour : invoquer le skill `/generate_pool_docs`.*
