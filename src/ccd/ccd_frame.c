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

/*  ccd_frame.c: frame operation functions */
/*  $Revision: 1.13 $ */
/*  $Date: 2004/11/19 19:01:22 $ */

/* Many functions here assume we have float frames - this must be fixed */

#define _GNU_SOURCE

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "ccd.h"
//#include "cpxcon_regs.h"


#define MAX_FLAT_GAIN 1.5	// max gain accepted when flatfielding
#define FITS_HROWS 36	// number of fits header lines in a block

static void free_frame(struct ccd_frame *fr);
//static void free_frame_data(struct ccd_frame *fr);
static struct ccd_frame *new_frame_fr(struct ccd_frame* fr, unsigned size_x, unsigned size_y);
static struct ccd_frame *new_frame_head_fr(struct ccd_frame* cam, unsigned size_x, unsigned size_y);
static int alloc_frame_data(struct ccd_frame *fr);
static int frame_to_float(struct ccd_frame *fr);


// new frame creates a frame of the specified size
struct ccd_frame *new_frame(unsigned size_x, unsigned size_y)
{
	return new_frame_fr(NULL, size_x, size_y);
}

// clone_frame produces a newly allocced copy of a frame
struct ccd_frame *clone_frame(struct ccd_frame *fr)
{
	struct ccd_frame *new;
	int all;

	new = new_frame_fr(fr, 0, 0);
	if (new == NULL)
		return NULL;
// now copy the data
//	d3_printf("nffr\n");
	all = new->w * new->h;
	memcpy(new->dat, fr->dat, all * fr->pix_size);
	new->ref_count = 1;
	return new;
}

// new_frame_fr creates and allocates a frame copying it's geometry from
// a given frame; when non-zero, the size params overwrite the default.
// returns a pointer to the new image header, or NULL if the request falls
// magic is copied from the original
static struct ccd_frame *new_frame_fr(struct ccd_frame* fr, unsigned size_x, unsigned size_y)
{
	struct ccd_frame *new;
	void *data;

	new = new_frame_head_fr(fr, size_x, size_y);
	if (new == NULL)
		return NULL;
//	d3_printf("nfrhfr\n");

	data = malloc(new->w * 
		      new->h * 
		      new->pix_size);

	if (data == NULL) {
		free(new);
		return NULL;
	}
	new->dat = data;
	return new;
}

// new_frame_head_fr is the same as new_frame_fr, only it does not allocate space for the
// data array, only for the header
// if the fr parameter is NULL, the function creates a frame header with only
// the magic and geometry fields set. It assumes 16bits/pix.
static struct ccd_frame *new_frame_head_fr(struct ccd_frame* fr, unsigned size_x, unsigned size_y)
{
	struct ccd_frame *hd;
	void *var;

	hd = calloc(1, sizeof(struct ccd_frame) );
	if (hd == NULL)
		return NULL;

	if (fr != NULL) {
		memcpy(hd, fr, sizeof(struct ccd_frame) );
	} 
	hd->pix_size = DEFAULT_PIX_SIZE;

	if (hd->nvar) { //we need to alloc space for variables
		var = calloc(hd->nvar * sizeof(FITS_row), 1);
		if (var == NULL) {
			free(hd);
			return NULL;
		}
		memcpy(var, hd->var, hd->nvar * sizeof(FITS_row));
		hd->var = var;
	} else {
		hd->var = NULL;
		hd->nvar = 0;
	}

	hd->stats.hist.hdat = NULL;
	hd->stats.statsok = 0;

	hd->magic = UNDEF_FRAME;
	if (size_x != 0)
		hd->w = size_x;
	if (size_y != 0)
		hd->h = size_y;
	if (!hd->fim.wcsset) {
		hd->fim.xref = 0.0;
		hd->fim.yref = 0.0;
		hd->fim.xrefpix = 0.0;
		hd->fim.yrefpix = 0.0;
		hd->fim.xinc = -0.000263;
		hd->fim.yinc = -0.000263;
		hd->fim.rot = 0.0;
	}
//	if (!hd->fim.bsset) {
//		hd->fim.bscale = 1.0;
//		hd->fim.bzero = 0.0;
//	}
	if (fr == NULL) {
		hd->exp.scale = 1.0;
		hd->exp.bias = 0.0;
		hd->exp.rdnoise = 1.0;
		hd->exp.flat_noise = 0.0;
	}
	hd->ref_count = 1;
	hd->pix_format = PIX_FLOAT;
//	d3_printf("new_head_fr returns\n");
	return hd;
}


// free_frame frees a frame completely (data array and header)
static void free_frame(struct ccd_frame *fr)
{
/*  	d3_printf("Freeing frame at %08x\n", fr); */
	if (fr) {
		free(fr->var);
		free(fr->stats.hist.hdat);
		free(fr->dat);
		free(fr);
	}
}

// free_frame_data frees the data array of a frame
/*
static void free_frame_data(struct ccd_frame *fr)
{
	free(fr->dat);
	free(fr->stats.hist.hdat);
	fr->stats.hist.hdat = NULL;
	fr->dat = NULL;
	fr->data_valid = 0;
}*/


// functions should call get_frame to show
// the frame will still be needed after they return
void get_frame(struct ccd_frame *fr)
{
/*  	d3_printf("get_frame: frame at %08x has refcount of %d\n", fr, fr->ref_count); */
	fr->ref_count ++;
}


