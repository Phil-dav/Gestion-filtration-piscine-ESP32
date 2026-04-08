**CODE::**

```markdown
# ESP32 - Projet Modulaire

Bienvenue dans mon labo ESP32.  
Ce projet, c’est ma réponse à tous les sketchs mal foutus, les capteurs ajoutés à l’arrache, et les codes qu’on n’ose plus toucher après 3 semaines. 
Ici, je construis une base **propre**, **modulaire**, et **extensible**. 
Si je veux ajouter un capteur, je le branche, je l’installe, et basta. Pas de prise de tête.

## 🧠 Philosophie

- **Modularité** : chaque composant (WiFi, NTP, capteurs, interface web) est indépendant
- **Clarté** : pas de code spaghetti, pas de hacks douteux
- **Évolutivité** : je veux pouvoir ajouter des trucs sans tout casser

Je bosse avec Copilot pour garder une trace claire de mes choix, mes codes, et mes galères. Tout est documenté dans `docs/copilot-journal.md`.

## 🔧 Fonctionnalités prévues

- Connexion WiFi avec gestion automatique
- Synchronisation NTP avec fuseau horaire et heure d’été
- Serveur web intégré avec interface HTML responsive
- Communication en temps réel via Server-Sent Events (SSE)
- Architecture modulaire pour l’ajout de capteurs

## 📁 Arborescence du projet

esp32-projet-modulaire  
├── src/                 → Code source ESP32  
│   ├── include/         → Fichiers d’en-tête  
│   └── data/            → Interface web (HTML, CSS/JS)  
├── lib/                 → Libraries tierces  
├── docs/                → Journal technique et documentation  
└── README.md            → Ce fichier


## 🚀 Objectifs

- Créer une base solide et maintenable
- Documenter chaque étape du développement
- Partager une approche claire pour les projets ESP32

## 📜 Licence

Ce projet est sous licence MIT. Libre à vous de l’utiliser, le modifier et le partager.

---

Développé par **Philippe**, passionné d’électronique et de clarté,  
avec l’aide de **Copilot**, qui écrit vite mais écoute bien 🤖
```
