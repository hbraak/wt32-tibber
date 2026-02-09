#ifndef TIBBER_H
#define TIBBER_H

// Deklarationen der Variablen mit extern
extern const char* tibberAPI;
extern const char* tibberToken;
extern const char* tibberQuery;
extern float currentElectricityPrice; // Globale Variable deklarieren
extern float tibberPrices[48];
// Prototypen der Funktionen
void fetchTibberPrices();
float getCurrentPrice();
void checkAndFetchTibberPrices();
void updateCurrentElectricityPrice();
int getCurrentHour();
void shiftTibberPrices();



#endif // TIBBER_H
