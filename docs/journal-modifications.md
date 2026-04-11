# Journal des modifications — ESP32 Piscine

Ce fichier trace les décisions techniques importantes prises au fil du projet,
avec le raisonnement derrière chaque choix.

---

## 2026-04-11 — Surveillance mémoire heap avec protection par reboot préventif

### Problème identifié

L'ESP32 exécute en parallèle AsyncWebServer, ArduinoJson, LittleFS, GPS, OLED et DS18B20.
Ce contexte multi-couches rend possible une fuite mémoire progressive — allocations non
libérées qui consomment la heap octet par octet sur plusieurs heures ou jours.

Sans surveillance, une telle fuite provoque un crash OOM (Out Of Memory) ou un reboot
watchdog silencieux, sans aucune trace permettant de diagnostiquer ce qui s'est passé.
L'utilisateur constate un reboot inopiné sans en connaître la cause.

### Raisonnement

Deux besoins distincts à traiter séparément :

**1. Observabilité** — savoir si la heap évolue normalement ou se dégrade.
`ESP.getFreeHeap()` retourne la valeur courante (fluctue avec les allocations).
`ESP.getMinFreeHeap()` retourne le minimum enregistré depuis le dernier boot (indicateur
de tendance : si cette valeur diminue d'un log à l'autre, une fuite est probable).
Ces deux valeurs sont loguées toutes les 30 s dans le moniteur série — aucun coût CPU.

**2. Protection active** — si la heap descend sous un seuil critique, agir avant le crash.
En dessous de 20 000 octets libres, AsyncWebServer lui-même peut crasher. À ce stade,
un reboot contrôlé vaut mieux qu'un crash OOM non tracé.
Séquence choisie : sauvegarde NVS du compteur filtration → écriture dans le journal
alertes CSV → `ESP.restart()`.

Le journal alertes (LittleFS, persistant) est préféré à `logSystem()` (moniteur série)
parce qu'un reboot nocturne serait invisible en série — l'entrée CSV reste lisible
dans l'onglet **Alertes** de l'interface web dès le lendemain matin.

### Ce qui a été fait

#### `include/config.h`

Ajout de la constante de seuil :

```cpp
// Avant : rien

// Après :
#define HEAP_MIN_SAFE 20000  // Seuil reboot préventif heap (octets libres)
```

#### `src/main.cpp` — PRIORITÉ 5 bloc 30 s

**Avant :**
```cpp
if (now - lastWifiCheck > 30000)
{
    lastWifiCheck = now;
    wl_status_t wifiSt = WiFi.status();
    if (wifiSt != WL_CONNECTED) {
        logSystem(WARNING, "WIFI", "WiFi perdu (code=" + String(wifiSt) + ") - reconnexion...");
        WiFi.begin();
    }
}
```

**Après :**
```cpp
if (now - lastWifiCheck > 30000)
{
    lastWifiCheck = now;

    uint32_t freeHeap = ESP.getFreeHeap();
    logSystem(INFO, "MEM", "Heap : libre=" + String(freeHeap) + " B  min=" + String(ESP.getMinFreeHeap()) + " B");

    if (freeHeap < HEAP_MIN_SAFE) {
        saveFiltrationProgress(pumpingDoneToday);
        logAlerte("MEMOIRE_CRITIQUE",
            ("Heap libre : " + String(freeHeap) + " B — reboot déclenché (seuil " + String(HEAP_MIN_SAFE) + " B)").c_str());
        ESP.restart();
    }

    wl_status_t wifiSt = WiFi.status();
    if (wifiSt != WL_CONNECTED) {
        logSystem(WARNING, "WIFI", "WiFi perdu (code=" + String(wifiSt) + ") - reconnexion...");
        WiFi.begin();
    }
}
```

### Résultat attendu

**En fonctionnement normal :**
Le moniteur série affiche toutes les 30 s :

```text
[INFO]     MEM : Heap : libre=187432 B  min=164208 B
```

Si `min` reste stable sur plusieurs jours → pas de fuite.
Si `min` descend progressivement → fuite identifiée, source à investiguer.

**En cas de fuite atteignant le seuil :**

- Le compteur de filtration est sauvegardé en NVS avant le reboot
- Le journal Alertes contient l'entrée horodatée :

  ```text
  11/04/2026 │ 03:28:15 │ MEMOIRE_CRITIQUE │ Heap libre : 18432 B — reboot déclenché (seuil 20000 B)
  ```

- Au boot suivant, la cause de redémarrage `RESET_LOGICIEL` apparaît dans le log BOOT
- Les deux traces combinées permettent de dater et quantifier la fuite

**Ce que ce mécanisme ne couvre pas :**
Une fuite brutale crashant l'ESP32 avant le prochain tick de 30 s ne sera pas tracée.
En pratique les fuites mémoire sont lentes et progressives — ce cas est considéré négligeable.

---

## 2026-04-10 — Feedback pompe visible dans l'interface web

### Problème identifié

Quand la sécurité anti-claquement bloque la pompe (5 min) ou que le fil GPIO33
est rompu (mode dégradé), aucun indicateur n'était visible dans l'interface web.
L'utilisateur ne savait pas pourquoi la pompe ne démarrait pas.

