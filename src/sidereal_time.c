/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 

Copyright (C) 2000 Liam Girdwood <liam@nova-ioe.org>
*/

#include <math.h>
#include "nutation.h"

#ifndef PI
#define PI 3.141592653589793
#endif

#define degrad(x) ((x) * PI / 180)
#define raddeg(x) ((x) * 180 / PI)


/* puts a large angle in the correct range 0 - 360 degrees */
double range_degrees (double angle)
{
    double temp;
    double range;
    
    
    if (angle >= 0.0 && angle < 360.0)
    	return(angle);
 
	temp = (int)(angle / 360);
	
	if ( angle < 0.0 )
	   	temp --;

    temp *= 360;
	range = angle - temp;
    return (range);
}


/*! \fn double get_mean_sidereal_time (double JD)
* \param JD Julian Day
* \return Mean sidereal time.
*
* Calculate the mean sidereal time at the meridian of Greenwich of a given date.
*/
/* Formula 11.1, 11.4 pg 83 
*/

double get_mean_sidereal_time (double JD)
{
    double sidereal;
    double T;
    
    T = (JD - 2451545.0) / 36525.0;
        
    /* calc mean angle */
    sidereal = 280.46061837 + (360.98564736629 * (JD - 2451545.0)) + (0.000387933 * T * T) - (T * T * T / 38710000.0);
    
    /* add a convenient multiple of 360 degrees */
    sidereal = range_degrees(sidereal);
    
    /* change to hours */
    sidereal *= 24.0 / 360.0;
        
    return(sidereal);
} 

/*! \fn double get_apparent_sidereal_time (double JD)
* \param JD Julian Day
* /return Apparent sidereal time (hours).
*
* Calculate the apparent sidereal time at the meridian of Greenwich of a given date. 
*/
/* Formula 11.1, 11.4 pg 83 
*/

double get_apparent_sidereal_time (double JD)
{
   double correction, hours, sidereal;
   struct ln_nutation nutation;  
   
   /* get the mean sidereal time */
   sidereal = get_mean_sidereal_time (JD);
        
   /* add corrections for nutation in longitude and for the true obliquity of 
   the ecliptic */   
   get_nutation (JD, &nutation); 
    
   correction = (nutation.longitude / 15.0 * cos (degrad(nutation.obliquity)));
  
   /* value is in degrees so change it to hours and add to mean sidereal time */
   hours = (24.0 / 360.0) * correction;

   sidereal += hours;
   
   return (sidereal);
}    

/* this function has been hacked by rcorlan to change the parameter types
 * sidereal is the gast (not local) */
    
void get_hrz_from_equ_sidereal_time (double objra, double objdec, 
				     double lng, double lat, 
				     double sidereal, double *alt, double *az)
{
	double H, ra, latitude, declination, A, Ac, As, h, Z, Zs;

	/* change sidereal_time from hours to radians*/
	sidereal *= 2.0 * PI / 24.0;

	/* calculate hour angle of object at observers position */
	ra = degrad (objra);
	H = sidereal - degrad (lng) - ra;

	/* hence formula 12.5 and 12.6 give */
	/* convert to radians - hour angle, observers latitude, object declination */
	latitude = degrad (lat);
	declination = degrad (objdec);

	/* formula 12.6 *; missuse of A (you have been warned) */
	A = sin (latitude) * sin (declination) + cos (latitude) * cos (declination) * cos (H);
	h = asin (A);

	/* covert back to degrees */
	*alt = raddeg (h);   

	/* zenith distance, Telescope Control 6.8a */
	Z = acos (A);

	/* is'n there better way to compute that? */
	Zs = sin (Z);

	/* sane check for zenith distance; don't try to divide by 0 */

	if (Zs < 1e-5)
	{
		if (lat > 0)
			*az = 180;
		else
			*az = 0;
		return;
	}

	/* formulas TC 6.8d Taff 1991, pp. 2 and 13 - vector transformations */
	As = (cos (declination) * sin (H)) / Zs;
	Ac = (sin (latitude) * cos (declination) * cos (H) - cos (latitude) * sin (declination)) / Zs;

	// don't blom at atan2
	if (fabs(As) < 1e-5)
	{
		*az = 0;
		return;
	}
	A = atan2 (As, Ac);

	// normalize A
	A = (A < 0) ? 2 * M_PI + A : A;

	/* covert back to degrees */
	*az = range_degrees(raddeg (A));
}
