# Journal des modifications — ESP32 Piscine

Ce fichier trace les décisions techniques importantes prises au fil du projet,
avec le raisonnement derrière chaque choix.

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
