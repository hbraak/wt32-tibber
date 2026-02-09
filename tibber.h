#ifndef TIBBER_H
#define TIBBER_H

extern float currentElectricityPrice;
extern float tibberPrices[48];

void fetchTibberPrices();
void checkAndFetchTibberPrices();
void updateCurrentElectricityPrice();
int getCurrentHour();
void shiftTibberPrices();

#endif
