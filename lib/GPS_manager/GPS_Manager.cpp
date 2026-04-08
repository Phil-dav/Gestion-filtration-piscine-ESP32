#include "config.h"
#include "GPS_Manager.h"
#include "DebugManager.h"

HardwareSerial GPS_Serial(2);
TinyGPSPlus gps;
int gpsLocalHour = 0; // Heure locale (UTC + correction été/hiver) mise à jour dans getGPSPitch()
void initGPS()
{
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  logSystem(INFO, "GPS", "GPS avec TinyGPS++ initialise");
}

void updateGPS()
{
  // Lecture non-bloquante : on traite ce qui est dispo
  while (GPS_Serial.available() > 0)
  {
    gps.encode(GPS_Serial.read());
  }
}

String getGPSPitch()
{
  if (!gps.time.isValid() || gps.date.year() < 2024)
  {
    return "Synchro GPS...";
  }

  // On travaille sur des copies pour pouvoir les modifier
  int year = gps.date.year();
  int month = gps.date.month();
  int day = gps.date.day();
  int hour = gps.time.hour();
  int min = gps.time.minute();
  int sec = gps.time.second();

  // 1. Calcul du jour de la semaine (d: 0=Dim, 1=Lun...)
  int a = (14 - month) / 12;
  int y = year - a;
  int m = month + 12 * a - 2;
  int d = (day + y + y / 4 - y / 100 + y / 400 + (31 * m) / 12) % 7;

  // 2. Logique Heure d'Été / Hiver précise
  int offset = 1;
  if (month > 3 && month < 10)
    offset = 2;
  else if (month == 3)
  {
    // Changement à 1h UTC (2h locale) le dernier dimanche
    if (day - d >= 25 && (d != 0 || hour >= 1))
      offset = 2;
  }
  else if (month == 10)
  {
    // Changement à 1h UTC (3h locale -> 2h) le dernier dimanche
    if (day - d < 25 || (d == 0 && hour < 1))
      offset = 2;
  }

  // 3. Application de l'offset et correction DATE/HEURE
  hour += offset;

  if (hour >= 24)
  {
    hour -= 24;
    day++; // On avance d'un jour

    // Gestion simplifiée de fin de mois (pour la date affichée)
    // Note : On pourrait faire un calcul complet, mais pour l'affichage
    // quotidien d'une piscine, c'est déjà beaucoup plus juste.
    int joursDansMois[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
      joursDansMois[2] = 29; // Année bissextile

    if (day > joursDansMois[month])
    {
      day = 1;
      month++;
      if (month > 12)
      {
        month = 1;
        year++;
      }
    }
  }
  gpsLocalHour = hour; // Heure locale disponible pour le reste du programme via extern
  char buffer[32];
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", day, month, year, hour, min, sec);
  return String(buffer);
}