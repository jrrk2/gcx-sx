/*******************************************************************************
  Copyright(c) 2000 - 2003 Radu Corlan. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information: radu@corlan.net
*******************************************************************************/

// badpix.c: fix bad pixels and columns in frames
// also contains functions for dark/flat processing

// $Revision: 1.1 $
// $Date: 2003/12/01 00:19:37 $

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "ccd.h"


#define BAD_INCREMENT 1024
#define BAD_SIGMAS 6.0


static double median_pixel(float *dp, int w)
{
	double p[8];
	dp --;
	p[0] = *dp++;
	p[1] = *dp++;
	p[2] = *dp;
	dp -= (w + 1);
	p[3] = *dp;
	dp += 2*w;
	p[4] = *dp;
	return dmedian(p, 5);
}

// fix a bad column; frx is the column number
// TODO implement methods
static int fix_column(struct ccd_frame *fr, int frx, int method)
{
	float *dp;
	int i;

	dp = (float *)(fr->dat);
	dp += frx;

	for(i=0; i < fr->h; i++) {
		*dp = (*(dp-1) + *(dp+1)) / 2;
		dp += fr->w;
	}
	return 0;
}


// add_bad_pixel adds a bad pixel to the bad pixel map
static int add_bad_pixel(int x, int y, struct bad_pix_map *map, int type)
{
	void *sp;

// first check we have room to save the new pixel
	if (map->pix == NULL) {
		map->pix = (struct bad_pixel *)(malloc(BAD_INCREMENT * sizeof(struct bad_pixel)));
		if (map->pix == NULL) { // alloc error
			err_printf("error allocating memory for bad pixel map\n");
			return ERR_ALLOC;
		}
		d3_printf("a1\n");
		map->size = BAD_INCREMENT;
	}

	if (map->pixels + 1 > map->size) {
		sp = map->pix;
		map->pix = (struct bad_pixel *)realloc(sp, (BAD_INCREMENT + map->size) *
						       sizeof(struct bad_pixel));
		d3_printf("a2\n");
		if (map->pix == NULL) {
			err_printf("error allocating memory for bad pixel map\n");
			map->pix = sp;
			return ERR_ALLOC;
		}
		map->size += BAD_INCREMENT;
	}
//	d3_printf("add %d\n", 	map->pixels );

// now do the actual save
	(map->pix)[map->pixels].x = x;
	(map->pix)[map->pixels].y = y;
	(map->pix)[map->pixels].type = type;
	map->pixels ++;
//	d3_printf("add %d\n", 	map->pixels );
	return 0;
}

// find_bad_pixels writes a bad pixel file, that contains a listing
// of hot/bright and dark pixels; For each pixel (except a 1-pix border)
// it compares the pixel to the median of it's neighbours; If the difference
// is greater than 'sig' frame csigmas, it is added to the pixel map
// the bad pixel map's size is increased if necessary

int find_bad_pixels(struct bad_pix_map *map, struct ccd_frame *fr, double sig)
{
	int x, y, ret;

	double lo, hi;
	float v;
	float *dp, *dpt;
	int count=0;

	map->pixels = 0;
	free(map->pix);
	map->pix = NULL;


	if (fr->pix_size != 4 || fr->pix_format !=0)
	{
		err_printf("find_bad_pixels: bad frame format\n");
		return ERR_FATAL;
	}

	ret = 0;

	if ( !fr->stats.statsok )
		frame_stats(fr);

	lo = - sig * fr->stats.csigma;
	hi =   sig * fr->stats.csigma;
	dp = (float *)(fr->dat);

	for (x = 1; x < fr->w - 1; x++) {
		dpt = dp + x + fr->w;
		for (y = 1; y < fr->h - 1; y++) {
			v = *dpt - median_pixel(dpt, fr->w);
			if (v > hi)
				ret = add_bad_pixel(x, y, map, BAD_HOT);
			else if (v < lo)
				ret = add_bad_pixel(x, y, map, BAD_DARK);
			if (ret)
				goto error;
			dpt += fr->w;
		}
	}

/*
	for (x=0; x < fr->w; x++) {
		dpt = dp + x;
		for (y=0; y < fr->h; y++) {
			v = *dpt; 
			dpt += fr->w;
			if (v < lo) {
//				d3_printf("l\n");
				ret = add_bad_pixel(x, y, map, BAD_FR);
			}
			if (v > hi) {
//				d3_printf("h\n");
				ret = add_bad_pixel(x, y, map, BAD_HOT);
			}
			if (ret) {
				goto error;
			}
		}
	}
*/

error:
	map->x_skip = fr->x_skip;
	map->y_skip = fr->y_skip;
	map->bin_x = fr->exp.bin_x;
	map->bin_y = fr->exp.bin_y;

	info_printf("found %d (%d) bad pixels\n", map->pixels, count);
	return ret;

}

