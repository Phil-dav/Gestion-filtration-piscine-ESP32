# Journal des améliorations — Automate Piscine ESP32

Ce fichier liste les améliorations identifiées, classées par priorité.
Cocher la case `[x]` une fois l'amélioration terminée, puis ajouter une entrée dans `journal-modifications.md` si la modification est significative.

---

## Légende

| Symbole | Signification |
|---------|---------------|
| `[ ]`   | À faire       |
| `[x]`   | Terminé       |
| `[~]`   | En cours      |
| `[?]`   | À investiguer avant de coder |

---

## Priorité haute — Code

### A1 — Vérifier `hourCorrectionIndex` (GPS)
- **Fichiers concernés :** `lib/GPS_manager/GPS_Manager.h`, `src/main.cpp`
- **Statut :** `[x]` — 2026-04-10 — Non applicable (déjà résolu)

**Résultat de l'investigation (2026-04-10) :**
`hourCorrectionIndex` n'existe plus dans le code — supprimé lors d'une session précédente.
La variable actuelle `gpsLocalHour` est correctement définie une seule fois dans `GPS_Manager.cpp`
et déclarée `extern` dans `GPS_Manager.h`. Aucune action requise.

---

### A2 — Documenter le `delay(100)` dans `setup()`
- **Fichiers concernés :** `src/main.cpp` ligne ~22
- **Statut :** `[x]` — 2026-04-10

**Problème :** Le seul `delay()` du projet est dans `setup()` après `Serial.begin()`.
C'est légitime (attente stabilisation Serial) mais sans commentaire il pourrait être
supprimé par erreur lors d'un refactoring futur.

**Action :**
- [ ] Ajouter un commentaire explicatif sur la ligne `delay(100)`

```cpp
delay(100); // Stabilisation Serial — seul delay() autorisé dans ce projet (setup uniquement)
```

---

### A3 — Afficher l'état `isPumpBlocked()` dans l'interface web

- **Fichiers concernés :** `lib/Utils/web_utils.cpp`, `data/script.js`
- **Statut :** `[x]` — 2026-04-10

**Ce qui a été fait :**

- `web_utils.cpp` : ajout de `pumpBlocked` et `feedbackFault` dans le JSON `/sensors`
- `script.js` : ajout de 2 messages dans `updateFaults()` — alerte rouge bloquage pompe, avertissement mode dégradé GPIO33
- `index.html` : aucune modification nécessaire (`updateFaults()` génère les items dynamiquement)

---

### A4 — Interface web : afficher "Continu" en mode canicule
- **Fichiers concernés :** `data/index.html`, `lib/Utils/web_utils.h`
- **Statut :** `[ ]`

**Problème :** En mode canicule (T° > 28°C), la filtration est continue mais l'interface
affiche probablement une plage horaire standard, ce qui est trompeur.

**Action :**
- [ ] Vérifier ce qu'affiche l'UI pour la plage horaire quand `isCaniculeActif() == true`
- [ ] Si trompeur : afficher "Filtration continue (canicule)" à la place de l'horaire

---

## Priorité normale — Code

### B1 — Réaffecter `PIN_RELAY_TEMP_EAU` (PCF P1, libre)
- **Fichiers concernés :** `include/config.h`, `lib/PumpManager/PumpManager.cpp`
- **Statut :** `[?]`

**Problème :** Le relais P1 est libre depuis le renommage. Une LED ou buzzer
d'alarme (gel, canicule, défaut niveau) serait utile sans câblage supplémentaire complexe.

**Action :**
- [ ] Décider de l'usage : alarme visuelle LED, buzzer, ou garder libre
- [ ] Une fois décidé : renommer dans `config.h` et câbler en conséquence

---

### B2 — Protéger `calculateTargetHours()` contre -127°C
- **Fichiers concernés :** `lib/WaterTempManager/WaterTempManager.cpp`
- **Statut :** `[ ]`

**Problème :** Si le DS18B20 retourne -127°C (erreur bus OneWire) avant que
`isTempValid()` soit appelé, `calculateTargetHours(-127)` retourne une valeur négative
(-63.5 h), ce qui peut fausser le compteur de filtration.

**Action :**
- [ ] Ajouter un guard en entrée de `calculateTargetHours()` :
```cpp
if (temp < -10.0f || temp > 50.0f) return 0.0f; // Valeur hors plage — capteur non prêt
```

---

### B3 — Fallback reset journalier sans GPS ni WiFi
- **Fichiers concernés :** `src/main.cpp`
- **Statut :** `[?]`

**Problème :** Si NTP et GPS sont indisponibles en permanence, `jourPourPompe == -1`
et le reset journalier ne se déclenche jamais → `pumpingDoneToday` grossit indéfiniment.

**Action :**
- [ ] Investiguer la fréquence réelle des pannes WiFi+GPS simultanées sur le terrain
- [ ] Si le risque est réel : ajouter un reset par `millis()` après 24 h sans source de temps valide

---

## Priorité haute — Sécurité & Stabilité (suggestions proactives)

