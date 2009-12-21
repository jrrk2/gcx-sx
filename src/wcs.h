#ifndef _WCS_H_
#define _WCS_H_


/* function prototypes */
void wcs_from_frame(struct ccd_frame *fr, struct wcs *wcs);
struct wcs *wcs_new(void);
void wcs_ref(struct wcs *wcs);
void wcs_release(struct wcs *wcs);
int window_fit_wcs(GtkWidget *window);
int w_worldpos(struct wcs *wcs, double xpix, double ypix, double *xpos, double *ypos);
int w_xypix(struct wcs *wcs, double xpos, double ypos, double *xpix, double *ypix);
void cat_change_wcs(GSList *sl, struct wcs *wcs);
int auto_pairs(struct gui_star_list *gsl);
int pairs_cs_diff(GSList *pairs, double *dxo, double *dyo, 
		  double *dso, double *dthetao, int scale_en, int rot_en);
int fastmatch(GSList *field, GSList *cat);


#endif