// save_bad_pix saves a bad pixel map to a file. The file
// starts with a header containing the frame skip, bin and number of
// bad pixels/columns 

int save_bad_pix(struct bad_pix_map *map, char *filename)
{
	char lb[81];
	FILE *fp;
	int i;

	lb[80] = 0;

	strncpy(lb, filename, 70);

	if (strlen(lb) <= 7 || strcmp(lb + strlen(lb) - 7, ".badpix") != 0)
		strcat(lb, ".badpix");

	fp = fopen(lb, "w");
	if (fp == NULL) {
		err_printf("save_bad_pix: Cannot open file: %s for writing\n",
			filename);
		return (ERR_FILE);
	}

	fprintf(fp, "pixels %d\n", map->pixels);
	fprintf(fp, "x_skip %d\n", map->x_skip);
	fprintf(fp, "y_skip %d\n", map->y_skip);
	fprintf(fp, "bin_x %d\n", map->bin_x);
	fprintf(fp, "bin_y %d\n", map->bin_y);
	for (i=0; i<map->pixels; i++) 
		fprintf(fp, "%c %d %d\n", map->pix[i].type, map->pix[i].x, map->pix[i].y);
	fclose (fp);
	return 0;
}

// load_bad_pix loads a bad pixel file; if the map structure supplied
// already has a data table attached to it, it is freed first

int load_bad_pix(struct bad_pix_map *map, char *filename)
{
	char lb[81];
	FILE *fp;
	int i, ret, pixels, count;

	lb[80] = 0;

	strncpy(lb, filename, 70);

	if (strlen(lb) <= 7 || strcmp(lb + strlen(lb) - 7, ".badpix") != 0)
		strcat(lb, ".badpix");

	fp = fopen(lb, "r");
	if (fp == NULL) {
		err_printf("load_bad_pix: Cannot open file: %s for reading\n",
			filename);
		return (ERR_FILE);
	}

	count = 0;

	if (map->pixels != 0) 
		free(map->pix);

	ret = fscanf(fp, "pixels %d", &pixels);
	if (ret != 1)
		goto bad_format;
	ret = fscanf(fp, " x_skip %d", &map->x_skip);
	if (ret != 1)
		goto bad_format;
	ret = fscanf(fp, " y_skip %d", &map->y_skip);
	if (ret != 1)
		goto bad_format;
	ret = fscanf(fp, " bin_x %d", &map->bin_x);
	if (ret != 1)
		goto bad_format;
	ret = fscanf(fp, " bin_y %d", &map->bin_y);
	if (ret != 1)
		goto bad_format;

	map->pix = (struct bad_pixel *)(calloc(pixels, sizeof(struct bad_pixel)));
	if (map->pix == NULL) {
		err_printf("load_bad_pix: cannot alloc memory for bad pixels map\n");
		return ERR_ALLOC;
	}
	map->size = pixels;

	for (i=0; i<pixels; i++) {
		ret = fscanf(fp, " %c %d %d", &map->pix[i].type, &map->pix[i].x, &map->pix[i].y);
		if (ret != 3)
			break;
	}
	if (i != pixels)
		err_printf("load_bad_pix: file is short");
	map->pixels = i;
	info_printf("loaded %d bad pixels\n", map->pixels);
	return 0;


bad_format:
	fclose(fp);
	err_printf("load_bad_pix: invalid file format\n");
	return ERR_FATAL;

}

// find bad neighbours of a (bad) pixel
#define BAD_L 1
#define BAD_R 2
#define BAD_A 4
#define BAD_B 8