// decrement usage count and free frame if it reaches 0;
void release_frame(struct ccd_frame *fr)
{
//  	d3_printf("release_frame: frame at %08x has refcount of %d\n", fr, fr->ref_count); 
	if (fr->ref_count > 0) {
		fr->ref_count --;
	} else {
		err_printf("release_frame: warning: called with ref_count = 0 \n");
	}
	if (fr->ref_count <= 0)
		free_frame(fr);
}

// alloc_frame_data allocated the data area for a frame according to the 
// parameters in the supplied header; returns ERR_ALLOC if it cannot allocate the space, 0 for ok.
static int alloc_frame_data(struct ccd_frame *fr)
{
	void *data;

	data = malloc(fr->w * 
		      fr->h * 
		      fr->pix_size);

	if (data == NULL) {
		return ERR_ALLOC;
	} else {
		fr->dat = data;
		return 0;
	}


}

// frame_stats fills the statistics buffer for the frame
#define H_MIN (0) // leftmost bin value
#define HIST_OFFSET (-H_START) // offset to accomodate small negative values in histogram
#define H_MAX (H_SIZE) // rightmost bin value

int frame_stats(struct ccd_frame *hd)
{
	double sum, sumsq, min, max;
	int i, is = 0, ie;
	unsigned all, s, e;
	float *data;
	float v;
	unsigned hsize;
	double hmin, hstep;
	unsigned binmax;
	unsigned *hdata;
	int b, n;
	double cs;
	double median = 0.0, c, bv;
	int ret;

	if (hd->pix_format != PIX_FLOAT) {
		d3_printf("frame_stats: converting frame to float format!\n");
		ret = frame_to_float(hd);
		if (ret < 0) {
			err_printf("error converting \n");
			return ret;
		}
	}

	if (hd->stats.hist.hdat == NULL) { // we need to allocate the histogram data
		if (hd->stats.hist.hsize == 0)
			hd->stats.hist.hsize = H_MAX - H_MIN;
		hd->stats.hist.hdat = (unsigned *)calloc(hd->stats.hist.hsize, sizeof(unsigned));
		if (hd->stats.hist.hdat == NULL) {
			err_printf("frame_stats: histogram alloc failed\n");
			return ERR_ALLOC;
		}
	}

	hdata = hd->stats.hist.hdat;

	hsize = hd->stats.hist.hsize;
	hmin = H_MIN;
	hstep = (H_MAX - H_MIN) / hsize;
	binmax = 0;

	sum = 0.0;
	sumsq = 0.0;

// do the reading and calculate stats
	all = hd->w * (hd->h); 
	data = (float *)hd->dat;
	min = *data;
	max = *data;

	for (i = 0; i < all; i++) {
		v = *data++;
		if (isnan(v))
			d3_printf("found NaN at %d", i);
//		if (v < -HIST_OFFSET)
//			v = -HIST_OFFSET;
//		if (v > H_MAX)
//			v = H_MAX;
		b = HIST_OFFSET + floor( (v - hmin) / hstep );
		if (b < 0)
			b = 0;
		if (b >= hsize)
			b = hsize - 1;
		hdata[b] ++;
		if (hdata[b] > binmax)
			binmax = hdata[b];
		if (v > max) {
			max = v;
//			d3_printf("max = %.1f", max);
		}
		else if (v < min) {
			min = v;
//			d3_printf("min = %.1f", min);
		}
		sum += v;
		sumsq += v * v;
	}

	hd->data_valid = 1;
//	hd->magic = UNDEF_FRAME;
	hd->stats.min = min;
	hd->stats.max = max;
	hd->stats.avg = sum / (1.0 * all);
	hd->stats.sigma = sqrt(sumsq / (1.0 * all) - hd->stats.avg * hd->stats.avg);
	hd->stats.hist.binmax = binmax;
	hd->stats.hist.st = H_MIN - HIST_OFFSET;
	hd->stats.hist.end = H_MAX - HIST_OFFSET;

// scan the histogram to get the median, cavg and csigma

	sum = 0.0;
	sumsq = 0.0;
	b=0;
	n=0;

	bv = hmin - HIST_OFFSET;
	s = all / POP_CENTER;
	e = all - all / POP_CENTER;
	c = all / 2;
	for (i = 0; i < hsize; i++) {
		b += hdata[i];
		bv += hstep;
		if (b < s) {
			is = i;
			continue;
		}
		if (b > e) {
			ie = i;
			break;
		}
		if (b - hdata[i] < c && b >= c) { // we are at the median point
			if (hdata[i] != 0) { // get the interpolated median
				median = (b - c) / hdata[i] * hstep + i + hmin - HIST_OFFSET;
//				info_printf("M: %.3f\n", median);
			} else {
				median = (i - 0.5) * hstep + hmin - HIST_OFFSET;
			}
		}
		n += hdata[i];
		sum += hdata[i] * bv;
		sumsq += hdata[i] * bv * bv;
	}
	i = is;
	hd->stats.hist.cst = i;
	cs = hmin + hstep * i - HIST_OFFSET;

//	info_printf("new cavg %.3f csigm %.3f median %.3f, sigma, %.3f ", sum / n, 
//		    sqrt(sumsq / n - sqr(sum / n)), median, hd->stats.sigma);

	if (n != 0) {
		hd->stats.cavg = sum / n;
		hd->stats.csigma = 2 * sqrt(sumsq / n - sqr(sum / n));
	} else {
		hd->stats.cavg = hd->stats.avg;
		hd->stats.csigma = hd->stats.sigma;
	}
	hd->stats.median = median;

	hd->stats.statsok = 1;
//	info_printf("min=%.2f max=%.2f avg=%.2f sigma=%.2f cavg=%.2f csigma=%.2f\n", 
//		    hd->stats.min, hd->stats.max, hd->stats.avg, hd->stats.sigma, 
//		    hd->stats.cavg, hd->stats.csigma);
	return 0;
}

