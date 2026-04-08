#ifndef PCF8574_DRIVER_H
#define PCF8574_DRIVER_H

#include <Arduino.h>
#include <PCF8574.h>

// On partage l'objet pour qu'il soit accessible partout
extern PCF8574 pcf;

bool pcf_init();
bool isPcfReady();
void setRelay(uint8_t pin, bool state);
int pcf_read(uint8_t pin);

#endif