static int bad_neighbours(struct bad_pix_map *map, int pixel, int bin_x, int bin_y)
{
	int i;
	int bn = 0;
	int bx, by, bxbr, bybr;

	bx = map->pix[pixel].x;
	by = map->pix[pixel].y;
	bxbr = map->pix[pixel].x + bin_x - 1;
	bybr = map->pix[pixel].y + bin_y - 1;
//	d3_printf("pix %d\n", );

// first we look to the left and above the current pixel
	if (pixel != 0)
		for (i=pixel - 1; i >= 0; i--) {
			if (bx - map->pix[i].x > bin_x) 
				// we are more that 1 pixel away, stop searching
				break;
			if (by - map->pix[i].y < bin_y  && bx - map->pix[i].x <= bin_x) 
				//found one to the left 
				bn |= BAD_L;
			if (bx - map->pix[i].x > bin_x && by - map->pix[i].y <= bin_y) 
				//found one above 
				bn |= BAD_A;
		}

//	d3_printf("nl\n");
// next we look to the right and below
	if (pixel < map->pixels - 1)
		for (i=pixel; i <map->pixels; i++) {
			if (bxbr - map->pix[i].x < -bin_x ) 
				// we are more that 1 pixel away, stop searching
				break;
			if (bybr - map->pix[i].y > -bin_y && bxbr - map->pix[i].x >= -bin_x) 
				//found one to the right 
				bn |= BAD_R;
			if (bxbr == map->pix[i].x > -bin_x && bybr - map->pix[i].y >= -bin_y) 
				//found one below
				bn |= BAD_B;
		}	
//track 	d3_printf("nr\n");

	return bn;
}

// replace a pixel with the average of it's neighbours
static void fix_pixel(struct ccd_frame *fr, int x, int y, int bn)
{
	double v = 0.0;
	int n = 0;
	float *pp;

	if (x < 0) 
		x = 0;
	else if (x >= fr->w) 
		x = fr->w - 1; 
	if (y < 0) 
		y = 0;
	else if (y >= fr->h) 
		y = fr->h - 1; 

	pp = ((float *)fr->dat) + x + y * fr->w;

	if (x > 0 && (bn & BAD_L) == 0) {
		v += *(pp - 1);
		n++;
	}
	if (x < fr->w - 1 && (bn & BAD_R) == 0) {
		v += *(pp + 1); 
		n++;
	}
	if (y > 0 && (bn & BAD_A) == 0) {
		v += *(pp - fr->w);
		n++;
	}
	if (y < fr->h - 1 && (bn & BAD_B) == 0) {
		v += *(pp + fr->w);
		n++;
	}
	if (n > 0)
		*pp = v / n;
//	if (n < 2)
//		d3_printf("n=%d\n", n);
}

// fix_bad_pixels will interpolate the bad pixels of a frame from their good first-order 
// neighbours. method is an 'or' of the methods required for bad pixel and bad column
// fixes
int fix_bad_pixels (struct ccd_frame *fr, struct bad_pix_map *map, int method)
{
	int i;
	int bn;
	int frx, fry;
	float *dp;

//	if (map->bin_x != 1 || map->bin_y != 1) 
//		/*if (map->bin_x != fr->exp.bin_x || map->bin_y != fr->exp.bin_y) */{
//			err_printf("fix_bad_pixels: incompatible binning\n");
//			return ERR_FATAL;
//		}

//	d3_printf("Frame skips: %d %d\n", fr->x_skip, fr->geom.y_skip);

	for (i=0; i<map->pixels; i++) {
		if (fr->exp.bin_y != 0 && fr->exp.bin_x != 0) {
			frx = map->pix[i].x - fr->x_skip / fr->exp.bin_x;
			fry = map->pix[i].y - fr->y_skip / fr->exp.bin_y;
		} else {
			frx = map->pix[i].x;
			fry = map->pix[i].y;
		}
		if (frx > 0 && frx < fr->w - 1 && fry > 0 && fry < fr->h - 1) {
			if (map->pix[i].type == BAD_COLUMN) {
				fix_column(fr, frx, method);
			} else if (method & BADPIX_MEDIAN) {
				dp = (float *)(fr->dat) + frx + fry * fr->w;
				*dp = median_pixel(dp, fr->w);
			} else {
				bn = bad_neighbours(map, i,  fr->exp.bin_x, fr->exp.bin_y);
				fix_pixel(fr, frx, fry, bn);
			}
		}
	}

	return 0;
}



