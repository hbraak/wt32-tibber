#ifndef VRM_H
#define VRM_H

// VRM daily stats (updated every 5 minutes)
extern float vrmSolarYield;       // kWh today
extern float vrmConsumption;      // kWh today
extern float vrmGridToConsumer;   // kWh from grid today
extern float vrmGridToGrid;       // kWh to grid today
extern float vrmSelfConsumption;  // % (solar-export)/solar
extern float vrmNetGrid;          // kWh net (positive=import, negative=export)
extern bool  vrmDataLoaded;

void fetchVrmToken();
void fetchVrmDailyStats();

#endif
