# CLAUDE.md — Automate Piscine ESP32

Instructions permanentes pour Claude Code sur ce projet.

---

## Contexte du projet

Automate de gestion de filtration de piscine sur **ESP32** (VS Code + PlatformIO, framework Arduino).
Développeur : Phil-dav — électronicien de formation, expérience Z80/8080, apprentissage ESP32/C++.
Langue de travail : **français exclusivement**.

---

## Règles de code — OBLIGATOIRES

### Aucun `delay()` dans la boucle principale
Toute temporisation doit passer par `millis()` avec des variables statiques.
Exemple correct :
```cpp
static uint32_t last = 0;
if (millis() - last >= 5000) { last = millis(); /* action */ }
```

### Architecture modulaire — respecter la séparation des responsabilités
Chaque fonctionnalité a son manager dans `lib/`. Ne pas écrire de logique métier dans `main.cpp`.
- Logique pompe → `PumpManager`
- Calcul filtration / modes thermiques → `WaterTempManager`
- Sécurités → `SafetyManager`
- Persistance → `StorageManager`
- Logs → `LogManager`

### Système de log unifié
Utiliser exclusivement `logSystem(level, "MODULE", "message")`. Ne jamais écrire `Serial.print()` directement.
Niveaux : `INFO`, `WARNING`, `CRITICAL`
Format moniteur : `    [INFO]     MODULE : message`

### Interrupteur debug
Dans `include/config.h` ligne 3 : `#define DEBUG_ENABLED` (commenter pour désactiver tout le debug en production).

### Sécurité pompe — mot-clé obligatoire
Toute modification de la logique relais pompe (PumpManager, config.h pins relais) nécessite
que l'utilisateur prononce le mot-clé **`sécurité-pompe`** avant que Claude ne touche au code.
Raison : risque de dommage matériel (moteur, contacts).

---

## Fichiers sensibles

| Fichier | Règle |
|---|---|
| `include/secrets.h` | **Ne jamais committer** — contient les credentials WiFi |
| `include/config.h` | Vérifier les adresses I2C et GPIO avant toute modif |
| `lib/PumpManager/` | Mot-clé `sécurité-pompe` requis |

---

## Commandes PlatformIO utiles

```bash
# Compiler
pio run

# Compiler + flasher
pio run -t upload

# Moniteur série (115200 baud)
pio device monitor

# Compiler + flasher + moniteur
pio run -t upload && pio device monitor

# Effacer le filesystem LittleFS
pio run -t uploadfs
```

---

## Structure du projet

```
src/
  main.cpp          — Boucle principale (5 priorités par millis)
include/
  config.h          — GPIO, constantes, DEBUG_ENABLED
  includes.h        — Header universel (inclus par tous les .cpp)
  secrets.h         — Credentials WiFi (non commité)
lib/
  PumpManager/      — Relais pompe + feedback GPIO33 + compteur filtration
  WaterTempManager/ — Calcul T°/2, modes GEL/CANICULE, plage horaire NVS
  SafetyManager/    — Niveau eau + défaut moteur (verrou latché)
  ModeManager/      — Interrupteur physique AUTO/MANU/OFF via PCF8574
  StorageManager/   — NVS (Preferences) + LittleFS (logs, CSV mensuels)
  LogManager/       — logSystem, logToFile, logSession, logAlerte
  ModeHistory/      — Timeline modes pour interface web
  GPS_manager/      — TinyGPSPlus, heure locale GPS
  ds18_manager/     — DS18B20 non-bloquant (demande/lecture découplées)
  Sensors_Manager/  — AHT10 température/humidité air
  WaterLevel/       — Détecteur niveau GPIO34
  PCF8574_Manager/  — Driver I2C expandeur relais
  OLED_Manager/     — Affichage multi-pages 128×64
  DebugManager/     — Flag DEBUG_ENABLED en NVS
  Utils/            — wifi_utils, web_utils, time_utils
data/
  index.html        — Interface web (servie par LittleFS)
docs/
  journal-modifications.md  — Décisions techniques (à maintenir)
PROJECT_DOC.md      — Documentation technique générée
```

---

## Méthode de travail

1. **Montrer avant/après** pour chaque modification de code — l'utilisateur valide avant application.
2. **Procéder fichier par fichier**, pas de modifications groupées sans explication.
3. **Demander confirmation** avant tout push GitHub.
4. **Mettre à jour `docs/journal-modifications.md`** de manière autonome pour toute décision technique significative (nouvelle architecture, correction de fond, mécanisme de sécurité). Ne pas le faire pour des corrections mineures.
5. Utiliser le modèle d'entrée présent dans le journal (date, problème, raisonnement, avant/après, résultat attendu).

---

## GPIO — référence rapide

| GPIO | Rôle |
|------|------|
| GPIO4 | DS18B20 (OneWire, température eau) |
| GPIO16/17 | GPS UART RX/TX |
| GPIO21/22 | I2C SDA/SCL (OLED, PCF8574, AHT10) |
| GPIO33 | Feedback pompe — Contact NO relais miroir (pull-down) |
| GPIO34 | Détecteur niveau d'eau |

**PCF8574 (0x21) — LOW = activé :**
P0 = Relais pompe · P1 = Libre · P2 = Relais miroir pompe · P3 = Défaut système
P4 = Bouton OLED · P5 = MANU · P6 = AUTO · P7 = Défaut moteur

---

## Projets futurs (ne pas coder sans demande explicite)

- **Capteur de courant** SCT-013 ou ACS712 sur moteur 220V — détection anomalies par consommation
- **Logging CSV enrichi** — sessions pompe, bilan journalier, alertes horodatées (LittleFS Option B/C)
- **Accès extérieur** — Raspberry Pi comme tunnel ngrok/Cloudflare (Orange bloque les ports entrants)
- **Distinction reset/coupure courant** — solution hardware GPIO + condensateur (limitation ESP32 confirmée)