### Raisonnement

`isPumpBlocked()` et `isFeedbackFault()` existaient déjà dans `PumpManager`
mais leurs états n'étaient pas exposés dans le JSON `/sensors`.
La carte "Défauts Système" de l'interface possède déjà un mécanisme générique
`updateFaults()` qui affiche dynamiquement les alertes actives — il suffisait
de lui fournir les deux nouveaux états.

### Ce qui a été fait

#### `lib/Utils/web_utils.cpp` — endpoint `/sensors`

Ajout de deux champs dans le JSON retourné :

```cpp
json += "\"pumpBlocked\":"   + String(isPumpBlocked()   ? "true" : "false") + ",";
json += "\"feedbackFault\":"  + String(isFeedbackFault() ? "true" : "false") + ",";
```

#### `data/script.js` — fonction `fetchAllData()`

Ajout de deux messages dans le tableau `faults[]` transmis à `updateFaults()` :

```javascript
if (data.pumpBlocked)   faults.push('⛔ POMPE BLOQUÉE — claquement détecté — reprise automatique dans 5 min');
if (data.feedbackFault) faults.push('⚠ Fil feedback GPIO33 non connecté — mode dégradé (pompe non bloquée)');
```

### Résultat attendu

- Claquement détecté → bandeau rouge "POMPE BLOQUÉE" dans l'interface
- Fil GPIO33 absent → avertissement "mode dégradé" visible dès le démarrage
- Aucune modification HTML requise (`updateFaults()` génère les items dynamiquement)

---

## 2026-04-10 — Timeline filtration : journée complète + marqueurs de plage

### Problème identifié

La timeline de filtration sur l'interface web était clippée à la plage horaire configurée
(ex: 8h–20h). Tout événement pompe hors de cette fenêtre (anti-gel à 3h du matin, mode
MANU en soirée, boost hors plage) était invisible — aucune trace dans l'interface.

### Raisonnement

La plage de filtration est une **consigne**, pas la limite des événements réels.
La pompe peut s'activer à n'importe quelle heure (anti-gel, MANU, boost).
Montrer uniquement la fenêtre configurée donne une vue partielle et trompeuse de la journée.

### Ce qui a été fait

#### `data/script.js` — fonction `updateFiltration()`

- Échelle de la timeline : fenêtre `[debut, fin]` → **journée complète 0h–24h**
- `toPercent` : `(h - debut) / fenetre` → `h / 24`
- Labels d'axe : début/milieu/fin de plage → **0h / 12h / 24h** (fixes)
- Zone surlignée (`filtRange`) : toute la barre → **plage configurée uniquement**
  (positionnée à `toPercent(debut)`, largeur `(fin-debut)/24*100%`)
- Segments de mode : clipping `[debut, fin]` supprimé → **toute la journée visible**
- Barre "fait" (fallback sans historique) : `left=0%` → `left=toPercent(debut)`

#### `data/index.html` + `data/script.js` — marqueurs visuels de plage

Ajout de deux traits bleus verticaux (`filtr-window-mark`) sur la timeline,
positionnés à `debut` et `fin`. Chacun porte un label horaire en dessous
(ex : `8h00` et `20h00`).

### Résultat attendu

- Les événements hors plage (anti-gel nocturne, MANU en soirée) sont **visibles**
- La plage configurée reste identifiable grâce à la zone surlignée et aux deux traits bleus
- L'axe 0h–24h donne une lecture immédiate de la position dans la journée

---

## 2026-04-09 — Gestion de la saturation de /systeme.log

### Problème identifié

Au reboot, le port série affichait l'intégralité de `/systeme.log`.
Ce fichier contenait des centaines de lignes "Sauvegarde auto" car la fonction
`logToFile()` était appelée **toutes les 5 minutes** depuis `PumpManager.cpp`.

Calcul de croissance :
- 288 lignes/jour × ~55 octets = **~15 KB/jour**
- LittleFS (1 à 1,5 MB) → **saturation en 2 à 3 mois**

### Raisonnement

La ligne "Sauvegarde auto" dans `/systeme.log` était **redondante** :
- La vraie sauvegarde de la progression de filtration se fait en **NVS** (`saveFiltrationProgress()`)
- NVS = mémoire flash persistante, survit aux coupures secteur
- Le fichier `systeme.log` ne sert qu'à afficher l'historique sur le port série au reboot

**Conclusion** : la valeur utile en cas de reboot, c'est la dernière enregistrée en NVS.
Les 288 lignes intermédiaires dans le fichier ne servent à rien — NVS fait déjà ce travail.

### Ce qui a été fait

#### 1. Retrait du `logToFile()` dans PumpManager.cpp (ligne ~172)

Fichier : `lib/PumpManager/PumpManager.cpp`

**Avant :**
```cpp
logToFile("Sauvegarde auto : " + String(pumpingDoneToday, 2) + "h (...)");
```

