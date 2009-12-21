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

// x11ops.c: image display and user interface functions
// $Revision: 1.2 $
// $Date: 2004/12/04 00:10:43 $

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#include "gcx.h"
#include "x11ops.h"


#define MAX_ZOOMOUT 5
#define MAX_ZOOMIN 8

#define SHOW_DATA_INIT (SHOW_STATS | SHOW_MAP | SHOW_EXP)

#define OVLY_TEXT_COLOR 0x00ff0000 //(100% red)
#define BR_TEXT_COLOR 0x00808000 //(100% red, 25% green)
#define IMAGE_BORDER_COLOR 0x00003000 // dark green 
#define SOURCE_COLOR 0x0000cc00
#define REFS_COLOR 0x00cccc00
#define STD_COLOR 0x00cccc00

#define IMWIN_XS 770
#define IMWIN_YS 500

#define TRKWIN_XS 500
#define TRKWIN_YS 270

#define ZOOMWIN_XS 100
#define ZOOMWIN_YS 100

#define HC_NSIGMAS 15.0	// number of sigmas above avg for the default hcut
#define MINSIGMA 7.0	// lowest value of sigma for cuts calculations

#define AVG_AT_DEF 0.05

#define BORDER 2	// for sources detect
#define SNR 12
#define BURN 20000


#define NAVG 13
static double avgt[NAVG]={-0.5, -0.25, 0, 0.05, 0.10, 0.15, 0.25, 0.5, 0.75, 0.85, 1, 1.25, 1.5};

static void draw_source(int iw, GC gc, struct star *s);
//static void draw_ap_star(int iw, GC gc, double xs, double ys);
static void window_help(int iw);
static int sel_key;

// X11 - related globals

static Display *d;
//Window w;	// the main display window
static Visual *visual;
static unsigned int depth;
//GC gc_black, gc_white, gc_ovly;

static Cursor gr_curs;
#define CURSOR XC_circle

//unsigned long *addr;		// image area for main screen
//XImage *image;

// clear and deallocate the sources field
static void delete_sources(struct fr_display_par *par)
{
	if (par->src.ns != 0) {
		free (par->src.s);
		par->src.s = NULL;
		par->src.ns = 0;
	}
	par->ref_star.x=0.0;
	par->star.x=0.0;
	par->ref_star.y=0.0;
	par->star.y=0.0;

}

// initialise lut for given gamma
static void lut_gamma(struct fr_display_par *par)
{
	int i;
	double id;
	int v;
	double gamma;
	int invert;

	gamma = par->gamma;
	invert = par->invert;
	if (gamma > 0.0) {
		for (i = 0; i < LUT_SIZE; i++) {
			id = (i + 1.0) / LUT_SIZE;
			id = exp(log(id) / gamma);
			v = ((int)floor(id*255.0));
			if (invert)
				v = 255-v;
			par->lut[i]= v * 0x00010101;
		}
	} else {
		for (i = 0; i < LUT_SIZE; i++) 
			v = (i * 255 / LUT_SIZE);
			if (invert)
				v = 255-v;
			par->lut[i]= v * 0x00010101;
	}
	par->map_changed = 1;
}

// initialise a display par structure with defaults
static int init_display_par(struct fr_display_par *par, struct ccd_frame *fr, XImage *img)
{
	int z;
	double s;

// now stuff the structure with defaults
	par->center_x = fr->w / 2;
	par->center_y = fr->h / 2;
	par->p1.x = fr->w / 2;
	par->p1.y = fr->h / 2;
	par->p1.set = 0;
	par->p2.x = 0;
	par->p2.y = 0;
	par->p2.set = 0;

	par->zoom_in = 1;
	if (fr->w > img->width || fr->h > img->height) {
		par->zoom_out = 1 + fr->w / img->width;
		z = fr->h / img->height + 1;
		if (z > par->zoom_out) 
			par->zoom_out = z;
	} else {
		par->zoom_out = 1;
	}
	par->zin_mode = ZIN_NNEIGHBOUR;
	par->zout_mode = ZOUT_AVG;
	par->map_changed = 1;
	if ( !fr->stats.statsok )
		frame_stats(fr);
	par->avg_at = AVG_AT_DEF;
	s = fr->stats.csigma;
	if (s < MINSIGMA) 
		s = MINSIGMA;
	par->lcut = fr->stats.cavg - HC_NSIGMAS * s * par->avg_at;
	par->hcut = fr->stats.cavg + HC_NSIGMAS * s * (1.0 - par->avg_at);
	if (par->hcut > fr->stats.max)
		par->hcut = fr->stats.max;
	if (par->lcut < fr->stats.min)
		par->lcut = fr->stats.min;
	par->gamma = 1.5;
	par->invert = 0;
	lut_gamma(par);
	delete_sources(par);
	return 0;
}