### D1 — Surveillance de la mémoire heap

- **Fichiers concernés :** `src/main.cpp`, `include/config.h`
- **Statut :** `[x]` — 2026-04-11

**Ce qui a été fait :**

- `config.h` : constante `HEAP_MIN_SAFE 20000` (seuil reboot préventif)
- `main.cpp` bloc 30 s : log INFO heap courante + minimum depuis boot (`getMinFreeHeap`)
- Si `freeHeap < HEAP_MIN_SAFE` : sauvegarde NVS → `logAlerte("MEMOIRE_CRITIQUE", ...)` → `ESP.restart()`
- L'alerte est visible dans l'onglet **Alertes** de l'interface web après le reboot

---

### D2 — Protection de l'interface web (authentification)

- **Fichiers concernés :** `lib/Utils/web_utils.cpp`
- **Statut :** `[ ]`
- **Difficulté :** Faible

**Problème :** N'importe qui sur le réseau WiFi local peut allumer/arrêter la pompe,
effacer les journaux, modifier la plage horaire. Aucune authentification en place.

**Action :**

- [ ] Ajouter HTTP Basic Auth sur toutes les routes sensibles (`/pump`, `/boost`,
  `/set-schedule`, `/clear-logs`, `/reset_motor_fault`) via le mécanisme natif
  d'ESPAsyncWebServer (`request->authenticate()`)
- [ ] Stocker login/mot de passe dans `secrets.h`

---

### D3 — Mise à jour OTA (Over-The-Air)

- **Fichiers concernés :** `platformio.ini`, `src/main.cpp`
- **Statut :** `[ ]`
- **Difficulté :** Faible

**Problème :** Chaque mise à jour firmware nécessite un câble USB et d'être physiquement
à côté de l'ESP32 — contraignant quand l'automate est installé au bord de la piscine.

**Action :**

- [ ] Ajouter `ArduinoOTA` dans `setup()` (bibliothèque incluse dans le framework Arduino ESP32)
- [ ] Configurer `platformio.ini` avec `upload_protocol = espota` et l'IP fixe de l'ESP32
- [ ] Protéger l'OTA par un mot de passe dans `secrets.h`

---

### D4 — Graphique de température eau sur 7 jours

- **Fichiers concernés :** `data/index.html`, `data/script.js`, `lib/LogManager/`
- **Statut :** `[?]`
- **Difficulté :** Moyenne

**Problème :** Les CSV `daily_MMYY.csv` contiennent déjà les températures journalières
mais aucune visualisation n'existe. Voir la courbe de montée en température au printemps
permettrait d'anticiper le passage en mode canicule.

**Action :**

- [ ] Vérifier que `daily_MMYY.csv` contient bien la température min/max par jour
- [ ] Ajouter un endpoint `/log/daily` qui retourne le CSV du mois
- [ ] Afficher une courbe simple (canvas HTML5 ou SVG) dans l'interface web

---

## Priorité normale — Documentation

### C1 — Ajouter une entrée journal pour CLAUDE.md et PROJECT_DOC.md
- **Fichiers concernés :** `docs/journal-modifications.md`
- **Statut :** `[ ]`

**Action :**
- [ ] Rédiger une entrée dans `journal-modifications.md` expliquant la mise en place
  de l'outillage de documentation (skill `generate_pool_docs`, `PROJECT_DOC.md`, `CLAUDE.md`)

---

### C2 — Mettre à jour le skill `generate_pool_docs`
- **Fichiers concernés :** `C:\Users\floph\.claude\skills\generate_pool_docs.md`
- **Statut :** `[ ]`

**Action :**
- [ ] Ajouter dans le skill la lecture de `CLAUDE.md` (section "Projets futurs")
  pour que les projets à venir apparaissent dans la documentation générée
- [ ] Ajouter la lecture de `docs/ameliorations.md` pour un résumé du backlog

---

### C3 — Section "Démarrage rapide" dans README.md
- **Fichiers concernés :** `README.md`
- **Statut :** `[ ]`

**Action :**
- [ ] Ajouter au README les commandes PlatformIO essentielles (build, upload, monitor, uploadfs)
- [ ] Ajouter un schéma rapide des GPIO (ou lien vers `PROJECT_DOC.md`)

---

## Historique des améliorations terminées

| Date       | Réf | Description                                                              |
|------------|-----|--------------------------------------------------------------------------|
| 2026-04-11 | D1  | Surveillance heap 30 s + reboot préventif + logAlerte MEMOIRE_CRITIQUE   |
| 2026-04-10 | A2  | Commentaire sur le `delay(100)` de `setup()` dans `main.cpp`             |
| 2026-04-10 | A1  | Investigation : `hourCorrectionIndex` déjà supprimé — sans action        |
| 2026-04-10 | —   | Timeline 0h–24h + marqueurs bleus début/fin de plage (script.js, html)   |

---

*Ce fichier est maintenu manuellement. Mettre à jour le statut au fil des sessions de travail.*
