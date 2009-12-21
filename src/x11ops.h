// x11ops.h: image display and user interface declarations
// $Revision: 1.1 $
// $Date: 2003/12/01 00:19:37 $

#ifndef _X11OPS_H_
#define _X11OPS_H_

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/xf86dga.h>
#include <X11/cursorfont.h>

//number of elements in LUT
#define LUT_SIZE 4096


// parameters for displaying a frame
struct fr_display_par {
// intensity mapping
	double lcut; // the low cut
	double hcut; // the high cut
	int invert; // flag requesting reversed display
	double avg_at; // the position of the average between cuts (0..1.0)
	double gamma; // the gamma setting; a setting of one means linear
	unsigned lut[LUT_SIZE];
// geometric mapping
	unsigned center_x; // center of the area to be displayed
	unsigned center_y;
	unsigned zoom_in; // zoom in factor 
	unsigned zoom_out; // zoom out factor (one of zoom_in and zoom_out must be 1)
	int flip_h;	// flag for x flip
	int flip_v;	// y flip
	int zin_mode; // method of interpolation when zooming in
	int zout_mode; // method of combining pixel values when zooming out
// flag that must be set when changing the mappings, so the code knows to recalculate
	int map_changed; 
//	XImage *last_img; // pointer to the image that was last produced 
	struct sources src;
	struct point p1;
	struct point p2;
	struct star ref_star;
	struct star star;
};

// structure describing an image window
struct image_window {
	int magic;	// magic number defining type of window
	Window w;
	XImage *img; 	// holder for bitmap to display
	struct ccd_frame *fr; // frame holding the data we show
	struct fr_display_par par;	// structure defining how we display the image
					// in this window
	unsigned data_to_show; // bitfield defining what aux data we show
	int zoomw; 		// index of our zoom window, if any (-1 for none)
};

// the table of image windows
#define MAX_WINDOWS 32
struct image_window iwt[MAX_WINDOWS];

// window magic numbers
#define WIN_INVALID 0
#define WIN_IMG 1
#define WIN_TRACK 2
#define WIN_ZOOM 3

// data_to_show bits
#define SHOW_STATS 1	// show frame statistics
#define SHOW_MAP 2	// show mapping (cuts, zoom, position)
#define SHOW_EXP 4	// show exposure data
#define SHOW_HIST 8	// show histogram
#define SHOW_CROSS 16	// show cross over point
#define SHOW_REGION 32	// show region with points as corners
#define SHOW_SOURCES 64	// show detected sources (stars)

// defines for zoom modes
#define ZIN_NNEIGHBOUR 0	// nearest-neighbour method
#define ZOUT_AVG 0		// average method
#define ZOUT_DECIMATE 1		// decimate (subsample)

//extern Display *d;

//extern Visual *visual;

//extern unsigned int depth;

//extern GC gc_black, gc_white;

extern int xops_init(void);

extern int new_image_window(int height, int width, int magic, int parent);

extern int xops_event_loop(void);

extern int show_frame(int iw, struct ccd_frame *fr, struct fr_display_par *par);

// returns -1 for error
extern int xops_test(struct ccd_frame * frame) ;


extern int show_stat(int iw, char *st);

extern int show_sources(int iw, struct sources *src);

extern int image_repaint(int iw);

extern int x_select_wait(void);

extern void print_gui_help();


#endif
