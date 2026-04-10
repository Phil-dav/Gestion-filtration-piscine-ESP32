<p align="center">
  <img src="assets/banner.webp" alt="Phil Domo - ESP32 Piscine Logo" width="100%">
</p>

---

<p align="center">
  <img src="https://img.shields.io/badge/Matériel-ESP32-E38B00?style=for-the-badge&logo=espressif" alt="ESP32">
  <img src="https://img.shields.io/badge/IDE-PlatformIO-0282C9?style=for-the-badge&logo=platformio" alt="PlatformIO">
  <img src="https://img.shields.io/badge/Thème-Vert%20Émeraude%20%26%20Or-D4AF37?style=for-the-badge" alt="Theme">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Statut-Actif-success?style=for-the-badge" alt="Status">
</p>

---

# ESP32 — Automatisme Piscine Modulaire

Contrôleur de filtration piscine embarqué sur ESP32, avec interface web, journalisation LittleFS, protection gel, et gestion multi-sources de l'heure (GPS + NTP).

---

## Fonctionnalités actuelles

### Gestion de la pompe

- **3 modes physiques** via interrupteur 3 positions (PCF8574) : **AUTO / OFF / MANU**.
- **Mode AUTO** : calcul automatique des heures de filtration selon la température de l'eau (règle T°/2).
- **Mode MANU** : commande manuelle via bouton web.
- **Boost web** : surclasse le mode AUTO temporairement (marche ou arrêt forcé).
- **Sécurités** : verrouillage 10s au démarrage et anti-rebond matériel sur GPIO33.

### Protection et sécurité

- **Anti-gel prioritaire** : pompe forcée sous 2°C, même en MODE_OFF.
- **Surveillance niveau d'eau** (GPIO34) : arrêt pompe + alarme si manque d'eau.
- **Défaut moteur** : entrée PCF8574 IN7 avec verrou et réarmement web requis.
- **Relais de signalisation** : 4 relais via PCF8574 pour les états système et alertes.

### Modes thermiques automatiques

| Température eau | Mode | Comportement |
| :--- | :--- | :--- |
| < 2°C | Anti-gel | Pompe forcée même en MODE_OFF |
| 2°C – 4°C | Gel | 0 h objectif (pompe arrêtée) |
| 4°C – 15°C | Hiver | Filtration réduite |
| 15°C – 28°C | Normal | T°/2 heures par jour |
| > 28°C | Canicule | Filtration continue 24h/24 |

---

## Architecture Logicielle & Matérielle

### Sources de temps

- **GPS (TinyGPSPlus)** : prioritaire (≥ 4 satellites).
- **NTP WiFi** : secours avec gestion automatique heure été/hiver.

### Interface Web & Logs

- **Dashboard** : temps réel via Server-Sent Events (SSE).
- **Journalisation LittleFS** : fichiers CSV pour les sessions, bilans journaliers et alertes.
- **Persistence** : sauvegarde des réglages et compteurs en NVS (Preferences).

### Câblage principal

| Signal | GPIO | Remarque |
| :--- | :--- | :--- |
| I2C (SDA/SCL) | 21 / 22 | Bus PCF8574 + OLED SSD1306 |
| DS18B20 | 4 | Température eau (OneWire) |
| Niveau eau | 34 | Entrée directe |
| GPS (RX/TX) | 16 / 17 | UART2 |

---

## Structure du Projet

```text
Architecture-modulaire/
├── src/main.cpp            # Logique de décision pompe
├── include/config.h        # Paramètres GPIO et I2C
├── assets/                 # Logo et ressources visuelles
├── lib/                    # Modules (GPS, Log, OLED, Pump, etc.)
└── data/                   # Interface Web (HTML/JS)
