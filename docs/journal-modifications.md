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
