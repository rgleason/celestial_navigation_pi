#ifndef _MOON_H_
#define _MOON_H_

#define EARTH_RADIUS 6378.14		// Earth radius at equator in km.
#define MOON_MEAN_RADIUS 1737.5		// Moon mean radius in km
#define K_MOON ((MOON_MEAN_RADIUS) / (EARTH_RADIUS))

extern double moon_distance(double JD);

#endif