static void do_zi_im_copy(struct fr_display_par *par, struct ccd_frame *fr, XImage *img)
{
//	struct fr_display_par *par;
	int fxs, fys, fxe, fye, lb, rb, tb, bb;
	unsigned *imp, *impl;
	float *frp, *frpl;
	int frllen, frltot, imllen, imltot, nlines, nilines;
	double lc;
	double mult; 
	double v;
	int zi, z, zy;
	int x,y;
	unsigned vi;
	int li;

	zi = par->zoom_in;

	if (par->center_x >= fr->w)
		par->center_x = fr->w - 1;
	if (par->center_x < 1)
		par->center_x = 1;
	if (par->center_y >= fr->h)
		par->center_y = fr->h - 1;
	if (par->center_y < 1)
		par->center_y = 1;

// calculate extents
// x of first pixel displayed from frame
	fxs = par->center_x - (img->width / 2) / zi;
	if (fxs < 0) {
		lb = -fxs * zi; // left border
		fxs = 0;
	} else lb = 0;
// y of first pixel displayed from frame		
	fys = par->center_y - (img->height / 2) / zi;
	if (fys < 0) {
		tb = -fys * zi; // top border
		fys = 0;
	} else tb = 0;
// x of last pixel displayed from frame
	fxe = par->center_x + (img->width - img->width / 2) / zi;
	if (fxe >= fr->w) {
		rb = (fxe - fr->w) * zi;
		fxe = fr->w - 1;
	} else rb = 0;
// y of last pixel displayed from frame
	fye = par->center_y + (img->height - img->height / 2) / zi;
	if (fye >= fr->h) {
		bb = (fye - fr->h) * zi;
		fye = fr->h - 1;
	} else bb = 0;

//	d3_printf("fxs:%d fxe:%d lb:%d rb:%d fys:%d fye:%d tb:%d bb:%d\n", 
//		  fxs, fxe, lb, rb, fys, fye, tb, bb);


// TODO select the gamma/lut branches here

	frllen = fxe - fxs;
	frltot = fr->w;
	imllen = img->width - lb - rb;
	imltot = img->width;
	nlines = fye - fys;
	nilines = img->height - tb - bb;
	lc = par->lcut;
	mult = (LUT_SIZE - 1) / (par->hcut - par->lcut);

// do the image copy here
	impl = ((unsigned *)(img->data)) + imltot * tb + lb;
	frpl = ((float *)(fr->dat)) + frltot * fys + fxs;
	for (y = 0; y < nlines; y++) {
		for(zy = 0; zy < zi; zy++) {
			imp = impl;
			frp = frpl;
			for (x = 0; x < frllen; x++) {
				v = (*frp++ - lc) * mult;
				li = (int)floor(v);
				if (li > (LUT_SIZE - 1))
					li = (LUT_SIZE - 1);
				else if (li < 0)
					li = 0;
				vi = par->lut[li];
				for(z = 0; z < zi; z++)
					*imp++ = vi;
			}
			impl += imltot;
		}	
		frpl += frltot;
	}
// fill the borders
// do the top border 
//	d3_printf("TB\n");
	if (tb > 0) { 
		impl = (unsigned *)(img->data);
		for (x=0; x < tb * imltot; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
//	d3_printf("FLLB\n");
// first line of the left border
	if (lb > 0) {
		impl = (unsigned *)(img->data) + tb * imltot;
		for(x = 0; x < lb; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
//left/right border
//	d3_printf("LRB\n");
	if (lb > 0 || rb > 0) { 
		impl = (unsigned *)(img->data) + tb * imltot + imltot - rb;
		for (y = 0; y < nilines - 1; y++) {
			for(x = 0; x < lb + rb; x++)
				*impl++ = IMAGE_BORDER_COLOR;
			impl += imllen;
		}
	}
// last line of the right border
//	d3_printf("LLRB\n");
	if (rb > 0) {
		impl = (unsigned *)(img->data) + (tb + nilines) * imltot - rb;
		for(x = 0; x < rb; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
// bottom border
//	d3_printf("BB\n");
	if (bb > 0) { 
		impl = (unsigned *)(img->data) + (tb + nilines) * imltot;
		for (x=0; x < bb * imltot; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
// done
}

static void do_zo_im_copy(struct fr_display_par *par, struct ccd_frame *fr, XImage *img)
{

//	struct fr_display_par *par;
	int fxs, fys, fxe, fye, lb, rb, tb, bb;
	unsigned *imp, *impl;
	float *frp, *frpl;
	int frllen, frltot, imllen, imltot, nlines, nilines;
	double lc;
	double mult; 
	double v;
	int zo, z, zy;
	int x,y;
	int li;

	zo = par->zoom_out;

	if (par->center_x >= fr->w)
		par->center_x = fr->w - 1;
	if (par->center_x < 1)
		par->center_x = 1;
	if (par->center_y >= fr->h)
		par->center_y = fr->h - 1;
	if (par->center_y < 1)
		par->center_y = 1;

// calculate extents
// x of first pixel displayed from frame
	fxs = par->center_x - (img->width / 2) * zo;
	if (fxs < 0) {
		lb = -fxs / zo; // left border
		fxs = 0;
	} else lb = 0;
// y of first pixel displayed from frame
	fys = par->center_y - (img->height / 2) * zo;
	if (fys < 0) {
		tb = -fys / zo; // top border
		fys = 0;
	} else tb = 0;
// x of last pixel displayed from frame
	fxe = par->center_x + (img->width - img->width / 2) * zo;
	if (fxe >= fr->w) {
		rb = (fxe - fr->w) / zo;
		fxe = fr->w - 1;
	} else rb = 0;
// y of last pixel displayed from frame
	fye = par->center_y + (img->height - img->height / 2) * zo;
	if (fye >= fr->h) {
		bb = (fye - fr->h) / zo;
		fye = fr->h - 1;
	} else bb = 0;

//	d3_printf("fxs:%d fxe:%d lb:%d rb:%d fys:%d fye:%d tb:%d bb:%d\n", 
//		  fxs, fxe, lb, rb, fys, fye, tb, bb);


// TODO select the gamma/lut branches here

	frllen = (fxe - fxs) / zo * zo;
	frltot = fr->w;
	imllen = img->width - lb - rb;
	imltot = img->width;
	nlines = (fye - fys) / zo * zo;
	nilines = img->height - tb - bb;
	lc = par->lcut * zo * zo ;
	mult = (LUT_SIZE - 1) / (par->hcut - par->lcut) / zo / zo; 

// do the image copy here
	impl = ((unsigned *)(img->data)) + imltot * tb + lb;
	frpl = ((float *)(fr->dat)) + frltot * fys + fxs;
	if (par->zout_mode == ZOUT_AVG) {
		for (y = 0; y < nlines; y+= zo) {
			imp = impl;
			frp = frpl;
			for (x = 0; x < frllen; x+= zo) {
				v = 0;
				for (z = 0; z < zo; z++)
					for (zy=0; zy<zo; zy++)
						v += *(frp + z + zy*frltot); 
				frp += zo;
				v = (v - lc) * mult;
				li = (int)floor(v);
				if (li > (LUT_SIZE - 1))
					li = (LUT_SIZE - 1);
				else if (li < 0)
					li = 0;
				*imp++ = par->lut[li];
			}
			impl += imltot;	
			frpl += frltot * zo;
		}
	} else {
		err_printf("unknown zoom out mode %d\n", par->zout_mode);
	}
// fill the borders
// do the top border 
//	d3_printf("TB\n");
	if (tb > 0) { 
		impl = (unsigned *)(img->data);
		for (x=0; x < tb * imltot; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
//	d3_printf("FLLB\n");
// first line of the left border
	if (lb > 0) {
		impl = (unsigned *)(img->data) + tb * imltot;
		for(x = 0; x < lb; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
//left/right border
//	d3_printf("LRB\n");
	if (lb > 0 || rb > 0) { 
		impl = (unsigned *)(img->data) + tb * imltot + imltot - rb;
		for (y = 0; y < nilines - 1; y++) {
			for(x = 0; x < lb + rb; x++)
				*impl++ = IMAGE_BORDER_COLOR;
			impl += imllen;
		}
	}
// last line of the right border
//	d3_printf("LLRB\n");
	if (rb > 0) {
		impl = (unsigned *)(img->data) + (tb + nilines) * imltot - rb;
		for(x = 0; x < rb; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
// bottom border
//	d3_printf("BB\n");
	if (bb > 0) { 
		impl = (unsigned *)(img->data) + (tb + nilines) * imltot;
		for (x=0; x < bb * imltot; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
// done

}

static void do_x1_im_copy(struct fr_display_par *par, struct ccd_frame *fr, XImage *img)
{
//	struct fr_display_par *par;
	int fxs, fys, fxe, fye, lb, rb, tb, bb;
	unsigned *imp, *impl;
	float *frp, *frpl;
	int frllen, frltot, imllen, imltot, nlines, nilines;
	double lc;
	double mult; 
	double v;
	int x,y;
	int li;

	if (par->center_x >= fr->w)
		par->center_x = fr->w - 1;
	if (par->center_x < 1)
		par->center_x = 1;
	if (par->center_y >= fr->h)
		par->center_y = fr->h - 1;
	if (par->center_y < 1)
		par->center_y = 1;

// calculate extents
// x of first pixel displayed from frame
	fxs = par->center_x - (img->width / 2);
	if (fxs < 0) {
		lb = -fxs ; // left border
		fxs = 0;
	} else lb = 0;
// y of first pixel displayed from frame
	fys = par->center_y - (img->height / 2);
	if (fys < 0) {
		tb = -fys ; // top border
		fys = 0;
	} else tb = 0;
// x of last pixel displayed from frame
	fxe = par->center_x + (img->width - img->width / 2);
	if (fxe >= fr->w) {
		rb = fxe - fr->w ;
		fxe = fr->w - 1;
	} else rb = 0;
// y of last pixel displayed from frame
	fye = par->center_y + (img->height - img->height / 2) ;
	if (fye >= fr->h) {
		bb = fye - fr->h ;
		fye = fr->h - 1;
	} else bb = 0;

// TODO select the gamma/lut branches here

	frllen = fxe - fxs;
	frltot = fr->w;
	imllen = img->width - lb - rb;
	imltot = img->width;
	nlines = fye - fys;
	nilines = img->height - tb - bb;
	lc = par->lcut;
	mult = (LUT_SIZE - 1) / (par->hcut - par->lcut);

//	d3_printf("fxs:%d fxe:%d lb:%d rb:%d fys:%d fye:%d tb:%d bb:%d\n", 
//		  fxs, fxe, lb, rb, fys, fye, tb, bb);



// do the image copy here
	impl = ((unsigned *)(img->data)) + imltot * tb + lb;
	frpl = ((float *)(fr->dat)) + frltot * fys + fxs;
	for (y = 0; y < nlines; y++) {
		imp = impl;
		frp = frpl;
		for (x = 0; x < frllen; x++) {
			v = (*frp++ - lc) * mult;
			li = (int)floor(v);
			if (li > (LUT_SIZE - 1))
				li = (LUT_SIZE - 1);
			else if (li < 0)
				li = 0;
			*imp++ = par->lut[li];
		}
		impl += imltot;
		frpl += frltot;
	}

// fill the borders
// do the top border 
//	d3_printf("TB\n");
	if (tb > 0) { 
		impl = (unsigned *)(img->data);
		for (x=0; x < tb * imltot; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
//	d3_printf("FLLB\n");
// first line of the left border
	if (lb > 0) {
		impl = (unsigned *)(img->data) + tb * imltot;
		for(x = 0; x < lb; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
//left/right border
//	d3_printf("LRB\n");
	if (lb > 0 || rb > 0) { 
		impl = (unsigned *)(img->data) + tb * imltot + imltot - rb;
		for (y = 0; y < nilines - 1; y++) {
			for(x = 0; x < lb + rb; x++)
				*impl++ = IMAGE_BORDER_COLOR;
			impl += imllen;
		}
	}
// last line of the right border
//	d3_printf("LLRB\n");
	if (rb > 0) {
		impl = (unsigned *)(img->data) + (tb + nilines) * imltot - rb;
		for(x = 0; x < rb; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
// bottom border
//	d3_printf("BB\n");
	if (bb > 0) { 
		impl = (unsigned *)(img->data) + (tb + nilines) * imltot;
		for (x=0; x < bb * imltot; x++)
			*impl++ = IMAGE_BORDER_COLOR;
	}
// done
}

// Prepare a XImage from a ccd frame for display. The parameters used for data mapping are 
// provided in the par structure. Returns 0 for ok, or a negative error code

int fr2image(struct fr_display_par *par, struct ccd_frame *fr, XImage *img)
{
	if (par->zoom_in == 1) {
		if (par->zoom_out == 1) { 
//			d3_printf("x1\n");
			do_x1_im_copy(par, fr, img);
		} else {
//			d3_printf("zo\n");
			do_zo_im_copy(par, fr, img);
		}
	} else {
//		d3_printf("zi\n");
		do_zi_im_copy(par, fr, img);
	}

	par->map_changed = 0;
	return 0;
}

int xops_init(void)
{

	Colormap cmap;

	unsigned long white, black;
	d = XOpenDisplay("");
	if (d) {
		int scr = DefaultScreen(d);
		cmap = DefaultColormap(d, scr);
		visual = DefaultVisual(d, scr);
		depth = DefaultDepth(d, scr);
		white = WhitePixel(d, scr);
		black = BlackPixel(d, scr);
		gr_curs = XCreateFontCursor(d, CURSOR);
		return 0;
	} else {
		fprintf(stderr, "No X-display available\n");
		return -1;
	}

}


// print string in the corner of the window; corner 0 is top-left, 1 is top-right, 
// 2 is bot left, 3 is bot right. Printing a null string resets the line counter
// to the cornermost position. A null string and a corner that is not 0,1,2 or 3
// resets all corners
#define PR_BORDER 3

static int print_in_corner(Display *d, Window w, GC gc, char *str, int corner)
{
	int asc, desc, dir, i;
	XCharStruct xcs;
	static int linec[4] = {0,0,0,0};
        Window root_return;
        int x_return, y_return;
        unsigned int width_return, height_return;
        unsigned int border_width_return;
        unsigned int depth_return;

	XGetGeometry(d, w, &root_return, &x_return, &y_return, &width_return, 
		     &height_return, &border_width_return, &depth_return);


	if (str == NULL) {
		XQueryTextExtents(d, XGContextFromGC(gc), "X", strlen("X"), &dir, &asc, &desc, &xcs);
		if (corner >= 0 && corner <= 3)
			linec[corner] = 0;
		else for (i=0; i<4; i++)
			linec[i]=0;
		if (corner <= 0)
			linec[0] = (asc + desc) * 12 / 10;
		return 0;
	}

	XQueryTextExtents(d, XGContextFromGC(gc), str, strlen(str), &dir, &asc, &desc, &xcs);

	switch(corner) {
	case 0:
		XDrawString(d, w, gc, PR_BORDER, PR_BORDER + linec[corner] + asc, 
			    str, strlen(str));
		linec[corner] += (asc + desc) * 12 / 10;
		return 0;
	case 1:
		XDrawString(d, w, gc, width_return - xcs.rbearing - PR_BORDER, 
			    PR_BORDER + linec[corner] + asc, 
			    str, strlen(str));
		linec[corner] += (asc + desc) * 12 / 10;
		return 0;
	case 2:
		XDrawString(d, w, gc, PR_BORDER, 
			    height_return - (PR_BORDER + linec[corner] + desc),
			    str, strlen(str));
		linec[corner] += (asc + desc) * 12 / 10;
		return 0;
	case 3:
		XDrawString(d, w, gc, width_return - xcs.rbearing - PR_BORDER, 
			    height_return - (PR_BORDER + linec[corner] + desc),
			    str, strlen(str));
		linec[corner] += (asc + desc) * 12 / 10;
		return 0;
	default:
		return -1;
	}

}

int print_frame_info(Display *d, int iw, GC gc, int corner)
{
	int ret;
	char buf[256];
	struct ccd_frame *fr;
	Window w;

	w = iwt[iw].w;
	fr = iwt[iw].fr;

//	d3_printf("pfi\n");
	sprintf(buf, "Size: %dx%d; bin: %dx%d", fr->w, fr->h,
		fr->exp.bin_x, fr->exp.bin_y);
//	d3_printf("string is %s\n", buf);
	ret = print_in_corner(d, w, gc, buf, corner);
	if (ret) return ret;
	sprintf(buf, "Origin: %d,%d", fr->x_skip, 
		fr->y_skip);
	ret = print_in_corner(d, w, gc, buf, corner);
	if (ret) return ret;
	if (fr->magic & FRAME_UNCERTAIN)
	ret = print_in_corner(d, w, gc, "UNCERTAIN", corner);
	return ret;
}

int print_frame_stats(Display *d, int iw, GC gc, int corner)
{
	int ret;
	char buf[256];
	struct ccd_frame *fr;
	Window w;

	w = iwt[iw].w;
	fr = iwt[iw].fr;

	sprintf(buf, "cavg=%.1f; csigma=%.1f", fr->stats.cavg, fr->stats.csigma);
	ret = print_in_corner(d, w, gc, buf, corner);
	if (ret) return ret;
	sprintf(buf, "min=%.1f; max=%.1f", fr->stats.min, fr->stats.max);
	ret = print_in_corner(d, w, gc, buf, corner);
	if (ret) return ret;
	if (iwt[iw].par.p1.set) {
		sprintf(buf, "xy:%d,%d avg=%.1f; sigma=%.1f", 
			iwt[iw].par.p1.x, iwt[iw].par.p1.y,
			iwt[iw].par.p1.avg, iwt[iw].par.p1.sigma);
		ret = print_in_corner(d, w, gc, buf, corner);
		if (ret) return ret;
	}
	if (iwt[iw].par.p2.set) {
		sprintf(buf, "xy:%d,%d avg=%.1f; sigma=%.1f", 
			iwt[iw].par.p2.x, iwt[iw].par.p2.y,
			iwt[iw].par.p2.avg, iwt[iw].par.p2.sigma);
		ret = print_in_corner(d, w, gc, buf, corner);
		if (ret) return ret;
	}
	return ret;
}

int print_frame_map(Display *d, int iw, GC gc, int corner)
{
	int ret;
	char buf[256];
	int zi, zo;
	struct fr_display_par *par;
	Window w;

	w = iwt[iw].w;
	par = &iwt[iw].par;

	zi = par->zoom_in;
	zo = par->zoom_out;

	if (zo == 1) 
		sprintf(buf, "X %d @ %d,%d", zi,
			par->center_x,
			par->center_y);
	else 
		sprintf(buf, "X 1/%d @ %d,%d", zo, 
			par->center_x,
			par->center_y);
	ret = print_in_corner(d, w, gc, buf, corner);
	if (ret) return ret;
	sprintf(buf, "lcut=%.1f; hcut=%.1f; g=%.1f", 
		par->lcut,
		par->hcut,
		par->gamma );
	ret = print_in_corner(d, w, gc, buf, corner);
	return ret;
}


// new_image_window creates and maps a new window used for displaying image data
// returns the window's index in the table, or -1 if there was an error
// if height and/or width are 0, the default size for the window type is selected
// if the parent != -1, a zoom window is created as a subwindow of parent
// image and tracking windows are created from the screen's root
int new_image_window(int height, int width, int magic, int parent)
{
	int iw;
	int scr;
	unsigned long black;
	Window w;

	if (height == 0 || width == 0) {
		switch(magic) {
		case WIN_IMG:
			height = IMWIN_YS;
			width = IMWIN_XS;
			break;
		case WIN_TRACK:
			height = TRKWIN_YS;
			width = TRKWIN_XS;
			break;
		case WIN_ZOOM:
			height = ZOOMWIN_YS;
			width = ZOOMWIN_XS;
			break;
		default:
			err_printf("new_image_window: unknown window type: %d\n");
			return -1;
		}
	}
// look for an empty slot in the window table
	for (iw = 0; iw < MAX_WINDOWS; iw++)
		if (iwt[iw].magic == WIN_INVALID)
			break;
	if (iw == MAX_WINDOWS) {
		err_printf("new_image_window: too many windows\n");
		return -1;
	}

//	d3_printf("found empty slot at %d\n", iw);

// clear the structure fields
	iwt[iw].img = NULL;
	iwt[iw].fr = NULL;
	iwt[iw].data_to_show = SHOW_DATA_INIT;
	iwt[iw].zoomw = -1;
// create and map the window on the server
	scr = DefaultScreen(d);
	black = BlackPixel(d, scr);

//	d3_printf("preparing to create window; w=%d, h=%d\n",width, height);
	
	if (magic != WIN_ZOOM || parent < 0) {
		w = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0,
					width, height, 0, black, black);
	} else {
		if (iwt[parent].magic == WIN_IMG || iwt[parent].magic == WIN_TRACK)
			w = XCreateSimpleWindow(d, iwt[parent].w, 0, 0,
						width, height, 0, black, black);
		else {
			err_printf("new_image_window: bad parent\n");
			return -1;
		}
	}
	XDefineCursor(d, w, gr_curs);
	XSelectInput(d, w, KeyPressMask | ExposureMask |
		     ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
	XMapWindow(d, w);
	iwt[iw].w = w;
	iwt[iw].magic = magic;
	return iw;
}

// transform frame coordinates to window coordinates
static void frxy_to_winxy(int iw, int x, int y, int *x_ret, int *y_ret)
{
        Window root_return;
        int x_return, y_return;
        unsigned int width_return, height_return;
        unsigned int border_width_return;
        unsigned int depth_return;
	struct fr_display_par *par;
	struct ccd_frame *fr;
	int xd, yd;


	par = &iwt[iw].par;
	fr = iwt[iw].fr;

	XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
		     &height_return, &border_width_return, &depth_return);


	xd = x - par->center_x;//fr->w / 2;
	yd = y - par->center_y;//fr->h / 2;
//	d3_printf("xd = %d yd= %d\n", xd, yd);

	xd = xd * (int)par->zoom_in / (int)par->zoom_out;
	yd = yd * (int)par->zoom_in / (int)par->zoom_out;
//	d3_printf("xd = %d yd= %d\n", xd, yd);

	xd += width_return / 2 + par->zoom_in / 2;
	yd += height_return / 2 + par->zoom_in / 2;
//	d3_printf("xd = %d yd= %d\n", xd, yd);

	*x_ret = xd;
	*y_ret = yd;
//	d3_printf("xd = %d yd= %d\n", xd, yd);
}


static void draw_box(Display *d, int iw, GC gc)
{
	int x1, x2, y1, y2;

        Window w;
	w = iwt[iw].w;

	frxy_to_winxy(iw, iwt[iw].par.p1.x, iwt[iw].par.p1.y, &x1, &y1);
	frxy_to_winxy(iw, iwt[iw].par.p2.x, iwt[iw].par.p2.y, &x2, &y2);

	XDrawLine(d, w, gc, x1, y1, x2, y1);
	XDrawLine(d, w, gc, x2, y1, x2, y2);
	XDrawLine(d, w, gc, x2, y2, x1, y2);
	XDrawLine(d, w, gc, x1, y2, x1, y1);

}


// draw a crosshair around p1
#define CROSSHAIR_SIZE 100.0

static void draw_crosshair(Display *d, int iw, GC gc)
{
	double cx, cy;
	double a, di;
	double x1, x2, y1, y2;
	int xr, yr;
	double cs;
	double dx, dy;

        Window root_return, w;
        int x_return, y_return;
        unsigned int width_return, height_return;
        unsigned int border_width_return;
        unsigned int depth_return;
	struct fr_display_par *par;
//	struct ccd_frame *fr;

	GC gc_br;

	w = iwt[iw].w;

	gc_br = XCreateGC(d, w, 0, 0);
	XSetForeground(d, gc_br, BR_TEXT_COLOR);

	par = &iwt[iw].par;
//	fr = iwt[iw].fr;

	XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
		     &height_return, &border_width_return, &depth_return);


	dy = 1.0 * (par->p1.y - par->p2.y);
	dx = 1.0 * (par->p2.x - par->p1.x);
	if (par->p1.set && par->p2.set)
		a = atan2(dy, dx);
	else 
		a = 0.0;

	di = 3.0 * par->zoom_in / par->zoom_out;

	cs = CROSSHAIR_SIZE * par->zoom_in / par->zoom_out;

	frxy_to_winxy(iw, iwt[iw].par.p1.x, iwt[iw].par.p1.y, &xr, &yr);

	cx = 1.0 * xr + 0.5 * par->zoom_in;
	cy = 1.0 * yr + 0.5 * par->zoom_in;

	x1 = cx + di * sin(a);
	y1 = cy + di * cos(a);
	x2 = x1 + cs * cos(a);
	y2 = y1 - cs * sin(a);
	XDrawLine(d, w, gc, floor(x1), floor(y1), floor(x2), floor(y2));

	x1 = cx + di * sin(a);
	y1 = cy + di * cos(a);
	x2 = x1 - cs * cos(a);
	y2 = y1 + cs * sin(a);
	XDrawLine(d, w, gc, floor(x1), floor(y1), floor(x2), floor(y2));

	x1 = cx - di * sin(a);
	y1 = cy - di * cos(a);
	x2 = x1 + cs * cos(a);
	y2 = y1 - cs * sin(a);
	XDrawLine(d, w, gc, floor(x1), floor(y1), floor(x2), floor(y2));

	x1 = cx - di * sin(a);
	y1 = cy - di * cos(a);
	x2 = x1 - cs * cos(a);
	y2 = y1 + cs * sin(a);
	XDrawLine(d, w, gc, floor(x1), floor(y1), floor(x2), floor(y2));

	a += 3.1415926 / 2;

	x1 = cx + di * sin(a);
	y1 = cy + di * cos(a);
	x2 = x1 + cs * cos(a);
	y2 = y1 - cs * sin(a);
	XDrawLine(d, w, gc_br, floor(x1), floor(y1), floor(x2), floor(y2));

	x1 = cx + di * sin(a);
	y1 = cy + di * cos(a);
	x2 = x1 - cs * cos(a);
	y2 = y1 + cs * sin(a);
	XDrawLine(d, w, gc, floor(x1), floor(y1), floor(x2), floor(y2));

	x1 = cx - di * sin(a);
	y1 = cy - di * cos(a);
	x2 = x1 + cs * cos(a);
	y2 = y1 - cs * sin(a);
	XDrawLine(d, w, gc_br, floor(x1), floor(y1), floor(x2), floor(y2));

	x1 = cx - di * sin(a);
	y1 = cy - di * cos(a);
	x2 = x1 - cs * cos(a);
	y2 = y1 + cs * sin(a);
	XDrawLine(d, w, gc, floor(x1), floor(y1), floor(x2), floor(y2));


}

// repaint an image window
int image_repaint(int iw)
{
        Window root_return;
        int x_return, y_return;
        unsigned int width_return, height_return;
        unsigned int border_width_return;
        unsigned int depth_return;
	XImage *img;
	char *addr;
	int ret;
	struct ccd_frame *fr;
	struct fr_display_par *par;
	char buf[256];
	Window w;
	double dx, dy;

	GC gc_black, gc_ovly, gc_src, gc_ref;

	if (iwt[iw].fr == NULL)
		return -1;


	img = iwt[iw].img;
	par = &iwt[iw].par;
	w = iwt[iw].w;

	gc_black = XCreateGC(d, iwt[iw].w, 0, 0);
	gc_ovly = XCreateGC(d, iwt[iw].w, 0, 0);
	XSetForeground(d, gc_ovly, OVLY_TEXT_COLOR);
	gc_src = XCreateGC(d, iwt[iw].w, 0, 0);
	XSetForeground(d, gc_src, SOURCE_COLOR);
	gc_ref = XCreateGC(d, iwt[iw].w, 0, 0);
	XSetForeground(d, gc_ref, REFS_COLOR);

	XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
		     &height_return, &border_width_return, &depth_return);

	if (img == NULL || width_return != img->width || height_return != img->height) {
		if (img != NULL) {
			XDestroyImage(img);
//			d3_printf("image_repaint: destroying old image\n");
		}
		addr = malloc(width_return * height_return * 4);
		if (addr == NULL) {
//			d3_printf("image_repaint: cannot alloc data area for new XImage\n");
			return -1;
		}
//		d3_printf("image_repaint: making new image\n");
		img = XCreateImage(d, visual, depth, ZPixmap, 0, addr,
				   width_return, height_return, 32, 0);
		XInitImage(img);
		iwt[iw].img = img;
		ret = fr2image(&iwt[iw].par, iwt[iw].fr, img);
	} else {
//		d3_printf("image_repaint: reusing image\n");
	}

// if mapping params changed,  copy the image from the frame to the XImage
	if (par->map_changed) {
		ret = fr2image(par, iwt[iw].fr, img);
//		if (ret) return ret;
//		iwt[iw].fr = fr;
		par->map_changed = 0;
	}

//	d3_printf("image_repaint: successfuly remapped\n");

	fr = iwt[iw].fr;

// do the actual repaint

	XPutImage(d, iwt[iw].w, gc_black, img, 0, 0, 0, 0, img->width,
		  img->height);
	print_in_corner(d, iwt[iw].w, gc_ovly, NULL, -1);
	if (iwt[iw].data_to_show & SHOW_EXP)
		print_frame_info(d, iw, gc_ovly, 0);
	if (iwt[iw].data_to_show & SHOW_STATS)
		print_frame_stats(d, iw, gc_ovly, 2);
	if (iwt[iw].data_to_show & SHOW_MAP)
		print_frame_map(d, iw, gc_ovly, 1); 
	if ((iwt[iw].data_to_show & SHOW_REGION) && par->p1.set && par->p2.set)
		draw_box(d, iw, gc_ovly); 
	else if (iwt[iw].data_to_show & SHOW_CROSS)
		draw_crosshair(d, iw, gc_ovly); 
	if (iwt[iw].data_to_show & SHOW_SOURCES) {
//		d3_printf("SOUR");
		if (iwt[iw].par.src.ns ==0) {
		}
		show_sources(iw, &(iwt[iw].par.src)); 
	}
	if (iwt[iw].par.ref_star.x != 0.0) {// we have a ref star
		draw_source(iw, gc_ref, &par->ref_star);

		sprintf(buf, "REF %.1f,%.1f fwhm:%.1f M:%.1f", 
			par->ref_star.x, par->ref_star.y,
			par->ref_star.fwhm,
			flux_to_absmag(par->ref_star.flux)
			); 
		print_in_corner(d, w, gc_ovly, buf, 3);
		if (iwt[iw].par.star.x != 0.0) {// we also have a star
			draw_source(iw, gc_src, &par->star);
			sprintf(buf, "STAR %.1f,%.1f fwhm:%.1f M:%.1f", 
				par->star.x, par->star.y,
				par->star.fwhm,
				flux_to_absmag(par->star.flux)
				);  
			print_in_corner(d, w, gc_ovly, buf, 3);
			dx = par->star.x - par->ref_star.x;
			dy = par->star.y - par->ref_star.y;
			sprintf(buf, "dx:%.1f, dy:%.1f, d:%.1f, ang:%.1f, M:%.2f", 
				dx, dy, sqrt(dx * dx + dy * dy), 180 / PI * atan2(-dy, dx)
				,flux_to_absmag(par->star.flux) -
				flux_to_absmag(par->ref_star.flux));
			print_in_corner(d, w, gc_ovly, buf, 3);
		}
	} else { // no reference
		if (iwt[iw].par.star.x != 0.0) {// we also have a star
			draw_source(iw, gc_src, &par->star);
			sprintf(buf, "STAR %.1f,%.1f fwhm:%.1f M:%.1f", 
				par->star.x, par->star.y,
				par->star.fwhm,
				flux_to_absmag(par->star.flux)
				);  
			print_in_corner(d, w, gc_ovly, buf, 3);
		}
	}

	XFreeGC(d, gc_black);
	XFreeGC(d, gc_ovly);
	XFreeGC(d, gc_src);
	XFreeGC(d, gc_ref);
	XFlush(d);
	return 0;
}

// display a frame in a window. if par is NULL, the window par structure is 
// initialised with defaults, else it is copied from the argument
// note that the frame must remain available to the xops for showing after
// show_frame is called. It can only be released after a different frame is 
// assigned to the window, or the window is destroyed
int show_frame(int iw, struct ccd_frame *fr, struct fr_display_par *par)
{
        Window root_return;
        int x_return, y_return;
        unsigned int width_return, height_return;
        unsigned int border_width_return;
        unsigned int depth_return;
	XImage *img;
	char *addr;
	int ret;

	img = iwt[iw].img;

// first check that the frame has up-to date statistics, and redo them if needed
	if (fr->stats.statsok == 0) 
		frame_stats(fr);

// see if we can re-use the old XImage for this window; if not, discard the old one and 
// create another

	XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
		     &height_return, &border_width_return, &depth_return);

	if (img == NULL || width_return != img->width || height_return != img->height) {
		if (img != NULL)
			XDestroyImage(img);
		addr = malloc(width_return * height_return * 4);
		if (addr == NULL) {
			d3_printf("show_frame: cannot alloc data area for new XImage\n");
			return -1;
		}
		img = XCreateImage(d, visual, depth, ZPixmap, 0, addr,
				   width_return, height_return, 32, 0);
		XInitImage(img);
		iwt[iw].img = img;
	}			

// copy or initialise the par structure
	if (par == NULL) {
//		d3_printf("New par\n");
		ret = init_display_par(&iwt[iw].par, fr, img);
	}
	else 
		memcpy(&iwt[iw].par, par, sizeof(struct fr_display_par) );
//	if (ret) return ret;

	if (iwt[iw].fr) {
		release_frame(iwt[iw].fr);
	}

	iwt[iw].fr = fr;
	get_frame(fr);

// now copy the image from the frame to the XImage
	ret = fr2image(&iwt[iw].par, fr, img);

//	d3_printf("fr2image returns %d", ret);

	XStoreName(d, iwt[iw].w, fr->name);

// finally, paint the window
	ret = image_repaint(iw);
	return ret;
}

// increase/decrease zoom level; when zd is 0, zoom so that the complete image fits
static void set_zoom(int iw, int zd)
{
	int z;
	struct fr_display_par *par;
	struct ccd_frame *fr;
	XImage *img;

	par = &iwt[iw].par;

	if (zd > 0) {
		if (par->zoom_out > 1)
			par->zoom_out --;
		else {
			par->zoom_in ++;
			if (par->zoom_in > MAX_ZOOMIN)
				par->zoom_in = MAX_ZOOMIN;
		}
	} else if (zd < 0) {
		if (par->zoom_in > 1)
			par->zoom_in --;
		else 
			par->zoom_out ++;
			if (par->zoom_out > MAX_ZOOMOUT)
				par->zoom_out = MAX_ZOOMOUT;
	} else { // zoom all
		img = iwt[iw].img;
		fr = iwt[iw].fr;
		par->center_x = fr->w / 2;
		par->center_y = fr->h / 2;
		par->zoom_in = 1;
		if (fr->w > img->width || fr->h > img->height) {
			par->zoom_out = 1 + fr->w / img->width;
			z = fr->h / img->height + 1;
			if (z > par->zoom_out) 
			par->zoom_out = z;
		} else {
			par->zoom_out = 1;
		}
	}
	par->map_changed = 1;
//	d3_printf("zi: %d, zo: %d\n", par->zoom_in, par->zoom_out);
}





// transform window coordinates to frame coordinates
static void winxy_to_frxy(int iw, int x, int y, int *x_ret, int *y_ret)
{
	int fxs, fys, lb, tb;
        Window root_return;
        int x_return, y_return;
        unsigned int width_return, height_return;
        unsigned int border_width_return;
        unsigned int depth_return;
	struct fr_display_par *par;
	struct ccd_frame *fr;
	int xd, yd, zi;


	par = &iwt[iw].par;
	fr = iwt[iw].fr;

	XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
		     &height_return, &border_width_return, &depth_return);

	if (par->zoom_in == 1) {
		
		xd = x - width_return / 2;	
		yd = y - height_return / 2;
//	d3_printf("xd = %d yd= %d\n", xd, yd);

		xd = (xd * (int)par->zoom_out) / (int)par->zoom_in;
		yd = (yd * (int)par->zoom_out) / (int)par->zoom_in;
//	d3_printf("xd = %d yd= %d\n", xd, yd);

		xd += par->center_x;
		yd += par->center_y;
//	d3_printf("xd = %d yd= %d\n", xd, yd);
	} else {
		zi = par->zoom_in;

//		if (par->center_x >= fr->w)
//			par->center_x = fr->w - 1;
//		if (par->center_x < 1)
//			par->center_x = 1;
//		if (par->center_y >= fr->h)
//			par->center_y = fr->h - 1;
//		if (par->center_y < 1)
//			par->center_y = 1;

// calculate extents
// x of first pixel displayed from frame
		fxs = par->center_x - (width_return / 2) / zi;
		if (fxs < 0) {
			lb = -fxs * zi; // left border
			fxs = 0;
		} else lb = 0;
// y of first pixel displayed from frame		
		fys = par->center_y - (height_return / 2) / zi;
		if (fys < 0) {
			tb = -fys * zi; // top border
			fys = 0;
		} else tb = 0;

		xd = x - lb;
		yd = y - tb;
		xd /= zi;
		yd /= zi;
		xd += fxs;
		yd += fys;
	}

	if (xd < 0) 
		xd = 0;
	if (xd > fr->w - 1) 
		xd = fr->w - 1;
	if (yd < 0) 
		yd = 0;
	if (yd > fr->h - 1) 
		yd = fr->h - 1;
	*x_ret = xd;
	*y_ret = yd;




//	d3_printf("xd = %d yd= %d\n", xd, yd);
}

// pan the image so the supplied coordinates are centered
static void pan_image(int iw, int x, int y)
{
	int xfr, yfr;
	winxy_to_frxy(iw, x, y, &xfr, &yfr);
	iwt[iw].par.center_x = xfr;
	iwt[iw].par.center_y = yfr;
	iwt[iw].par.map_changed = 1;
}

// adjust the contrast/cuts of an image according to the supplies params
// dx sets brightness, dy sets gain
// a negative sigmas value will go from min to max
static void set_map_xy(int iw, double dx, double dy)
{
	struct fr_display_par *par;
	struct ccd_frame *fr;

	double s, av;

	double span;

	par = &iwt[iw].par;
	fr = iwt[iw].fr;

	if ( !fr->stats.statsok ) {
		frame_stats(fr);
		par->avg_at = AVG_AT_DEF;
	}
	span = par->hcut - par->lcut;
	s = fr->stats.max - fr->stats.min;
	av = fr->stats.avg;

	if (dy > 0.2)
		dy = (0.2 + 5 * (dy - 0.2));
	else if (dy < -0.2)
		dy = (-0.2 + 5 * (dy + 0.2));
	dy /= 4.2;


	if (dx > 0.2)
		dx = (0.2 + 10 * (dx - 0.2));
	else if (dx < -0.2)
		dx = (-0.2 + 10 * (dx + 0.2));
	dx /= 8.2;

	par->lcut = av - s * dx - par->avg_at * (dy) * s;
	par->hcut = av - s * dx + (1.0 - par->avg_at) * (dy) * s; 

	par->map_changed = 1;
}


// adjust the contrast of the image to span the supplied number of sigmas
// a negative sigmas value will go from min to max
static void set_contrast(int iw, double sigmas)
{
	struct fr_display_par *par;
	struct ccd_frame *fr;

	double s;

	par = &iwt[iw].par;
	fr = iwt[iw].fr;

	if ( !fr->stats.statsok ) {
		frame_stats(fr);
		par->avg_at = AVG_AT_DEF;
	}
	if (sigmas > 0.0) {
		s = fr->stats.csigma;
		if (s < MINSIGMA) 
			s = MINSIGMA;
		par->lcut = fr->stats.cavg - sigmas * s * par->avg_at ;
		par->hcut = fr->stats.cavg + sigmas * s * (1.0 - par->avg_at);
		if (par->hcut > fr->stats.max)
			par->hcut = fr->stats.max;
		if (par->lcut < fr->stats.min)
			par->lcut = fr->stats.min;
	} else {
		par->hcut = fr->stats.max;
		par->lcut = fr->stats.min;
	}
	par->map_changed = 1;
}

// adjust the brightness of the image in steps
// a step of 0 will restore the default
static void set_brightness(int iw, int step)
{
	struct fr_display_par *par;
	struct ccd_frame *fr;
	int i;

	double s;

	par = &iwt[iw].par;
	fr = iwt[iw].fr;

	if ( !fr->stats.statsok ) {
		frame_stats(fr);
		par->avg_at = AVG_AT_DEF;
		s = fr->stats.csigma;
		if (s < MINSIGMA) 
			s = MINSIGMA;
		par->lcut = fr->stats.cavg - HC_NSIGMAS * s * par->avg_at ;
		par->hcut = fr->stats.cavg + HC_NSIGMAS * s * (1.0 - par->avg_at);
	} else {
		if (step == 0)
			par->avg_at = AVG_AT_DEF;
		else {
			for(i=0; i<NAVG - 1; i++) 
				if (par->avg_at < (avgt[i] + avgt[i+1]) / 2) break;
			i += step;
			if (i < 0)
				i = 0;
			if (i > NAVG - 1)
				i = NAVG - 1;
			par->avg_at = avgt[i];
		}
		s = par->hcut - par ->lcut;
		par->lcut = fr->stats.cavg - s * par->avg_at ;
		par->hcut = fr->stats.cavg + s * (1.0 - par->avg_at);
	}

//	if (par->hcut > fr->stats.max)
//		par->hcut = fr->stats.max;
//	if (par->lcut < fr->stats.min)
//		par->lcut = fr->stats.min;
	par->map_changed = 1;

}

#define RADIUS 10

// set coords of point 1 and compute local statistics
static void set_p1(int iw, int x, int y)
{
	int xfr, yfr;
	struct rstats rs;
	struct ccd_frame *fr;
	double median;

	winxy_to_frxy(iw, x, y, &xfr, &yfr);
	iwt[iw].par.p1.x = xfr;
	iwt[iw].par.p1.y = yfr;
	iwt[iw].par.p1.set = 1;
	iwt[iw].par.p2.set = 0;
	iwt[iw].par.map_changed = 1;

	if (!(iwt[iw].fr))
		return;
	fr = iwt[iw].fr;
	if (!fr->stats.statsok)
		frame_stats(fr);

	ring_stats(iwt[iw].fr, xfr, yfr, 0, RADIUS, QUAD1|QUAD2|QUAD3|QUAD4, &rs, -HUGE, HUGE);
	median = rs.median;
	ring_stats(iwt[iw].fr, xfr, yfr, 0, RADIUS, QUAD1|QUAD2|QUAD3|QUAD4, 
		   &rs, median - 6 * fr->stats.csigma, median + 6 * fr->stats.csigma);

	iwt[iw].par.p1.avg = rs.avg;
	iwt[iw].par.p1.sigma = rs.sigma;

//	info_printf("x:%d y:%d cavg:%.1f csig:%.1f\n", xfr, yfr, rs.avg, rs.sigma );
}

static void set_p2(int iw, int x, int y)
{
	int xfr, yfr;
	struct rstats rs;
	struct ccd_frame *fr;
	double median;

	winxy_to_frxy(iw, x, y, &xfr, &yfr);
	if (xfr != iwt[iw].par.p1.x || yfr != iwt[iw].par.p1.y) {
		iwt[iw].par.p2.x = xfr;
		iwt[iw].par.p2.y = yfr;
		iwt[iw].par.p2.set = yfr;
		iwt[iw].par.map_changed = 1;
	}

	if (!(iwt[iw].fr))
		return;
	fr = iwt[iw].fr;
	if (!fr->stats.statsok)
		frame_stats(fr);

	ring_stats(iwt[iw].fr, xfr, yfr, 0, RADIUS, QUAD1|QUAD2|QUAD3|QUAD4, &rs, -HUGE, HUGE);
	median = rs.median;
	ring_stats(iwt[iw].fr, xfr, yfr, 0, RADIUS, QUAD1|QUAD2|QUAD3|QUAD4, 
		   &rs, median - 6 * fr->stats.csigma, median + 6 * fr->stats.csigma);

	iwt[iw].par.p2.avg = rs.avg;
	iwt[iw].par.p2.sigma = rs.sigma;
}

// show info on a star by user's request
// a ref of 1 sets the current star to be the reference
#define APERTURE_RADIUS 10
static void star_info(int iw, int x, int y, int ref)
{
	struct ccd_frame *fr;
	int fx, fy;
	int ret;
	struct star s;

	fr = iwt[iw].fr;

	if (fr == NULL)
		return;

	winxy_to_frxy(iw, x, y, &fx, &fy);
	ret = get_star_near(fr, fx, fy, 0, &s);
	if (ret)
		return;

	if (ref)  // set as reference
		memcpy(&(iwt[iw].par.ref_star), &s, sizeof(struct star));
	else 
		memcpy(&(iwt[iw].par.star), &s, sizeof(struct star));
 //	d3_printf("star at x:%.1f y:%.1f xfwhm:%.1f yfwhm:%.1f A:%.1f err:%.1f\n", s.x, s.y, 
//		  s.vg.s*FWHMSIG, s.hg.s*FWHMSIG, s.vg.A, s.err);
}

// calculate &display stats for the current line/column
static void line_col_stats(int iw, int x, int y)
{
	int xfr, yfr;
	struct ccd_frame *fr;
	Window w;

	w = iwt[iw].w;
	fr = iwt[iw].fr;

	winxy_to_frxy(iw, x, y, &xfr, &yfr);

// TODO line/col stats and print in window here

	XFlush(d);
}

// show coords/value of cursor 
static void show_coords(int iw, int x, int y)
{
	int xfr, yfr;
	struct ccd_frame *fr;
	int asc, desc, dir;
	XCharStruct xcs;
	double v;
	GC gc_s;
	char buf[64];

	if (iwt[iw].fr == NULL)
		return;

	gc_s = XCreateGC(d, iwt[iw].w, 0, 0);
	XSetForeground(d, gc_s, OVLY_TEXT_COLOR);
//	XSetBackground(d, gc_s, IMAGE_BORDER_COLOR);

	winxy_to_frxy(iw, x, y, &xfr, &yfr);
	fr = iwt[iw].fr;

	if (xfr < 0)
		xfr = 0;
	if (xfr >= fr->w)
		xfr = fr->w - 1;
	if (xfr < 0)
		yfr = 0;
	if (yfr >= fr->h)
		yfr = fr->h - 1;

	v = ((float *)(fr->dat))[xfr + yfr * fr->w];
	sprintf(buf, "%4d %4d %8.1f ", xfr, yfr, v);
	XQueryTextExtents(d, XGContextFromGC(gc_s), buf, strlen(buf), &dir, &asc, &desc, &xcs);

	if (iwt[iw].img != NULL) {
		XPutImage(d, iwt[iw].w, gc_s, iwt[iw].img, 0, 0, 0, 0, xcs.width,
			  12 * (asc+desc) / 10 + 1);
	} else {
		XClearArea(d, iwt[iw].w, 0, 0, xcs.width, 12 * (asc+desc) / 10 + 1, 0);
	}


	XDrawString(d, iwt[iw].w, gc_s, PR_BORDER, PR_BORDER + asc, 
			    buf, strlen(buf));
	XFreeGC(d, gc_s);
	XFlush(d);
}

// Process button release event
static int do_button_release_evt(int iw, XEvent *e)
{
	int ret=0;
	unsigned button;

	button = e->xbutton.button;

	switch(button) {
	case Button1: // set p2
		if (!(e->xbutton.state & ShiftMask))
			set_p2(iw, e->xbutton.x, e->xbutton.y);
		ret = 1;
//		iwt[iw].data_to_show |= SHOW_CROSS;
//		d3_printf("B1 pressed x=%d y=%d\n", e->xbutton.x, e->xbutton.y);
		break;
	}
	return ret;
}


// process button press event
static int do_button_evt(int iw, XEvent *e)
{
        Window root_return;
        int x_return, y_return;
        unsigned int width_return, height_return;
        unsigned int border_width_return;
        unsigned int depth_return;

	int ret=0;
	double dx, dy;

	unsigned button;

	button = e->xbutton.button;

	switch(button) {
	case Button1: // set point
		if (e->xbutton.state & ShiftMask)
			set_p2(iw, e->xbutton.x, e->xbutton.y);
		else 
			set_p1(iw, e->xbutton.x, e->xbutton.y);
		ret = 1;
//		iwt[iw].data_to_show |= SHOW_CROSS;
//		d3_printf("B1 pressed x=%d y=%d\n", e->xbutton.x, e->xbutton.y);
		break;
	case Button2: // pan center
//		d3_printf("B2 pressed x=%d y=%d\n", e->xbutton.x, e->xbutton.y);
		pan_image(iw, e->xbutton.x, e->xbutton.y);
		ret = 1;
		break;
	case Button3:
//		d3_printf("B3 pressed x=%d y=%d\n", e->xbutton.x, e->xbutton.y);
		XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
			     &height_return, &border_width_return, &depth_return);
		dx = (e->xbutton.x - width_return / 2.0) / (0.5 * width_return);
		dy = (e->xbutton.y * 1.0) / (height_return);
		if (e->xbutton.state & ShiftMask)
			set_map_xy(iw, dx, dy);
		else 
			set_map_xy(iw, dx/8, dy/4);
		ret = 1;
		break;
	}
	return ret;
}


// process button event
static int do_motion_evt(int iw, XEvent *e)
{
        Window root_return;
        int x_return, y_return;
        unsigned int width_return, height_return;
        unsigned int border_width_return;
        unsigned int depth_return;

	int ret=0;
	double dx, dy;

	unsigned state;

	state = e->xmotion.state;

	if (state & Button3Mask) {
//		d3_printf("B3 pressed x=%d y=%d\n", e->xbutton.x, e->xbutton.y);
		XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
			     &height_return, &border_width_return, &depth_return);
		dx = (e->xmotion.x - width_return / 2.0) / (0.5 * width_return);
		dy = (e->xmotion.y * 1.0) / (height_return);
		if (state & ShiftMask) {
			set_map_xy(iw, dx, dy);
		} else {
			set_map_xy(iw, dx/8, dy/4);
		}
		ret = 1;
	}
	show_coords(iw, e->xmotion.x, e->xmotion.y);

	return ret;
}


// process key events
// returns true if the window needs to be repainted
static int do_keypress_evt(int iw, XEvent *e)
{
        Window root_return;
        int x_return, y_return;
        unsigned int width_return, height_return;
        unsigned int border_width_return;
        unsigned int depth_return;

	int ret=0;

	KeySym key;

	key = XLookupKeysym(&e->xkey, 0);
	switch (key) {
// information display options
	case XK_y:
		sel_key = 'y';
		break;
	case XK_a:
		sel_key = 'a';
		break;
	case XK_n:
		sel_key = 'n';
		break;
	case XK_space:
		ret = 1;
		break;
	case XK_h:
	case XK_F1:
		window_help(iw);
		break;
	case XK_q:
		sel_key = 'q';
		break;
	case XK_i:	// toggle exposure data annotation
		iwt[iw].data_to_show ^= SHOW_EXP;
		iwt[iw].data_to_show ^= SHOW_STATS;
		iwt[iw].data_to_show ^= SHOW_MAP;
		ret = 1;
		break;
	case XK_c:	// toggle image statistics annotation
//		iwt[iw].data_to_show ^= SHOW_CROSS;
		sel_key = 'c';
		ret = 1;
		break;
	case XK_r:	// toggle image statistics annotation
		iwt[iw].data_to_show ^= SHOW_REGION;
		ret = 1;
		break;
// zoom/pan
	case XK_Page_Up:	// zoom in and pan to cursor
		pan_image(iw, e->xkey.x, e->xkey.y);
		set_zoom(iw, 1);
		XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
			     &height_return, &border_width_return, &depth_return);
//		XWarpPointer(d, iwt[iw].w, None, 0, 0, 0, 0, 
//			     width_return / 2 - e->xkey.x,
//			     height_return / 2 - e->xkey.y);
		ret = 1;
		break;
	case XK_Page_Down:	// zoom out
		set_zoom(iw, -1);
		ret = 1;
		break;
	case XK_Home:		// zoom all
		set_zoom(iw, 0);
		ret = 1;
		break;
	case XK_Left:	// pan left
		XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
			     &height_return, &border_width_return, &depth_return);
		pan_image(iw, width_return / 4, height_return / 2);
		ret = 1;
		break;
	case XK_Right:	// pan right
		XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
			     &height_return, &border_width_return, &depth_return);
		pan_image(iw, width_return * 3 / 4, height_return / 2);
		ret = 1;
		break;
	case XK_Up:	// pan up
		XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
			     &height_return, &border_width_return, &depth_return);
		pan_image(iw, width_return / 2, height_return / 4);
		ret = 1;
		break;
	case XK_Down:	// pan down
		XGetGeometry(d, iwt[iw].w, &root_return, &x_return, &y_return, &width_return, 
			     &height_return, &border_width_return, &depth_return);
		pan_image(iw, width_return / 2, height_return * 3 / 4);
		ret = 1;
		break;
	case XK_Insert:	// pan center
		pan_image(iw, e->xkey.x, e->xkey.y);
		ret = 1;
		break;
// change cuts / intensity mapping
	case XK_1:	// output range = 4 sigmas
		set_contrast(iw, HC_NSIGMAS / 8.0);
		ret = 1;
		break;
	case XK_2:	// output range = 7 sigmas
		set_contrast(iw, HC_NSIGMAS / 4.0);
		ret = 1;
		break;
	case XK_3:	// output range = 12 sigmas
		set_contrast(iw, HC_NSIGMAS / 2.0);
		ret = 1;
		break;
	case XK_4:	// output range = 20 sigmas
		set_contrast(iw, HC_NSIGMAS * 1.0);
		ret = 1;
		break;
	case XK_5:	
		set_contrast(iw, HC_NSIGMAS * 2.0);
		ret = 1;
		break;
	case XK_6:	
		set_contrast(iw, HC_NSIGMAS * 4.0);
		ret = 1;
		break;
	case XK_7:	
		set_contrast(iw, HC_NSIGMAS * 8.0);
		ret = 1;
		break;
	case XK_8:	
		set_contrast(iw, HC_NSIGMAS * 16.0);
		ret = 1;
		break;
	case XK_9:	// output range = min to max
		set_contrast(iw, -1.0);
		ret = 1;
		break;
	case XK_0: 	// reset default intensity mapping
		set_brightness(iw, 0);
		set_contrast(iw, HC_NSIGMAS);
		ret = 1;
		break;
	case XK_g:	// cycle through gamma settings 
		iwt[iw].par.gamma += 0.5;
		if (iwt[iw].par.gamma > 3.0)
			iwt[iw].par.gamma = 1.0;
		lut_gamma(&(iwt[iw].par));
		ret = 1;
		break;
	case XK_v:
		iwt[iw].par.invert = ! iwt[iw].par.invert;
		lut_gamma(&(iwt[iw].par));
		ret = 1;
		break;
	case XK_b:	// make display brighter (move the average point up)
		set_brightness(iw, 1);
		ret = 1;
		break;
	case XK_d:	// make display darker (move the average point down)
		set_brightness(iw, -1);
		ret = 1;
		break;
	case XK_p:	// 'photometry' - show star info (near cursor)
		star_info(iw, e->xkey.x, e->xkey.y, 0);
//		set_p1(iw, e->xkey.x, e->xkey.y);
		sel_key = 'p';
		ret = 1;
		break;
	case XK_o:	// set ref star
		star_info(iw, e->xkey.x, e->xkey.y, 1);
		ret = 1;
		sel_key = 'o';
		break;
	case XK_l:	// line/col statistics
		line_col_stats(iw, e->xkey.x, e->xkey.y);
//		ret = 1;
		break;
	case XK_s:	// toggle sources display
		iwt[iw].data_to_show ^= SHOW_SOURCES;
		if (!(iwt[iw].data_to_show & SHOW_SOURCES))
			delete_sources(&iwt[iw].par);
		ret = 1;
		break;
	}
	return ret;
}

// run the event handler for X operations
// should be called periodically from the main loop

int xops_event_loop(void)
{
	XEvent e;
	int i, ret;
	int redraw[MAX_WINDOWS];

	for(i=0; i<MAX_WINDOWS; i++)
		redraw[i] = 0;

	while (XPending(d) > 0) {
		XNextEvent(d, &e);
// now we look to determine the image window to which the event corresponds
		for (i=0; i<MAX_WINDOWS; i++) 
			if (iwt[i].magic != WIN_INVALID && e.xany.window == iwt[i].w)
				break;
		if (i == MAX_WINDOWS) {
			err_printf("Got event for bad window %d\n", e.xany.window);
			return -1;
		}
		switch (e.type) {
		case Expose:
//			d3_printf("Got expose event for window: %d\n", i);
			if (e.xexpose.count == 0) {
//				d3_printf("\nExpose x=%d y=%d width=%d height=%d\n", 
//					  e.xexpose.x, e.xexpose.y, 
//					  e.xexpose.width, e.xexpose.height);
				redraw[i] = 1;
			} 
			break;
		case KeyPress:
//			d3_printf("Got keypress event for window: %d\n", i);
			ret = do_keypress_evt(i, &e);
			if (ret) redraw[i] = 1;
			break;
		case ButtonPress:
//			d3_printf("Got keypress event for window: %d\n", i);
			ret = do_button_evt(i, &e);
			if (ret) redraw[i] = 1;
			break;
		case ButtonRelease:
//			d3_printf("Got keypress event for window: %d\n", i);
			ret = do_button_release_evt(i, &e);
			if (ret) redraw[i] = 1;
			break;
		case MotionNotify:
//			d3_printf("Got keypress event for window: %d\n", i);
			ret = do_motion_evt(i, &e);
			if (ret) redraw[i] = 1;
			break;

		}
	}
	for(i=0; i<MAX_WINDOWS; i++)
		if (redraw[i])
			image_repaint(i);
	return 0;
}

// show_stat print a string in the window title line
int show_stat(int iw, char *st)
{
	XStoreName(d, iwt[iw].w, st);
	XFlush(d);
	return 0;
}

#define SOURCE_SIZE 6
#define PIX_PER_MAG 0.3	// source size increment per magnitude
#define BASE_MAG -11	// magnitude for which source is SOURCE_SIZE large
static void draw_source(int iw, GC gc, struct star *s)
{
	int x, y, wx, wy, z, sz;

	z = iwt[iw].par.zoom_in;
	x = floor(s->x);
	y = floor(s->y);
	frxy_to_winxy(iw, (int)x, (int)y, &wx, &wy);
	wx += floor((s->x - x) * z);
	wy += floor((s->y - y) * z);

	sz = (int) floor(SOURCE_SIZE + BASE_MAG - flux_to_absmag(s->flux));
	if (sz < 2)
		sz = 2;

	XDrawArc(d, iwt[iw].w, gc, wx - z * sz, wy - z * sz, 
		       2*sz*z, 2*sz*z, 0, 64 * 359);
}

void draw_ap_stars(int iw, struct vs_recipy *vs, double dx, double dy)
{
	int x, y, wx, wy, zi, zo;
	double xs, ys;
	int i, ox, oy, w;
	GC gc_s;
	GC gc_p;

//	d3_printf("draw_ap_stars: dx %.1f dy %.1f \n", dx, dy);

	gc_s = XCreateGC(d, iwt[iw].w, 0, 0);
	XSetForeground(d, gc_s, STD_COLOR);
	gc_p = XCreateGC(d, iwt[iw].w, 0, 0);
	XSetForeground(d, gc_p, SOURCE_COLOR);

	zi = iwt[iw].par.zoom_in;
	zo = iwt[iw].par.zoom_out;

	for (i=0; i<vs->cnt; i++) {
		xs = vs->s[i].x + dx;
		ys = vs->s[i].y + dy;
		x = floor(xs+0.5);
		y = floor(ys+0.5);
		frxy_to_winxy(iw, (int)x, (int)y, &wx, &wy);
		if (vs->s[i].aph.flags & AP_STD_STAR) {
			w = 2 * floor(vs->p.r1) * zi / zo;
			ox = wx - w/2;
			oy = wy - w/2;
			XDrawArc(d, iwt[iw].w, gc_s, ox, oy, w, w, 0, 64*359);
			w = 2 * floor(vs->p.r2) * zi / zo;
			ox = wx - w/2;
			oy = wy - w/2;
			XDrawArc(d, iwt[iw].w, gc_s, ox, oy, w, w, 0, 64*359);
			w = 2 * floor(vs->p.r3) * zi / zo;
			ox = wx - w/2;
			oy = wy - w/2;
			XDrawArc(d, iwt[iw].w, gc_s, ox, oy, w, w, 0, 64*359);
		} else {
			w = 2 * floor(vs->p.r1) * zi / zo;
			ox = wx - w/2;
			oy = wy - w/2;
			XDrawArc(d, iwt[iw].w, gc_p, ox, oy, w, w, 0, 64*359);
			w = 2 * floor(vs->p.r2) * zi / zo;
			ox = wx - w/2;
			oy = wy - w/2;
			XDrawArc(d, iwt[iw].w, gc_p, ox, oy, w, w, 0, 64*359);
			w = 2 * floor(vs->p.r3) * zi / zo;
			ox = wx - w/2;
			oy = wy - w/2;
			XDrawArc(d, iwt[iw].w, gc_p, ox, oy, w, w, 0, 64*359);
		}

	}
	XFreeGC(d, gc_s);
	XFreeGC(d, gc_p);
	XFlush(d);

}


// show_sources displays circles on top of a bitmap display
// args are positions of the sources in frame coordinates
int show_sources(int iw, struct sources *src)
{
	GC gc_s;
	Window w;
	int i, z;
	struct star *st;

	st = calloc(src->ns, sizeof(struct star));
	if (st == NULL)
		return -1;

	w = iwt[iw].w;
	z = iwt[iw].par.zoom_in;

	iwt[iw].par.src.maxn = src->maxn;
	iwt[iw].par.src.ns = src->ns;
	iwt[iw].par.src.s = st;
	memcpy(st, src->s, src->ns * sizeof(struct star));

	gc_s = XCreateGC(d, w, 0, 0);
	XSetForeground(d, gc_s, SOURCE_COLOR);
	iwt[iw].data_to_show |= SHOW_SOURCES;

	for(i=0; i < src->ns; i++) {
		draw_source(iw, gc_s, &src->s[i]);
	}
	XFreeGC(d, gc_s);
	XFlush(d);
	return 0;
}

// wait for the user to do something on the image, then press y or no
// returns 0 if y or n weren't pressed, 1 for 'y', 0 for 'n'

int x_select_wait(void)
{
	int key = 0;
	xops_event_loop();
	if (sel_key) {
		key = sel_key;
		sel_key = 0;
	}

	return key;
}


// display window commands
void print_gui_help()
{
	info_printf("\nwindow controls:\n\n");
//	info_printf("Zoom/pan:\n\n");
	info_printf("RightButton drag: adjust brightness (left-right) / contrast (up-down)\n");
	info_printf("\n");
	info_printf("Arrows: pan frame                   Ins / CenterButton: pan center\n");
	info_printf("PgUp / PgDn: zoom in/out            Home: Zoom all\n");
	info_printf("\n");
	info_printf("1-9: adjust contrast (1=highest)    0: auto contrast / brightness \n");
	info_printf("b: brighter                         d: darker\n");
	info_printf("g: cycle through gamma settings     v: inVert display\n");
	info_printf("\n");
	info_printf("LeftButton: select point            LeftButton drag: select region\n");
	info_printf("c: toggle point crosshair display   r: toggle region display\n");
	info_printf("\n");
	info_printf("i: toggle info display              l: show line/col stats\n");
	info_printf("o: select reference star            p: select star\n");
	info_printf("space: redraw                       h/F1: show help\n");
	info_printf("\n");
	info_printf("Environment vars used by cx:\n");
	info_printf("CX_EDB_DIR location of edb catalogs (default: /usr/local/xephem/catalogs)\n");
	info_printf("CX_HOME location of cpxcon.mcs and track.badpix files (default: .)\n");
	info_printf("\n");
	info_printf("\n");
}

static void window_help(int iw)
{
	GC gc_s;
	Window w;

	if (iwt[iw].fr == NULL)
		return;

	w = iwt[iw].w;

	gc_s = XCreateGC(d, w, 0, 0);
	XSetForeground(d, gc_s, OVLY_TEXT_COLOR);


	print_in_corner(d,w,gc_s,"RightButton drag: adjust brightness (left-right) / contrast (up-down)",0);
	print_in_corner(d,w,gc_s,"Arrows: pan frame                   Ins / CenterButton: pan center",0);
	print_in_corner(d,w,gc_s,"PgUp / PgDn: zoom in/out            Home: Zoom all",0);
	print_in_corner(d,w,gc_s,"1-9: adjust contrast (1=highest)    0: auto contrast / brightness",0);
	print_in_corner(d,w,gc_s,"b: brighter                         d: darker",0);
	print_in_corner(d,w,gc_s,"g: cycle through gamma settings     v: inVert display\n",0);
//	print_in_corner(d,w,gc_s,"\n");
	print_in_corner(d,w,gc_s,"LeftButton: select point            LeftButton drag: select region",0);
	print_in_corner(d,w,gc_s,"c: toggle point crosshair display   r: toggle region display",0);
//	print_in_corner(d,w,gc_s,"\n");
	print_in_corner(d,w,gc_s,"i: toggle info display              l: show line/col stats",0);
	print_in_corner(d,w,gc_s,"o: select reference star            p: select star",0);
	print_in_corner(d,w,gc_s,"space: redraw                       h/F1: show help",0);

	XFreeGC(d, gc_s);
	XFlush(d);
}
