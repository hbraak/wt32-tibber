#ifndef BOILER_H
#define BOILER_H

void syncBoilerStatus();
void setBoilerPower(int power);
void toggleBoilerMode();
void drawBoilerSwitch();
void displayError(const char* message);  // Hinzugefügt: Fehlermeldung anzeigen
int getCurrentBoilerMode();  // Hinzugefügt: Aktuellen Boiler-Modus abfragen
#endif