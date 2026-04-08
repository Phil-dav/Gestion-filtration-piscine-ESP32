#pragma once

// --- 1. BIBLIOTHÈQUES SYSTÈME (TOUJOURS EN PREMIER) ---
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <FS.h>
#include <LittleFS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_AHTX0.h>
#include <TinyGPSPlus.h>
#include <Preferences.h>
// --- 2. TES UTILITAIRES (Dossier lib/Utils) ---
#include "time_utils.h"
#include "wifi_utils.h"
#include "web_utils.h"

// --- 3. TES MANAGERS (Dossier lib/...) ---
#include "pcf8574_driver.h"
#include "oled_display.h"
#include "sensors_manager.h"
#include "GPS_Manager.h"
#include "ds18_manager.h"
#include "pcf8574_config.h"
#include "PCF8574.h"
#include "PumpManager.h"
#include "WaterLevel.h"
#include "WaterTempManager.h"
#include "SafetyManager.h"
#include "ModeManager.h"
#include "config.h"
#include "StorageManager.h"
#include "LogManager.h"
#include "DebugManager.h"
#include "ModeHistory.h"