// check if a filename has a valid fits extension, or append one if it doesn't
// do not exceed fnl string length
// return 1 if the name was changed, -1 if the file is a .gz, .zip or .Z

static int fits_filename(char *fn, int fnl)
{
	int len;
	len = strlen(fn);
	if (len + 5 >= fnl)
		return 0;
	if (strcasecmp(fn + len - 5, ".fits") == 0) 
		return 0;
	if (strcasecmp(fn + len - 4, ".fit") == 0) 
		return 0;
	if (strcasecmp(fn + len - 4, ".fts") == 0) 
		return 0;
	if (strcasecmp(fn + len - 3, ".gz") == 0) {
//		d3_printf("we are a gzzzzzz\n");
		return -1;
	}
	if (strcasecmp(fn + len - 2, ".z") == 0) 
		return -1;
	if (strcasecmp(fn + len - 4, ".zip") == 0) 
		return -1;
	strcat(fn, ".fits");
	return 1;
}

/* read the wcs fields from the fits header lines
static void parse_fits_wcs(struct ccd_frame *fr, struct wcs *fim)
{
	fim->wcsset = WCS_INITIAL;

	if (fits_get_double(fr, "CRPIX1", &fim->xrefpix) <= 0)
		fim->wcsset = WCS_INVALID;
	if (fits_get_double(fr, "CRPIX2", &fim->yrefpix) <= 0)
		fim->wcsset = WCS_INVALID;
	if (fits_get_double(fr, "CRVAL1", &fim->xref) <= 0)
		fim->wcsset = WCS_INVALID;
	if (fits_get_double(fr, "CRVAL2", &fim->yref) <= 0)
		fim->wcsset = WCS_INVALID;
	if (fits_get_double(fr, "CDELT1", &fim->xinc) <= 0)
		fim->wcsset = WCS_INVALID;
	if (fits_get_double(fr, "CDELT2", &fim->yinc) <= 0)
		fim->wcsset = WCS_INVALID;
	if (fits_get_double(fr, "CROTA1", &fim->rot) <= 0)
		fim->rot = 0;
	if (fits_get_int(fr, "EQUINOX", &fim->equinox) <= 0) {
		fim->equinox = 2000;
	}
}
 */

/* read the wcs fields from the fits header lines

static void parse_fits_exp(struct ccd_frame *fr, struct exp_data *exp)
{

	if (fits_get_int(fr, "CCDBIN1", &exp->bin_x) > 0)
		exp->datavalid = 1;
	if (fits_get_int(fr, "CCDBIN2", &exp->bin_y) > 0)
		exp->datavalid = 1;
	if (fits_get_double(fr, "ELADU", &exp->scale) <= 0) {
		exp->scale = 1.0;
//		err_printf("No ELADU, assuming 1\n");
	}
	if (fits_get_double(fr, "RDNOISE", &exp->rdnoise) <= 0) {
		exp->rdnoise = 7.0;
//		err_printf("No RDNOISE, assuming 7\n");
	}
	if (fits_get_double(fr, "FLNOISE", &exp->flat_noise) <= 0) {
		exp->flat_noise = 0.0;
//		err_printf("No FLNOISE, assuming 0\n");
	}
	if (fits_get_double(fr, "DCBIAS", &exp->bias) <= 0) {
		exp->bias = 0.0;
//		err_printf("No DCBIAS, assuming 0\n");
	}
}
 */

#define MAX_FILENAME 1024
// read_fits_file reads a fits file from disk and creates a new frame 
// holding the data from the file. 
static struct ccd_frame *read_fits_file_from_fd(FILE *fp, char *fn, int force_unsigned)
{
	char lb[FITS_HCOLS + 1];
	short v[FITS_HCOLS * FITS_HROWS / 2];
	short ds;
	unsigned short du;
	int i, j, k, ef;
	unsigned all;
	unsigned naxis;
	struct ccd_frame* hd;
	float *data;
	FITS_row *cp;

	double bz, bs;
	float bscale = 0.0, bzero = 0.0;
	int bsset = 0;
	float d;

