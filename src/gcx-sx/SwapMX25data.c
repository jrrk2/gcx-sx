#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

static unsigned char mylog(unsigned short arg, unsigned short min, unsigned short max)
{
  double ln = log(arg-min)/log(max-min);
  return floor(ln*255);
}

void Arrayswap (short *Imptr1, short *Imptr2, short height, short width, short *end1, short *end2)
{
	int y,z;
// Imptr1 points to output array start, Imptr2 points to start of multiplexed image data

short *Impt1, *Impt2, *Impt5;

memset(Imptr1, 0, (end1-Imptr1)*sizeof(short));

Impt1=Imptr1;
Impt2=Imptr1+width;

Impt5=Imptr2;

for (y=1; y <= height/2; y++)
{		// data move
	for (z=1; z <= width/2; z++)
	{
		assert(Impt1 < end1);
		assert(Impt5 < end2);
		*Impt1++=*Impt5++;
		assert(Impt2 < end1);
		assert(Impt5 < end2);
		*Impt2++=*Impt5++;
		assert(Impt2 < end1);
		assert(Impt5 < end2);
		*Impt2++=*Impt5++;
		assert(Impt1 < end1);
		assert(Impt5 < end2);
		*Impt1++=*Impt5++;
	};
#if 1
    Impt1+=width;
    Impt2+=width;
#endif
}
 assert(Impt1 == end1);
 assert(Impt2 == end1+width);
 assert(Impt5 == end2);
}

void ArrayswapLog (unsigned char *Imptr1, unsigned short *Imptr2, short height, short width, unsigned char *end1, unsigned short *end2)
{
    unsigned short     *srcPix, minPix, maxPix;
    int x,y,z;
    double scale;
// Imptr1 points to output array start, Imptr2 points to start of multiplexed image data

unsigned char *Impt1, *Impt2;
unsigned short *Impt5;

            /*
             * Scan for min and max pixel values.
             */
    minPix = 0xFFFF;
    maxPix = 0x0000;
    srcPix = Imptr2;
            for (y = 0; y < height ; y++)
            {
                for (x = 0; x < width; x++, srcPix++)
                {
                	if (*srcPix > maxPix) maxPix = *srcPix;
                	if (*srcPix < minPix) minPix = *srcPix;
                }
            }
            if (maxPix == minPix)
                scale = 0.0;
            else
                scale = 255.0 / (maxPix - minPix);

    printf("MinPix %d, MaxPix %d\n", minPix, maxPix);

Impt1=Imptr1;
Impt2=Imptr1+width;

Impt5=Imptr2;

for (y=1; y <= height/2; y++)
{		// data move
	for (z=1; z <= width/2; z++)
	{
		assert(Impt1 < end1);
		assert(Impt5 < end2);
		*Impt1++=mylog(*Impt5++, minPix, maxPix);
		assert(Impt2 < end1);
		assert(Impt5 < end2);
		*Impt2++=mylog(*Impt5++, minPix, maxPix);
		assert(Impt2 < end1);
		assert(Impt5 < end2);
		*Impt2++=mylog(*Impt5++, minPix, maxPix);
		assert(Impt1 < end1);
		assert(Impt5 < end2);
		*Impt1++=mylog(*Impt5++, minPix, maxPix);
	};
    Impt1+=width;
    Impt2+=width;
}
}