**Après :**
```cpp
// [RETIRÉ] logToFile("Sauvegarde auto : ...") — supprimé volontairement.
// Raison : saturation LittleFS (288 lignes/jour). La NVS conserve la vraie valeur.
logSystem(INFO, "PUMP", "Sauvegarde auto : " + String(pumpingDoneToday, 2) + "h (...)");
```

L'affichage **port série en direct** est conservé via `logSystem()`.
Seule l'écriture dans le fichier est supprimée.

#### 2. Ajout de trimSystemLog() dans StorageManager

Fichier : `lib/StorageManager/StorageManager.h` et `.cpp`

Fonction de sécurité appelée chaque nuit à minuit lors du reset journalier.
Tronque `/systeme.log` aux **200 dernières lignes** si le fichier dépasse cette limite.
Algorithme : lecture → seek → copie dans `/sys_tmp.log` → remplacement.

```cpp
void trimSystemLog(int maxLines = 200);
```

#### 3. Ajout de purgeOldLogs() dans StorageManager

Fichier : `lib/StorageManager/StorageManager.h` et `.cpp`

Supprime les fichiers CSV dans `/logs/` plus vieux que N mois.
Les CSV sont au format `sessions_MMYY.csv`, `daily_MMYY.csv`, `alertes_MMYY.csv`.
Par défaut : garde les **2 derniers mois**.

```cpp
void purgeOldLogs(int keepMonths = 2);
```

#### 4. Appels à minuit dans main.cpp

Fichier : `src/main.cpp` — bloc reset journalier (`jourPourPompe != dernierJour`)

```cpp
trimSystemLog(200);  // filet de sécurité sur systeme.log
purgeOldLogs(2);     // purge CSV anciens
```

### Résultat attendu

- `/systeme.log` ne grossit plus qu'avec les vrais événements : reboots, changements de mode, alertes gel → **3 à 10 lignes/jour maximum**
- Plus aucun risque de saturation LittleFS
- L'affichage port série au reboot reste lisible et utile

---

---

## 2026-04-09 — Feedback hardware pompe (relais miroir + GPIO33)

### Problème identifié

Risque de démarrages/arrêts intempestifs du relais pompe (claquements) sans possibilité
de le détecter logiciellement. La commande est envoyée au PCF8574 mais rien ne confirme
que le relais a physiquement commuté.

### Raisonnement

Utiliser le **relais 2** (PCF8574 P2, jusqu'ici `PIN_RELAY_ALERTE_GEL` non utilisé) comme
relais miroir commandé en parallèle du relais pompe. Son contact NO renvoie l'info sur
**GPIO33** de l'ESP32 : fermé = pompe ON confirmée, ouvert = pompe OFF.

Deux situations distinctes à traiter différemment :

- **Claquement** : transitions ON↔OFF rapides et répétées → alerte + blocage temporaire 5 min
- **Fil rompu** : mismatch stable pendant > 30 s → mode dégradé (feedback ignoré, pompe non bloquée)

La pompe ne doit **jamais être bloquée** par une panne mécanique du fil de retour.

### Ce qui a été fait

#### `include/config.h`

- Renommé `PIN_RELAY_ALERTE_GEL 2` → `PIN_RELAY_FEEDBACK_POMPE 2`
- Ajouté `PIN_FEEDBACK_POMPE 33` (GPIO33 ESP32, pull-down interne)

#### `lib/PumpManager/PumpManager.h`

- Ajouté `isFeedbackFault()` — true si fil GPIO33 rompu (mode dégradé actif)
- Ajouté `isPumpBlocked()` — true si pompe bloquée suite à claquement

#### `lib/PumpManager/PumpManager.cpp`

- `initPumpManager()` : `pinMode(PIN_FEEDBACK_POMPE, INPUT_PULLDOWN)`
- `updatePumpSystem()` : lecture GPIO33 après délai de stabilisation (500 ms), détection mismatch
  - Mismatch > 30 s → `FEEDBACK_ROMPU` → mode dégradé (log CRITICAL + alerte CSV)
  - ≥ 5 transitions en 10 s → `CLAQUEMENT` → blocage pompe 5 min (log CRITICAL + alerte CSV)
- Lors de chaque changement de relais : `setRelay(PIN_RELAY_FEEDBACK_POMPE, pumpState)` en parallèle
- `forceStopPump()` : éteint aussi le relais miroir

### Résultat attendu

**Sans câblage (GPIO33 non connecté)** :

- GPIO33 reste LOW en permanence
- Dès que la pompe tourne → mismatch détecté
- Après 30 s → message CRITICAL "fil rompu, mode dégradé" → pompe continue normalement
- Comportement idéal pour tester le code avant de faire le câblage

**Avec câblage (GPIO33 câblé sur contact NO du relais miroir)** :

- Feedback cohérent → aucun message d'erreur
- Claquement réel détecté → blocage 5 min + alerte CSV
- Rupture de fil en cours de fonctionnement → mode dégradé après 30 s

---

## Modèle d'entrée pour les prochaines modifications

```
## AAAA-MM-JJ — Titre court

### Problème identifié
...

### Raisonnement
...

### Ce qui a été fait
Fichier : ...
Avant / Après (si applicable)

### Résultat attendu
...
```