	ef = 0;
	lb[80] = 0;


//now allocate the header for the new frame
	hd = new_frame_head_fr(NULL, 0, 0);
		if (hd == NULL) {
			err_printf("read_fits_file_from_fd: error creating header\n");
			return NULL;
		}
	for (i = 0; i < 6; i++)	{// at most 6 header blocks
		for (j = 0; j < FITS_HROWS; j++) {	// 36 cards per block
			for (k = 0; k < FITS_HCOLS; k++)	// chars per card
				lb[k] = fgetc(fp);
// now parse the fits header lines
			if (strncmp(lb, "END", 3) == 0) 
				ef = 1;
			else if (sscanf(lb, "NAXIS = %d", &naxis) ) 
				;
			else if (sscanf(lb, "NAXIS1 = %d", &hd->w) ) 
				;
			else if (sscanf(lb, "NAXIS2 = %d", &hd->h) ) 
				;
			else if (sscanf(lb, "BITPIX = %d", &hd->pix_size) ) 
				;
			else if (sscanf(lb, "BSCALE = %f", &d) ) {
				bsset = 1;
				bscale = d;
			}
			else if (sscanf(lb, "BZERO = %f", &d) ) {
				bsset = 1;
				bzero = d;
			}
			else if (!strncmp(lb, "SIMPLE", 6) ) 
				;
			else if (!ef) { //add the line to the unprocessed list
				cp = realloc(hd->var, (hd->nvar + 1) * FITS_HCOLS);
				if (cp == NULL) {
					err_printf("cannot alloc space for fits header line, skipping\n");
				} else {
					hd->var = cp;
					memcpy(cp + hd->nvar, lb, FITS_HCOLS);
//x					d3_printf("adding generic line:\n%80s\n", lb);
					hd->nvar ++;
				}
			}
		}
		if (ef == 1)
			break;
	}

	if (ef == 0) {
		err_printf("Bad FITS format; cannot find END keyword\n");
		goto err_exit;
	}

// checking header data
	if (naxis != 2) {
		err_printf("bad NAXIS = %d (!= 2)\n", naxis);
		goto err_exit;
	}
	if (hd->pix_size == 16) {
		hd->pix_size = 4;
	} else {
		err_printf("bad BITPIX = %d (must be 16)\n", hd->pix_size);
		goto err_exit;
	}
	if (hd->w <= 0) {
		err_printf("bad NAXIS1 = %d \n", hd->w);
		goto err_exit;
	}
	if (hd->h <= 0) {
		err_printf("bad NAXIS2 = %d \n", hd->h);
		goto err_exit;
	}

	if (alloc_frame_data(hd)) {
		err_printf("read_fits_file_from_fd: cannot allocate data for frame\n");
		goto err_exit;
	}

	//	info_printf("Reading FITS file: %s %d x %d x %d\n", 
	//		    fn, hd->w, hd->h, 8*hd->pix_size);

// check for any required scaling/shifting
	if (bsset) {
		if (bscale == 0.0) {
			err_printf("bad BSCALE (0) - using 1.0\n");
			bscale = 1.0;
		}
		bz = bzero;
		bs = bscale;
	} else {
		bz = 0;
		bs = 1.0;
	}

// do the reading and calculate stats
	all = hd->w * hd->h; 
	data = (float *)(hd->dat);
	j=0;

	while(j < all) {
		k = fread (v, 1, FITS_HCOLS * FITS_HROWS, fp);
		if (k < 0) {
			err_printf("data is short!\n");
			break;
		} else {
			if (force_unsigned && bz == 0 ){
				for(i = 0; i < FITS_HCOLS * FITS_HROWS / 2 && j < all; i++) {
					du = (((v[i] >> 8) & 0x0ff) | ((v[i] << 8) & 0xff00));
					*data++ = bs * du;
					j++;
				}
			} else {
				for(i = 0; i < FITS_HCOLS * FITS_HROWS / 2 && j < all; i++) {
					ds = (((v[i] >> 8) & 0x0ff) | ((v[i] << 8) & 0xff00));
					*data++ = bs * ds + bz;
					j++;
				}
			}
		}
	}

//	d3_printf("B");


	hd->data_valid = 1;
	hd->magic = UNDEF_FRAME;

	frame_stats(hd);

//	info_printf("min=%.2f max=%.2f avg=%.2f sigma=%.2f\n", 
//		    hd->stats.min, hd->stats.max, hd->stats.avg, hd->stats.sigma);

	strncpy(hd->name, fn, 255);
//	parse_fits_wcs(hd, &hd->fim);
//	parse_fits_exp(hd, &hd->exp);

	if (fits_get_int(hd, "CCDSKIP1", &hd->x_skip) <= 0)
		hd->x_skip = 0;
	if (fits_get_int(hd, "CCDSKIP2", &hd->y_skip) <= 0)
		hd->y_skip = 0;

	fclose(fp);
//	d3_printf("C");
	return hd;

err_exit: 
	free_frame(hd);
	return NULL;
}


/* entry points for read_fits. read_fits_file is provided for compatibility */
struct ccd_frame *read_fits_file(char *filename, int force_unsigned)
{
	char fn[MAX_FILENAME + 1];
	FILE *fp;
	int zipped;

	strncpy(fn, filename, MAX_FILENAME);
	zipped = (fits_filename(fn, MAX_FILENAME) < 0) ;
	if (zipped) {
		err_printf("cannot open non-fits file\n");
		return NULL;
	}
	fp = fopen(fn, "r");
	if (fp == NULL) {
		err_printf("\nCannot open file: %s\n", fn);
		return NULL;
	}

	return read_fits_file_from_fd(fp, fn, force_unsigned);
}

/* read from file, but using 'popen ungzcmd' if the file appears to be compressed */
/* a typical value for ungz would be 'zcat ' */
struct ccd_frame *read_gz_fits_file(char *filename, char *ungz, int force_unsigned)
{
	char fn[MAX_FILENAME + 1];
	FILE *fp;
	int zipped;
	char *cmd;

