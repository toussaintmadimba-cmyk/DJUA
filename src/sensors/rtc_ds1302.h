#ifndef RTC_DS1302_H
#define RTC_DS1302_H

#include <Arduino.h>

// Date et heure locales lues depuis le DS1302.
struct RTCData
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool running;
    bool valid;
};

// Initialise le DS1302 et corrige une horloge invalide si cette option
// est activee dans config.h.
bool initRTC();

// Retourne la date et l'heure courantes du DS1302.
RTCData readRTC();

// Indique si une date valide et une horloge active ont ete detectees.
bool isRTCAvailable();

// Produit un timestamp ISO 8601 avec le decalage GMT configure.
// Exemple : 2026-09-04T15:42:05+01:00
bool formatRTCTimestamp(
    const RTCData& data,
    char* output,
    size_t outputSize
);

#endif
