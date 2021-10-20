#include "coordinates.h"

#include <stdlib.h>
#include <stdint.h>

#include "shroom_settings.h"

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(a,b) \
	({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a > _b ? _a : _b; })

void calculateShroomCoordinates() {
	//Calculate adresses
	shroomsx[0] = shroomsettings.gridX;
	shroomsy[0] = shroomsettings.gridY;
	shroomsz[0] = shroomsettings.gridZ;

	shroomsx[1] = shroomsettings.gridX+0;
	shroomsy[1] = shroomsettings.gridY+1;
	shroomsz[1] = shroomsettings.gridZ-1;

	shroomsx[2] = shroomsettings.gridX+1;
	shroomsy[2] = shroomsettings.gridY+0;
	shroomsz[2] = shroomsettings.gridZ-1;

	shroomsx[3] = shroomsettings.gridX+1;
	shroomsy[3] = shroomsettings.gridY-1;
	shroomsz[3] = shroomsettings.gridZ+0;

	shroomsx[4] = shroomsettings.gridX+0;
	shroomsy[4] = shroomsettings.gridY-1;
	shroomsz[4] = shroomsettings.gridZ+1;

	shroomsx[5] = shroomsettings.gridX-1;
	shroomsy[5] = shroomsettings.gridY+0;
	shroomsz[5] = shroomsettings.gridZ+1;

	shroomsx[6] = shroomsettings.gridX-1;
	shroomsy[6] = shroomsettings.gridY+1;
	shroomsz[6] = shroomsettings.gridZ+0;
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