	strncpy(fn, filename, MAX_FILENAME);
	zipped = (fits_filename(fn, MAX_FILENAME) < 0) ;
	if (zipped && ungz == NULL) {
		err_printf("cannot open non-fits file\n");
		return NULL;
	}
	if (zipped) {
		asprintf(&cmd, "%s %s ", ungz, fn);
		fp = popen(cmd, "r");
		free(cmd);
		if (fp == NULL) {
			err_printf("Cannot open/unzip file: %s with %s\n", fn, ungz);
			return NULL;
		}
	} else {
		fp = fopen(fn, "r");
		if (fp == NULL) {
			err_printf("Cannot open file: %s\n", fn);
			return NULL;
		}
	}
	return read_fits_file_from_fd(fp, fn, force_unsigned);
}

// write a frame to disk as a fits file 

int write_fits_frame(struct ccd_frame *fr, char *filename)
{
	char lb[MAX_FILENAME];
	FILE *fp;
	int i, j, k, v;
	unsigned all;
//	struct tm *t;
	float *dp;
//	int jdi;
//	double jd;
	double bscale, bzero;

	lb[80] = 0;

	strncpy(lb, filename, MAX_FILENAME);

	fits_filename(lb, MAX_FILENAME);

	fp = fopen(lb, "w");
	if (fp == NULL) {
		err_printf("\nwrite_fits_frame: Cannot open file: %s for writing\n",
			filename);
		return (ERR_FILE);
	}
/*
	t = gmtime(&(fr->exp.exp_start.tv_sec));
	sprintf(lb, "'%d-%02d-%02dT%02d:%02d:%02d.%02d'", 1900 + t->tm_year, t->tm_mon + 1, 
		t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, 
		(int)(fr->exp.exp_start.tv_usec / 10000));
// do the julian date (per hsaa p107)
	i = 1900 + t->tm_year;
	j = t->tm_mon + 1;
	k = t->tm_mday;

	jdi = k - 32075 + 1461 * (i + 4800 + (j - 14) / 12) / 4
		+ 367 * (j - 2 - (j - 14) / 12 * 12) / 12
		- 3 * (( i + 4900 + (j - 14) / 12) / 100) / 4;

	jd = jdi - 0.5 + t->tm_hour / 24.0 + t->tm_min / (24.0 * 60.0) 
		+ t->tm_sec / (24.0 * 3600.0) + fr->exp.exp_start.tv_usec / 
		(1000000.0 * 24.0 * 3600.0);
*/

	if (fr->stats.statsok == 0)
		frame_stats(fr);

	i = 0;
	i++; fprintf(fp, "%-8s= %20s / %-40s       ", "SIMPLE", "T",
		"Standard FITS format");
	i++; fprintf(fp, "%-8s= %20d / %-40s       ", "BITPIX", 16,
		"Bits per pixel");
	i++; fprintf(fp, "%-8s= %20d   %-40s       ", "NAXIS", 2, "");
	i++; fprintf(fp, "%-8s= %20d   %-40s       ", "NAXIS1", fr->w, "");
	i++; fprintf(fp, "%-8s= %20d   %-40s       ", "NAXIS2", fr->h, "");
//	if (fr->exp.datavalid) {
//		i++; fprintf(fp, "%-8s= %-20s   %-40s       ", "TIMESYS", "'TT'", "");
//		i++; fprintf(fp, "%-8s= %-25s   %-35s       ", "DATE-OBS", lb, 
//			     "UTC OF INTEGRATION START");
//	}


	if ((fr->stats.max - fr->stats.min) < 32767.0) {// we use positive, scaled by 1 format
		bscale = 1.0;
		bzero = 0.0;
	} else {
		bscale = 1.0;
		bzero = 32768.0;
	}

	i++; fprintf(fp, "%-8s= %20.3f   %-40s       ", "BSCALE", bscale, "");
	i++; fprintf(fp, "%-8s= %20.3f   %-40s       ", "BZERO", bzero, "");

// fill in /replace the noise pars

// finally, print the rest of the header lines

	for (j=0; j < fr->nvar; j++) {
		for (k=0; k<FITS_HCOLS; k++)
			fputc(fr->var[j][k], fp);
		i++;
	}
		
	i++; fprintf(fp, "%-8s  %20s   %-40s       ", "END", "", "");

	k = FITS_HROWS * (i / FITS_HROWS) - i;
	if (k < 0) 
		k += FITS_HROWS;

	for (j = 0; j < k; j++)
		fprintf(fp, "%80s", "");
//	d3_printf("i=%d j=%d", i, j);

	if (fr->pix_size != 4 || fr->pix_format != PIX_FLOAT) {
		err_printf("\nwrite_fits_frame: I can only write float frames\n");
		return ERR_FATAL;
	}

	all = fr->w * fr->h;
	dp = (float *)(fr->dat);

//real value = bzero + bscale * <array_value>

	for (i = 0; i < all; i ++) {
		v = floor( (dp[i] - bzero) / bscale );
		if (v < -32768)
			v = -32768;
		if (v > 32767)
			v = 32767;
		putc((v >> 8) & 0xff, fp);
		putc((v) & 0xff, fp);
	}

/* pad the end of record */
	for (i = 0;  i < 2880 - (all * 2) % 2880; i++)		
		putc(' ', fp);
	fflush(fp);
	fsync(fileno(fp));
	fclose(fp);
	return 0;
}

int write_gz_fits_frame(struct ccd_frame *fr, char *fn, char *gzcmd)
{
	char *text;
	char lb[MAX_FILENAME];
	int ret;

	if (gzcmd == NULL) 
		return write_fits_frame(fr, fn);

	strncpy(lb, fn, MAX_FILENAME);
	fits_filename(lb, MAX_FILENAME);

	ret = write_fits_frame(fr, lb);
	if (!ret) {
		asprintf(&text, "%s %s", gzcmd, lb);
		system(text);
		free(text);
	}
	return ret;
}



