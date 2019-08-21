#include "coordinates.h"

#include <stdlib.h>
#include <stdint.h>

#include "settings.h"

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(a,b) \
	({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a > _b ? _a : _b; })

void calculateShroomCoordinates() {
	//Calculate adresses
	shroomsx[0] = settings.gridX;
	shroomsy[0] = settings.gridY;
	shroomsz[0] = settings.gridZ;

	shroomsx[1] = settings.gridX+0;
	shroomsy[1] = settings.gridY+1;
	shroomsz[1] = settings.gridZ-1;

	shroomsx[2] = settings.gridX+1;
	shroomsy[2] = settings.gridY+0;
	shroomsz[2] = settings.gridZ-1;

	shroomsx[3] = settings.gridX+1;
	shroomsy[3] = settings.gridY-1;
	shroomsz[3] = settings.gridZ+0;

	shroomsx[4] = settings.gridX+0;
	shroomsy[4] = settings.gridY-1;
	shroomsz[4] = settings.gridZ+1;

	shroomsx[5] = settings.gridX-1;
	shroomsy[5] = settings.gridY+0;
	shroomsz[5] = settings.gridZ+1;

	shroomsx[6] = settings.gridX-1;
	shroomsy[6] = settings.gridY+1;
	shroomsz[6] = settings.gridZ+0;
}

int closestDistanceToShroomWaves(int shroomnr, int x, int y, int z) {
	int deltax = shroomsx[shroomnr] - x;
	deltax = abs(deltax);
	int deltay = shroomsy[shroomnr] - y;
	deltay = abs(deltay);
	int deltaz = shroomsz[shroomnr] - z;
	deltaz = abs(deltaz);
	
	int smallestDistance = max(deltax, deltay);
	smallestDistance = max(smallestDistance, deltaz);
	return smallestDistance;
}

