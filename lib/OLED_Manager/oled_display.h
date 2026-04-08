// lib/OLED_Manager/oled_display.h
#pragma once
#include "includes.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Initialisation ---
void initOLED();

// --- Fonctions de base (utilisées à l'init dans main.cpp) ---
void displayMessage(const String &message);
void displayTwoLines(const String &l1, const String &l2);
void displayThreeLines(const String &line1, const String &line2, const String &line3);
void displayFourLines(const String &l1, const String &l2, const String &l3, const String &l4);

// --- Système multi-pages ---
// À appeler à CHAQUE itération de loop() pour la réactivité du bouton
void handleOledButton();

// À appeler toutes les secondes pour le rendu
void updateOledDisplay(String source, String time, float tAir, float hAir, float tEau);

// À appeler à minuit (reset bilan journalier T° min/max)
void resetDailyOledStats();