// flat_frame divides ff by (fr1 / cavg(fr1) ; the two frames are aligned according to their skips
// the size of fr is not changed

int flat_frame(struct ccd_frame *fr, struct ccd_frame *fr1)
{
	int xovlap, yovlap;
	int xst, yst;
	int x1st, y1st;
	float *dp, *dp1;
	int x, y;
	double mu, ll;

	if (!fr1->stats.statsok)
		frame_stats(fr1);
	mu = fr1->stats.cavg;
	if (mu <= 0.0) {
		err_printf("flat frame has negative avg: %.2f aborting\n", mu);
		return -1;
	}

	if (fabs(fr->exp.bias) > 2.0) {
		err_printf("I refuse to flat a frame with non-null bias (%.f)\n", fr->exp.bias);
		return -1;
	}
	if (fabs(fr1->exp.bias) > 2.0) {
		err_printf("I refuse to use a flat with non-null bias (%.f)\n", fr1->exp.bias);
		return -1;
	}

	ll = mu / MAX_FLAT_GAIN;

	xst = fr1->x_skip - fr->x_skip;
	if (fr1->x_skip + fr1->w >= fr->w + fr->x_skip)
		xovlap = fr->x_skip + fr->w - fr1->x_skip;
	else
		xovlap = fr1->x_skip + fr1->w - fr->x_skip;
	if (xst < 0) {
		x1st = -xst; 
		xst = 0;
	} else 
		x1st = 0;

	yst = fr1->y_skip - fr->y_skip;
	if (fr1->y_skip + fr1->h >= fr->h + fr->y_skip)
		yovlap = fr->y_skip + fr->h - fr1->y_skip;
	else
		yovlap = fr1->y_skip + fr1->h - fr->y_skip;
	if (yst < 0) {
		y1st = -yst; 
		yst = 0;
	} else 
		y1st = 0;

//	d3_printf("xst %d x1st %d yst %d x1st %d xovlap %d yovlap %d",
//		  xst, x1st, yst, y1st, xovlap, yovlap);

	dp = (float *)(fr->dat) + xst + yst * fr->w;
	dp1 = (float *)(fr1->dat) + x1st + y1st * fr1->w;

	for (y = 0; y < yovlap; y++) {
		for (x = 0; x < xovlap; x++) { 
			if (*dp1 > ll)  
				*dp = *dp / *dp1 * mu;
			else 
				*dp = *dp * MAX_FLAT_GAIN;
			dp ++;
			dp1 ++;
		}
		dp += fr->w - xovlap;
		dp1 += fr1->w - xovlap;
	}
	fr->stats.statsok = 0;

	fr->exp.flat_noise = sqrt( sqr(fr1->exp.rdnoise) + mu / sqrt(fr1->exp.scale) ) / mu;

// TODO  take care of biases
	return 0;
}


// add_frames adds fr1 to fr; the two frames are aligned according to their skips
// the size of fr is not changed

int add_frames (struct ccd_frame *fr, struct ccd_frame *fr1)
{
	int xovlap, yovlap;
	int xst, yst;
	int x1st, y1st;
	float *dp, *dp1;
	int x, y;

	xst = fr1->x_skip - fr->x_skip;
	if (fr1->x_skip + fr1->w >= fr->w + fr->x_skip)
		xovlap = fr->x_skip + fr->w - fr1->x_skip;
	else
		xovlap = fr1->x_skip + fr1->w - fr->x_skip;
	if (xst < 0) {
		x1st = -xst; 
		xst = 0;
	} else 
		x1st = 0;

	yst = fr1->y_skip - fr->y_skip;
	if (fr1->y_skip + fr1->h >= fr->h + fr->y_skip)
		yovlap = fr->y_skip + fr->h - fr1->y_skip;
	else
		yovlap = fr1->y_skip + fr1->h - fr->y_skip;
	if (yst < 0) {
		y1st = -yst; 
		yst = 0;
	} else 
		y1st = 0;

//	d3_printf("xst %d x1st %d yst %d x1st %d xovlap %d yovlap %d",
//		  xst, x1st, yst, y1st, xovlap, yovlap);

	dp = (float *)(fr->dat) + xst + yst * fr->w;
	dp1 = (float *)(fr1->dat) + x1st + y1st * fr1->w;

	for (y = 0; y < yovlap; y++) {
		for (x = 0; x < xovlap; x++) { 
			*dp = *dp + *dp1;
			dp ++;
			dp1 ++;
		}
		dp += fr->w - xovlap;
		dp1 += fr1->w - xovlap;
	}

// fit noise data
	fr->exp.bias = fr->exp.bias + fr1->exp.bias;
	fr->exp.rdnoise = sqrt(sqr(fr->exp.rdnoise) + sqr(fr1->exp.rdnoise));
	fr->stats.statsok = 0;
	return 0;
}

// sub_frames substracts fr1 from fr; the two frames are aligned according to their skips
// the size of fr is not changed; fr1 is assumed to be a dark frame

