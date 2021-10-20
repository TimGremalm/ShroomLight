#ifndef COORDINATS_H
#define COORDINATS_H

#include <stdint.h>

int shroomsx[7];
int shroomsy[7];
int shroomsz[7];

void calculateShroomCoordinates();
int closestDistanceToShroomWaves(int shroomnr, int x, int y, int z);

#endif /* COORDINATS_H */