int sub_frames (struct ccd_frame *fr, struct ccd_frame *fr1)
{
	int xovlap, yovlap;
	int xst, yst;
	int x1st, y1st;
	float *dp, *dp1;
	int x, y;

//	d3_printf("read noise is: %.1f %.1f\n", fr1->exp.rdnoise, fr->exp.rdnoise);

	xst = fr1->x_skip - fr->x_skip;
	if (fr1->x_skip + fr1->w >= fr->w + fr->x_skip)
		xovlap = fr->x_skip + fr->w - fr1->x_skip;
	else
		xovlap = fr1->x_skip + fr1->w - fr->x_skip;
	if (xst < 0) {
		x1st = -xst; 
		xst = 0;
	} else 
		x1st = 0;

	yst = fr1->y_skip - fr->y_skip;
	if (fr1->y_skip + fr1->h >= fr->h + fr->y_skip)
		yovlap = fr->y_skip + fr->h - fr1->y_skip;
	else
		yovlap = fr1->y_skip + fr1->h - fr->y_skip;
	if (yst < 0) {
		y1st = -yst; 
		yst = 0;
	} else 
		y1st = 0;

//	d3_printf("xst %d x1st %d yst %d x1st %d xovlap %d yovlap %d",
//		  xst, x1st, yst, y1st, xovlap, yovlap);

	dp = (float *)(fr->dat) + xst + yst * fr->w;
	dp1 = (float *)(fr1->dat) + x1st + y1st * fr1->w;

	for (y = 0; y < yovlap; y++) {
		for (x = 0; x < xovlap; x++) { 
			*dp = *dp - *dp1;
			dp ++;
			dp1 ++;
		}
		dp += fr->w - xovlap;
		dp1 += fr1->w - xovlap;
	}

// compute noise data
	fr->exp.bias = fr->exp.bias - fr1->exp.bias;
	fr->exp.rdnoise = sqrt(sqr(fr->exp.rdnoise) + sqr(fr1->exp.rdnoise));
	fr->stats.statsok = 0;
//	d3_printf("read noise is: %.1f %.1f\n", fr1->exp.rdnoise, fr->exp.rdnoise);
	return 0;
}




// make fr <= fr * m + s

int scale_shift_frame(struct ccd_frame *fr, double m, double s)
{
	int i, all;
	float *dp;

	all = fr->w * fr->h;
	dp = (float *)(fr->dat);

	for (i=0; i<all; i++) {
		*dp = *dp * m + s;
		dp ++;
	}
	fr->exp.bias = fr->exp.bias * m + s;
	fr->exp.scale /= fabs(m);
	fr->exp.rdnoise *= fabs(m);
	fr->stats.statsok = 0;

	return 0;
}

// lookup a keyword in the frame's vars. Return a pointer to it's
// line, or NULL if not found
FITS_row *fits_keyword_lookup(struct ccd_frame *fr, char *kwd)
{
	FITS_row *var;
	int i;

	if (kwd == NULL)
		return NULL;
	if (fr == NULL)
		return NULL;

	var = fr->var;
	for (i=0; i<fr->nvar; i++) {
		if (!strncmp((char *)(var + i), kwd, strlen(kwd)))
			return (var + i);
	}
	return NULL;
}

// add a string keyword to the fits header
// quotes are _not_ added to the string
// if the keyword exists, it is replaced
// return 0 if ok
int fits_add_keyword(struct ccd_frame *fr, char *kwd, char *val)
{
	char lb[81];
	FITS_row *v1;

	if (kwd == NULL)
		return 0;
	if (fr == NULL)
		return -1;
	if (val == NULL)
		return 0;

	sprintf(lb, "%-8s= %-70s", kwd, val);
	v1 = fits_keyword_lookup(fr, kwd);
	if (v1 != NULL) { // replace current value
		memcpy(v1, lb, sizeof(FITS_row));
	} else { // alloc space for a new one
		v1 = realloc(fr->var, (fr->nvar + 1) * FITS_HCOLS);
		if (v1 == NULL) {
			err_printf("cannot alloc mem for keyword %s\n", kwd);
			return ERR_ALLOC;
		} 
		fr->var = v1;
		memcpy(fr->nvar + v1, lb, FITS_HCOLS);
		fr->nvar ++;
	}
	return 0;

}

// append a HISTORY keyword to the fits header
// quotes are _not_ added to the string
// return 0 if ok
int fits_add_history(struct ccd_frame *fr, char *val)
{
	char lb[81];
	FITS_row *v1;

	if (val == NULL)
		return 0;
	if (fr == NULL)
		return -1;

	sprintf(lb, "HISTORY = %-70s", val);
	// alloc space for a new one
	v1 = realloc(fr->var, (fr->nvar + 1) * FITS_HCOLS);
	if (v1 == NULL) {
		err_printf("cannot alloc mem for history \n");
		return ERR_ALLOC;
	} 
	fr->var = v1;
	memcpy(fr->nvar + v1, lb, FITS_HCOLS);
	fr->nvar ++;
	return 0;
}

// crop_frame reduces the size of a frame; the upper-left corner of the frame will
// added to the current skip. The data area is realloced. Returns non-zero in case of an error.

int crop_frame(struct ccd_frame *fr, int x, int y, int w, int h)
{
	float *sp, *dp;
	int ix, iy;
	void *ret;

	d4_printf("crop from %d %d size %d %d \n", x, y, w, h);

// check that the subframe is fully contained in the source frame
	if (x < 0 || x + w > fr->w || y < 0 || y + h > fr->h) {
		err_printf("crop_frame: bad subframe\n");
		return ERR_FATAL;
	}
	sp = dp = (float *)(fr->dat);
	sp += x + y * fr->w;

// copy the contents to the upper-left corner
	for (iy=0; iy<h; iy++) {
		for(ix=0; ix<w; ix++) {
			*dp++ = *sp++;
		}
		sp += fr->w - w;
	}
	ret = realloc(fr->dat, sizeof(float)*w*h);
	if (ret == NULL) {
		err_printf("crop_frame: alloc error \n");
		return ERR_ALLOC;
	}
// adjust the frame info
	fr->x_skip += x;
	fr->y_skip += y;
	fr->w = w;
	fr->h = h;
	fr->dat = ret;
	fr->stats.statsok = 0;
	return 0;
}

// convert a frame to float format; may need to add more pixel formats here

static int frame_to_float(struct ccd_frame *fr)
{
	int all, i;
	void *ip;
	float *op;

	d3_printf("format is %d\n", fr->pix_format);

	if (fr->pix_format == PIX_FLOAT)
		return 0;

	all = fr->w * fr->h;

	switch(fr->pix_format) {
	case PIX_BYTE:
		op = (float *)fr->dat + all - 1;
		ip = fr->dat + all - 1;
		for (i=0; i< all; i++) {
			*op-- = 1.0 * (* (unsigned char *) ip);
			ip --;
		} 
		fr->pix_format = PIX_FLOAT;
		return 0;
#ifdef LITTLE_ENDIAN
	case PIX_16LE:
#else
	case PIX_16BE:
#endif
	case PIX_SHORT:
		op = (float *)fr->dat + all - 1;
		ip = (unsigned short *)fr->dat + all - 1;
		for (i=0; i< all; i++) {
			*op-- = 1.0 * (* (unsigned short *) ip);
			ip -= sizeof (unsigned short);
		} 
		fr->pix_format = PIX_FLOAT;
		return 0;
#ifdef LITTLE_ENDIAN
	case PIX_16BE:
		op = (float *)fr->dat + all - 1;
		ip = fr->dat + all * 2 - 2;
		for (i=0; i< all; i++) {
			*op-- = (256.0 * (* (unsigned char *) ip)
				 + 1.0 * (* (unsigned char *) (ip+1)) );
			ip -= 2;
		} 
		fr->pix_format = PIX_FLOAT;
		return 0;
#else
	case PIX_16LE:
		op = (float *)fr->dat + all - 1;
		ip = fr->dat + all * 2 - 2;
		for (i=0; i< all; i++) {
			*op-- = (256.0 * (* (unsigned char *) (ip+1))
				 + 1.0 * (* (unsigned char *) (ip)));
			ip -= 2;
		} 
		fr->pix_format = PIX_FLOAT;
		return 0;	
#endif /* LITTLE_ENDIAN */
		default:
		err_printf("cannot convert unknown format %d to float\n");
		return -1;
	}

}

/* get a double field 
 * return 1 if parsed ok, 0 if the field was not found,
 * or -1 for an error */
int fits_get_double(struct ccd_frame *fr, char *kwd, double *v)
{
	char vs[FITS_HCOLS+1];
	FITS_row *row;
	char * end;
	double vv;

	row = fits_keyword_lookup(fr, kwd);
	if (row == NULL)
		return 0; 
	memcpy(vs, row, FITS_HCOLS);
	vs[FITS_HCOLS + 1] = 0;
//	d3_printf("double field is: %s\n", vs+9);
	vv = strtod(vs+9, &end);
	if (end == vs+9)
		return -1;
	*v = vv;
	return 1;
}

/* get a int field (or the integer part of a double field)
 * return 1 if parsed ok, 0 if the field was not found,
 * or -1 for an error */
int fits_get_int(struct ccd_frame *fr, char *kwd, int *v)
{
	double vv;
	int ret;

	ret = fits_get_double(fr, kwd, &vv);
	if (ret > 0)
		*v = vv;
	return ret;
}


/* get a string field containing at most n characters
 * return the number of chars read, 0 if the field was not found,
 * or -1 for an error */
int fits_get_string(struct ccd_frame *fr, char *kwd, char *v, int n)
{
	char *row;
	int i, j;

	row = (char *)fits_keyword_lookup(fr, kwd);
	if (row == NULL)
		return 0; 
	for (i=9; i < FITS_HCOLS; i++) {
		if (row[i] == '"' || row[i] == '\'')
			break;
	}
	if (i++ >= FITS_HCOLS)
		return -1;
//	d3_printf("first quote at %d\n", i);
	for (j=0; i < FITS_HCOLS && j < n-1; i++, j++) {
		if (row[i] == '"' || row[i] == '\'') {
			break;
		}
		v[j] = row[i];
	}
//	d3_printf("second quote at %d\n", i);
	v[j] = 0;
	if (i == FITS_HCOLS)
		return -1;
	return j;
}

/* get a double (degrees) from a string field containing a 
 * DMS representation
 * return 1 if successfull, 0 if field is not found, -1 for a
 * parsing error */
int fits_get_dms(struct ccd_frame *fr, char *kwd, double *v)
{
	char dms[FITS_HCOLS];
	int ret;

	ret = fits_get_string(fr, kwd, dms, FITS_HCOLS - 1);
	if (ret <= 0)
		return ret;
//	d3_printf("dms field is: %s\n", dms);
	if (dms_to_degrees(dms, v)) {
		return -1;
	}
	return 1;
}


