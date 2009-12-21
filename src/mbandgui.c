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

/* Multiband reduction gui and menus */

/* we hold references to o_frames, sobs and o_stars without them being ref_counted. 
   so while the clists in the dialog are alive, the mbds must not be destroyed */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "sourcesdraw.h"
#include "params.h"
#include "wcs.h"
#include "camera.h"
#include "cameragui.h"
#include "filegui.h"
#include "interface.h"
#include "misc.h"
#include "obsdata.h"
#include "multiband.h"
#include "plots.h"
#include "recipy.h"
#include "symbols.h"


static void bands_list_update_vals(GtkWidget *dialog, struct mband_dataset *mbds);
static void sob_list_set_row_vals(GtkWidget *sob_list, int row, struct star_obs *sob);
static void mb_rebuild_sob_list(gpointer dialog, GList *sol);
static void sob_list_update_vals(GtkWidget *sob_list);

#define PLOT_RES_SM 1
#define PLOT_RES_COL 2
#define PLOT_WEIGHTED 0x10
#define PLOT_ZP_AIRMASS 3
#define PLOT_ZP_TIME 4
#define PLOT_STAR 5

#define FIT_ZPOINTS 1
#define FIT_ZP_WTRANS 3
#define FIT_TRANS 2
#define FIT_ALL_SKY 4

#define SEL_ALL 1
#define UNSEL_ALL 2

#define ERR_SIZE 1024
/* print the error string and save it to storage */
static int mbds_printf(gpointer window, const char *fmt, ...)
{
	va_list ap, ap2;
	int ret;
	char err_string[ERR_SIZE+1];
	GtkWidget *label;
#ifdef __va_copy
	__va_copy(ap2, ap);
#else
	ap2 = ap;
#endif
	va_start(ap, fmt);
	va_start(ap2, fmt);
	ret = vsnprintf(err_string, ERR_SIZE, fmt, ap2);
	if (ret > 0 && err_string[ret-1] == '\n')
		err_string[ret-1] = 0;
	label = gtk_object_get_data(GTK_OBJECT(window), "status_label");
	if (label != NULL)
		gtk_label_set_text(GTK_LABEL(label), err_string);
	va_end(ap);
	return ret;
}

static int fit_progress(char *msg, void *data)
{
	mbds_printf(data, "%s", msg);
	while (gtk_events_pending ())
		gtk_main_iteration ();
	return 0;
}

static void rep_file_cb(char *fn, gpointer data, unsigned action)
{
	GtkWidget *ofr_list;
	GList *gl;
	GList *ofrs = NULL, *sl;
	FILE *repfp = NULL;
	struct mband_dataset *mbds;
	struct o_frame *ofr;
	char qu[1024];
	int ret;

	d3_printf("Report action %x fn:%s\n", action, fn);

	mbds = gtk_object_get_data(GTK_OBJECT(data), "mbds");
	g_return_if_fail(mbds != NULL);
	ofr_list = gtk_object_get_data(GTK_OBJECT(data), "ofr_list");
	g_return_if_fail(ofr_list != NULL);

	if ((repfp = fopen(fn, "r")) != NULL) { /* file exists */
		snprintf(qu, 1023, "File %s exists\nAppend?", fn);
		if (!modal_yes_no(qu, "gcx: file exists")) {
			fclose(repfp);
			return;
		} else {
			fclose(repfp);
		}
	}

	repfp = fopen(fn, "a");
	if (repfp == NULL) {
		mbds_printf(data, "Cannot open/create file %s (%s)", fn, strerror(errno));
		return;
	}

	gl = GTK_CLIST(ofr_list)->selection;
	if (gl != NULL) {
		for (gl = GTK_CLIST(ofr_list)->selection; gl != NULL; gl = g_list_next(gl)) {
//			d3_printf("row %d is selected\n", (int) gl->data);
			ofr = gtk_clist_get_row_data(GTK_CLIST(ofr_list), (int) gl->data); 
			if (((action & FMT_MASK) != REP_DATASET) && 
			    (ofr == NULL || ZPSTATE(ofr) <= ZP_FIT_ERR)) {
//				d3_printf("skipping frame with 0 fitted stars\n");
				continue;
			}
			ofrs = g_list_prepend(ofrs, ofr);
		}
		ofrs = g_list_reverse(ofrs);
	} else {
		for (sl = mbds->ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data); 
			if (((action & FMT_MASK) != REP_DATASET) && 
			    (ofr == NULL || ZPSTATE(ofr) <= ZP_FIT_ERR)) {
				continue;
			}
			ofrs = g_list_prepend(ofrs, ofr);
		}
		ofrs = g_list_reverse(ofrs);
	}
	if (ofrs == NULL) {
		error_beep();
		mbds_printf(data, "Nothing to report. Only fitted frames will be reported.\n");
		return;
	}
	
	ret = mbds_report_from_ofrs(mbds, repfp, ofrs, action);
	if (ret < 0) {
		mbds_printf(data, "%s", last_err());
	} else {
		mbds_printf(data, "%d frame(s), %d star(s) reported to %s", 
			    g_list_length(ofrs), ret, fn);
	}
	g_list_free(ofrs);
	fclose(repfp);
}

static void rep_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	file_select(data, "Report File", "", rep_file_cb, action);
}


static void select_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *list;
	switch(action) {
	case SEL_ALL:
		list = gtk_object_get_data(GTK_OBJECT(data), "ofr_list");
		if (list) 
			gtk_clist_select_all(GTK_CLIST(list));
		list = gtk_object_get_data(GTK_OBJECT(data), "sob_list");
		if (list) 
			gtk_clist_select_all(GTK_CLIST(list));
		break;
	case UNSEL_ALL:
		list = gtk_object_get_data(GTK_OBJECT(data), "ofr_list");
		if (list) 
			gtk_clist_unselect_all(GTK_CLIST(list));
		list = gtk_object_get_data(GTK_OBJECT(data), "sob_list");
		if (list) 
			gtk_clist_unselect_all(GTK_CLIST(list));
		break;
	}
}

static void mb_close_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	struct mband_dataset *mbds;
	GtkWidget *list;

	mbds = gtk_object_get_data(GTK_OBJECT(data), "mbds");

	if (mbds && !modal_yes_no("Closing the dataset will "
		 "discard fit information\n"
		 "Are you sure?", "Close dataset?"))
		return;

	list = gtk_object_get_data(GTK_OBJECT(data), "ofr_list");
	if (list) {
//		gtk_widget_hide(list);
		gtk_object_set_data(GTK_OBJECT(data), "ofr_list", NULL);
	}
	list = gtk_object_get_data(GTK_OBJECT(data), "sob_list");
	if (list) {
//		gtk_widget_hide(list);
		gtk_object_set_data(GTK_OBJECT(data), "sob_list", NULL);
	}
	list = gtk_object_get_data(GTK_OBJECT(data), "bands_list");
	if (list) {
//		gtk_widget_hide(list);
		gtk_object_set_data(GTK_OBJECT(data), "bands_list", NULL);
	}
	if (mbds)
		gtk_object_set_data(GTK_OBJECT(data), "mbds", NULL);
}


static void do_plot(gpointer data, guint action, FILE *plfp)
{
	GtkWidget *ofr_list;
	GList *gl;
	GList *ofrs = NULL, *sl;
	struct mband_dataset *mbds;
	struct o_frame *ofr;
	struct star_obs *sob;
	int band = -1;

	mbds = gtk_object_get_data(GTK_OBJECT(data), "mbds");
	g_return_if_fail(mbds != NULL);

	if (action == PLOT_STAR) {
		ofr_list = gtk_object_get_data(GTK_OBJECT(data), "sob_list");
		if (ofr_list == NULL) {
			error_beep();
			mbds_printf(data, "No star selected\n");
			return;
		}
		gl = GTK_CLIST(ofr_list)->selection;
		if (gl == NULL) {
			error_beep();
			mbds_printf(data, "No star selected\n");
			return;
		}
		for (gl = GTK_CLIST(ofr_list)->selection; gl != NULL; gl = g_list_next(gl)) {
			sob = gtk_clist_get_row_data(GTK_CLIST(ofr_list), (int) gl->data); 
			ofrs = g_list_prepend(ofrs, sob);
		}
		plot_star_mag_vs_time(plfp, ofrs);
		return;
	}


	ofr_list = gtk_object_get_data(GTK_OBJECT(data), "ofr_list");
	gl = GTK_CLIST(ofr_list)->selection;
	if (gl != NULL) {
		for (gl = GTK_CLIST(ofr_list)->selection; gl != NULL; gl = g_list_next(gl)) {
//			d3_printf("row %d is selected\n", (int) gl->data);
			ofr = gtk_clist_get_row_data(GTK_CLIST(ofr_list), (int) gl->data); 
			if (ofr == NULL || ZPSTATE(ofr) <= ZP_FIT_ERR) {
//				d3_printf("skipping frame with 0 fitted stars\n");
				continue;
			}
			ofrs = g_list_prepend(ofrs, ofr);
		}
		ofrs = g_list_reverse(ofrs);
	} else {
		for (sl = mbds->ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data); 
			if (ofr == NULL || ZPSTATE(ofr) <= ZP_FIT_ERR) {
//				d3_printf("skipping frame with 0 fitted stars\n");
				continue;
			}
			ofrs = g_list_prepend(ofrs, ofr);
		}
		ofrs = g_list_reverse(ofrs);
	}
	if (ofrs == NULL) {
		error_beep();
		mbds_printf(data, "Nothing to plot. Only fitted frames will be plotted\n");
		return;
	}
	for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
		if (O_FRAME(sl->data)->band >= 0) {
			band = O_FRAME(sl->data)->band;
			break;
		}
	}

	switch(action & 0x0f) {
	case PLOT_RES_SM:
		ofrs_plot_residual_vs_mag(plfp, ofrs, (action & PLOT_WEIGHTED) != 0);
		break;
	case PLOT_RES_COL:
		if (band < 0) {
			mbds_printf(data, "None of the selected frames has a valid band\n");
			break;
		}
		ofrs_plot_residual_vs_col(mbds, plfp, band, 
					  ofrs, (action & PLOT_WEIGHTED) != 0);
		break;
	case PLOT_ZP_TIME:
		ofrs_plot_zp_vs_time(plfp, ofrs);
		break;
	case PLOT_ZP_AIRMASS:
		ofrs_plot_zp_vs_am(plfp, ofrs);
		break;
	}
	g_list_free(ofrs);
}

static void plot_to_file_cb(char *fn, gpointer data, unsigned action)
{
	FILE *repfp;
	char qu[1024];

	if ((repfp = fopen(fn, "r")) != NULL) { /* file exists */
		snprintf(qu, 1023, "File %s exists\nOverwrite?", fn);
		if (!modal_yes_no(qu, "gcx: file exists")) {
			fclose(repfp);
			return;
		} else {
			fclose(repfp);
		}
	}
	repfp = fopen(fn, "w");
	if (repfp == NULL) {
		mbds_printf(data, "Cannot create file %s (%s)", fn, strerror(errno));
		return;
	}
	do_plot(data, action, repfp);
	fclose(repfp);
}

static void plot_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *ifact, *mi;
	FILE *plfp = NULL;
	int plot_to_window = 0;
	struct mband_dataset *mbds;

	mbds = gtk_object_get_data(GTK_OBJECT(data), "mbds");
	g_return_if_fail(mbds != NULL);
	ifact = gtk_object_get_data(GTK_OBJECT(data), "main_menu_if");
	mi = gtk_item_factory_get_widget (GTK_ITEM_FACTORY(ifact), "/Plot/Plot to File");
	g_return_if_fail(mi != NULL);
	plot_to_window = !GTK_CHECK_MENU_ITEM(mi)->active;
	if (plot_to_window) {
		plfp = popen(P_STR(FILE_GNUPLOT), "w");
		if (plfp == NULL) {
			mbds_printf(data, "Error running gnuplot (with %s)\n", 
				    P_STR(FILE_GNUPLOT));
			return ;
		}
		do_plot(data, action, plfp);
		pclose(plfp);
	} else {
		file_select(data, "Plot File", "", plot_to_file_cb, action);
	}


}


static void ofr_list_set_row_vals(GtkWidget *ofr_list, int row, struct o_frame *ofr)
{
	char c[50 * 10];
	int i;
	char *obj;
	char *states[] = {"Not Fitted", "Err", "All-sky", "ZP Only", "OK",  
			  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			  NULL, NULL, NULL};

	obj = stf_find_string(ofr->stf, 1, SYM_OBSERVATION, SYM_OBJECT);
	if (obj)
		snprintf(c, 40, "%s", obj);
	else 
		c[0] = 0;
	snprintf(c+50, 40, "%s", ofr->trans->bname);
	if (ofr->zpstate >= ZP_ALL_SKY) {
		snprintf(c+150, 40, "%.3f", ofr->zpoint);
		snprintf(c+200, 40, "%.3f", ofr->zpointerr);
		snprintf(c+350, 40, "%.2f", ofr->me1);
	} else {
		c[150] = 0;
		c[200] = 0;
		c[350] = 0;
	}
	snprintf(c+100, 40, "%s%s", states[ofr->zpstate & ZP_STATE_M], 
		 ofr->as_zp_valid ? "-AV" : "");
	if ((ofr->zpstate & ZP_STATE_M) > ZP_NOT_FITTED) {
		snprintf(c+250, 40, "%d", ofr->vstars);
		snprintf(c+300, 40, "%d", ofr->outliers);
	} else {
		c[250] = 0;
		c[300] = 0;
	}
	snprintf(c+400, 40, "%.2f", ofr->airmass);
	snprintf(c+450, 40, "%.5f", ofr->mjd);
	for (i = 0; i < 10; i++)
		gtk_clist_set_text(GTK_CLIST(ofr_list), row, i, c+50*i);
}

static void fit_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *ofr_list, *sob_list;
	GList *gl;
	GList *ofrs = NULL, *sl;
	struct mband_dataset *mbds;
	struct o_frame *ofr;
	int i;

	ofr_list = gtk_object_get_data(GTK_OBJECT(data), "ofr_list");
	sob_list = gtk_object_get_data(GTK_OBJECT(data), "sob_list");
	mbds = gtk_object_get_data(GTK_OBJECT(data), "mbds");
	g_return_if_fail(ofr_list != NULL);
//	g_return_if_fail(sob_list != NULL);
	g_return_if_fail(mbds != NULL);
	if (GTK_CLIST(ofr_list)->selection != NULL) {
		for (gl = GTK_CLIST(ofr_list)->selection; gl != NULL; gl = g_list_next(gl)) {
			ofr = gtk_clist_get_row_data(GTK_CLIST(ofr_list), (int) gl->data);
			g_return_if_fail(ofr != NULL);
			ofrs = g_list_prepend(ofrs, ofr);
		}
	} else {
		ofrs = g_list_copy(mbds->ofrs);
	}
	switch (action) {
	case FIT_ZPOINTS:
		fit_progress("Fitting zero points with no color", data);
		for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data);
			if (ofr->band < 0)
				continue;
			ofr->trans->k = 0.0;
			ofr->trans->kerr = BIG_ERR;
			ofr_fit_zpoint(ofr, P_DBL(AP_ALPHA), P_DBL(AP_BETA), 1);
		}
		fit_progress("Transforming stars", data);
		for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data);
			if (ofr->band < 0)
				continue;
			ofr_transform_stars(ofr, mbds, 0, 0);
		}
		bands_list_update_vals(data, mbds);
		fit_progress("Done", data);
		break;
	case FIT_ZP_WTRANS:
		fit_progress("Fitting zero points with current color coefficients", data);
		for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data);
			if (ofr->band < 0)
				continue;
			ofr_fit_zpoint(ofr, P_DBL(AP_ALPHA), P_DBL(AP_BETA), 1);
		}
		fit_progress("Transforming stars", data);
		for (sl = ofrs; sl != NULL; sl = g_list_next(sl)) {
			ofr = O_FRAME(sl->data);
			if (ofr->band < 0)
				continue;
			ofr_transform_stars(ofr, mbds, 0, 0);
		}
		fit_progress("Done", data);
		break;
	case FIT_TRANS:
		mbds_fit_all(ofrs, fit_progress, data);
		fit_progress("Transforming stars", data);
		mbds_transform_all(mbds, ofrs, 0);
		bands_list_update_vals(data, mbds);
		fit_progress("Done", data);
		break;
	case FIT_ALL_SKY:
		fit_progress("Fitting all-sky extinction coefficient", data);
		if (fit_all_sky_zp(mbds, ofrs)) {
			error_beep();
			mbds_printf(data, "%s", last_err());
			mbds_transform_all(mbds, ofrs, 0);
			bands_list_update_vals(data, mbds);
			break;
		}
		fit_progress("Transforming stars", data);
		mbds_transform_all(mbds, ofrs, 0);
		bands_list_update_vals(data, mbds);
		fit_progress("Done", data);

		break;
	default:
		return;
	}
	if (GTK_CLIST(ofr_list)->selection == NULL) {
		for (i = 0; i < GTK_CLIST(ofr_list)->rows; i++) {
			ofr = gtk_clist_get_row_data(GTK_CLIST(ofr_list), i);
			if (ofr == NULL)
				continue;
			ofr_list_set_row_vals(ofr_list, i, ofr);
			
//			sob_list_update_by_ofr(sob_list, ofr);
		}
	} else {
		for (gl = GTK_CLIST(ofr_list)->selection; gl != NULL; gl = g_list_next(gl)) {
			ofr = gtk_clist_get_row_data(GTK_CLIST(ofr_list), (int) gl->data);
			ofr_list_set_row_vals(ofr_list, (int) gl->data, ofr);
//			sob_list_update_by_ofr(sob_list, ofr);
		}
	}
	if (sob_list != NULL)
		sob_list_update_vals(sob_list);
	g_list_free(ofrs);
}


static void mbds_ofr_to_list(GtkWidget *dialog, GtkWidget *list)
{
	struct mband_dataset *mbds;
	GList *sl;
	struct o_frame *ofr;
	int row, i;
	char *line [] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	mbds = gtk_object_get_data(GTK_OBJECT(dialog), "mbds");
	if (mbds == NULL)
		return;
	gtk_clist_freeze(GTK_CLIST(list));
	sl = mbds->ofrs;
	while(sl != NULL) {
		ofr = O_FRAME(sl->data);
		sl = g_list_next(sl);
		row = gtk_clist_append(GTK_CLIST(list), line);
		ofr_list_set_row_vals(list, row, ofr);
		gtk_clist_set_row_data(GTK_CLIST(list), row, ofr);
		ofr->data = row;
	}
	for (i = 0; i< 10; i++) {
		gtk_clist_set_column_auto_resize(GTK_CLIST(list), i, 1);
	}
	gtk_clist_thaw(GTK_CLIST(list));
}

void ofr_bpress_cb(GtkCList *clist, GdkEventButton *event, gpointer data)
{
	int row = -1, col = -1, x, y;
	x = floor(event->x);
	y = floor(event->y);
	if (gtk_clist_get_selection_info(clist, x, y, &row, &col))
		mb_rebuild_sob_list(data, O_FRAME(gtk_clist_get_row_data(clist, row))->sol);
}

static void ofr_click_col_cb(GtkCList *clist, gint column, gpointer data)
{
	static int lastcol = -1;
	gtk_clist_set_sort_column(GTK_CLIST(clist), column);
	if (column == lastcol) {
		lastcol = -1;
		gtk_clist_set_sort_type(GTK_CLIST(clist), GTK_SORT_DESCENDING);
	} else {
		lastcol = column;
		gtk_clist_set_sort_type(GTK_CLIST(clist), GTK_SORT_ASCENDING);
	}
	gtk_clist_sort(GTK_CLIST(clist));
}

static void mb_rebuild_ofr_list(gpointer dialog)
{
	GtkWidget *list;
	GtkScrolledWindow *scw;
	char *titles[] = {"Object", "Band", "Status", "Zpoint", "Err", "Fitted", 
			  "Outliers", "MEU", "Airmass", "MJD", NULL};

	list = gtk_object_get_data(GTK_OBJECT(dialog), "ofr_list");
	if (list == NULL) {
		scw = gtk_object_get_data(GTK_OBJECT(dialog), "ofr_scw");
		g_return_if_fail(scw != NULL);
		list = gtk_clist_new_with_titles(10, titles);
		gtk_scrolled_window_add_with_viewport(scw, list);
		gtk_widget_ref(list);
		gtk_object_set_data_full(GTK_OBJECT(dialog), "ofr_list",
					 list, (GtkDestroyNotify) gtk_widget_destroy);
		gtk_clist_set_selection_mode(GTK_CLIST(list), GTK_SELECTION_EXTENDED);
		gtk_signal_connect(GTK_OBJECT(list), "button-press-event", ofr_bpress_cb, dialog);
		gtk_signal_connect(GTK_OBJECT(list), "click-column", ofr_click_col_cb, dialog);
		gtk_widget_show(list);
	} else {
		gtk_clist_clear(GTK_CLIST(list));
	}
	mbds_ofr_to_list(dialog, list);
}

static void bands_list_set_row_vals(GtkWidget *bands_list, int row, int band,
				    struct mband_dataset *mbds)
{
	char c[50 * 10];
	int i;
	
	snprintf(c, 40, "%s", mbds->trans[band].bname);
	if (mbds->trans[band].c1 >= 0 && mbds->trans[band].c2 >= 0) {
		snprintf(c+50, 40, "%s-%s", mbds->trans[mbds->trans[band].c1].bname,
			 mbds->trans[mbds->trans[band].c2].bname);
		if (mbds->trans[band].kerr < BIG_ERR) {
			snprintf(c+100, 40, "%.3f/%.3f", mbds->trans[band].k, 
				 mbds->trans[band].kerr);
		} else {
			c[100] = 0;
		}
	} else {
		c[50] = 0;
		c[100] = 0;
	}
	if (mbds->trans[band].zzerr < BIG_ERR) {
		snprintf(c+150, 40, "%.3f/%.3f", mbds->trans[band].zz, 
			 mbds->trans[band].zzerr);
		snprintf(c+200, 40, "%.3f", mbds->trans[band].am);
		snprintf(c+250, 40, "%.3f/%.3f", -mbds->trans[band].e, 
			 mbds->trans[band].eerr);
		snprintf(c+300, 40, "%.2f", mbds->trans[band].eme1);
	} else {
		c[150] = c[200] = c[250] = c[300] = 0;
	}

	for (i = 0; i < 7; i++)
		gtk_clist_set_text(GTK_CLIST(bands_list), row, i, c+50*i);
}

static void bands_list_update_vals(GtkWidget *dialog, struct mband_dataset *mbds)
{
	GtkWidget *list;
	int i;

	list = gtk_object_get_data(GTK_OBJECT(dialog), "bands_list");
	g_return_if_fail(mbds != NULL);
	g_return_if_fail(list != NULL);
	gtk_clist_freeze(GTK_CLIST(list));
	for (i = 0; i < mbds->nbands; i++) {
		bands_list_set_row_vals(list, i, i, mbds);
	}
	gtk_clist_thaw(GTK_CLIST(list));
}


static void mbds_bands_to_list(GtkWidget *dialog, GtkWidget *list)
{
	struct mband_dataset *mbds;
	GList *sl;
	int row, i;
	char *line [] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	mbds = gtk_object_get_data(GTK_OBJECT(dialog), "mbds");
	if (mbds == NULL)
		return;
	gtk_clist_freeze(GTK_CLIST(list));
	sl = mbds->ofrs;
	for (i = 0; i < mbds->nbands; i++) {
		row = gtk_clist_append(GTK_CLIST(list), line);
		bands_list_set_row_vals(list, row, i, mbds);
	}
	for (i = 0; i < 7; i++) {
		gtk_clist_set_column_auto_resize(GTK_CLIST(list), i, 1);
		gtk_clist_set_column_justification(GTK_CLIST(list), i, GTK_JUSTIFY_CENTER);
	}
	gtk_clist_thaw(GTK_CLIST(list));
}


static void mb_rebuild_bands_list(gpointer dialog)
{
	GtkWidget *list;
	GtkScrolledWindow *scw;
	char *titles[] = {"Band", "Color", "Color coeff", 
			  "All-sky zeropoint", "Mean airmass", 
			  "Extinction coeff", "Extinction me1", NULL};

	list = gtk_object_get_data(GTK_OBJECT(dialog), "bands_list");
	if (list == NULL) {
		scw = gtk_object_get_data(GTK_OBJECT(dialog), "bands_scw");
		g_return_if_fail(scw != NULL);
		list = gtk_clist_new_with_titles(8, titles);
		gtk_scrolled_window_add_with_viewport(scw, list);
		gtk_widget_ref(list);
		gtk_object_set_data_full(GTK_OBJECT(dialog), "bands_list",
					 list, (GtkDestroyNotify) gtk_widget_destroy);
		gtk_clist_column_titles_passive(GTK_CLIST(list));
//		gtk_clist_set_selection_mode(GTK_CLIST(list), GTK_SELECTION_EXTENDED);
		gtk_widget_show(list);
	} else {
		gtk_clist_clear(GTK_CLIST(list));
	}
	mbds_bands_to_list(dialog, list);
}


static void sob_list_set_row_vals(GtkWidget *sob_list, int row, struct star_obs *sob)
{
	char c[50 * 15];
	double se;
	int flags, n, i;

	snprintf(c, 40, "%s", sob->cats->name);
	switch(CATS_TYPE(sob->cats)) {
	case CAT_STAR_TYPE_APSTD:
		strcpy(c+50, "Std");
		break;
	case CAT_STAR_TYPE_APSTAR:
		strcpy(c+50, "Tgt");
		break;
	case CAT_STAR_TYPE_CAT:
		strcpy(c+50, "Obj");
		break;
	case CAT_STAR_TYPE_SREF:
		strcpy(c+50, "Field");
		break;
	default:
		c[50] = 0;
		break;
	}
	snprintf(c+100, 40, "%s", sob->ofr->trans->bname);

	c[150] = c[200] = 0;
	if (CATS_TYPE(sob->cats) == CAT_STAR_TYPE_APSTD) {
		if (sob->ofr->band >= 0 && sob->ost->smagerr[sob->ofr->band] < BIG_ERR) {
			snprintf(c+150, 40, "%5.2f", sob->ost->smag[sob->ofr->band]);
			snprintf(c+200, 40, "%.3f", sob->ost->smagerr[sob->ofr->band]);
		} else if (sob->ofr->band >= 0 && sob->err < BIG_ERR) {
			snprintf(c+150, 40, "[%5.2f]", sob->mag);
			snprintf(c+200, 40, "%.3f", sob->err);
		} 
	} else {
		if (sob->ofr->band >= 0 && sob->err < BIG_ERR) {
			snprintf(c+150, 40, "[%5.2f]", sob->mag);
			snprintf(c+200, 40, "%.3f", sob->err);
		}
	}
	snprintf(c+250, 40, "%6.2f", sob->imag);
	snprintf(c+300, 40, "%.3f", sob->imagerr);
	if ((sob->ofr->zpstate & ZP_STATE_M) >= ZP_ALL_SKY && sob->weight > 0.0) {
		snprintf(c+350, 40, "%7.3f", sob->residual);
		se = fabs(sob->residual * sqrt(sob->nweight));
		snprintf(c+400, 40, "%.3f", se);
		snprintf(c+450, 40, "%s", se > OUTLIER_THRESHOLD ? "Y" : "N");
	} else {
		c[350] = c[400] = c[450] = 0;
	}
	degrees_to_dms_pr(c+500, sob->cats->ra / 15.0, 2);
	degrees_to_dms_pr(c+550, sob->cats->dec, 1);
	i = 0; n = 0;
	flags = sob->flags & (CPHOT_MASK) & ~(CPHOT_NO_COLOR);
	while (cat_flag_names[i] != NULL) {
		if (flags & 0x01)
			n += snprintf(c+600+n, 100-n, "%s ", cat_flag_names[i]);
		if (n >= 100)
			break;
		flags >>= 1;
		i++;
	}
	for (i = 0; i < 13; i++)
		gtk_clist_set_text(GTK_CLIST(sob_list), row, i, c+50*i);
}

static void sob_list_update_vals(GtkWidget *sob_list)
{
	int i;
	for (i = 0; i < GTK_CLIST(sob_list)->rows; i++)
		sob_list_set_row_vals(sob_list, i, 
				      STAR_OBS(gtk_clist_get_row_data(GTK_CLIST(sob_list), i)));
}


static void sobs_to_list(GtkWidget *list, GList *sol)
{
	GList *sl;
	struct star_obs *sob;
	int row, i;
	char *line [] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
			 NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	gtk_clist_freeze(GTK_CLIST(list));
	sl = sol;
	while(sl != NULL) {
		sob = STAR_OBS(sl->data);
		sl = g_list_next(sl);
		row = gtk_clist_append(GTK_CLIST(list), line);
		sob_list_set_row_vals(list, row, sob);
		gtk_clist_set_row_data(GTK_CLIST(list), row, sob);
		sob->data = row;
	}
	for (i = 0; i < 13; i++) {
		gtk_clist_set_column_auto_resize(GTK_CLIST(list), i, 1);
		if (i > 0 && i < 12)
			gtk_clist_set_column_justification(GTK_CLIST(list), 
							   i, GTK_JUSTIFY_CENTER);
	}
	gtk_clist_thaw(GTK_CLIST(list));
}


static void sob_click_col_cb(GtkCList *clist, gint column, gpointer data)
{
	static int lastcol = -1;
	gtk_clist_set_sort_column(GTK_CLIST(clist), column);
	if (column == lastcol) {
		lastcol = -1;
		gtk_clist_set_sort_type(GTK_CLIST(clist), GTK_SORT_DESCENDING);
	} else {
		lastcol = column;
		gtk_clist_set_sort_type(GTK_CLIST(clist), GTK_SORT_ASCENDING);
	}
	gtk_clist_sort(GTK_CLIST(clist));
}

static void mb_rebuild_sob_list(gpointer dialog, GList *sol)
{
	GtkWidget *list;
	GtkScrolledWindow *scw;
	char *titles[] = {"Name", "Type", "Band", "Smag", "Err", "Imag", "Err", "Residual", 
			  "Std Error", "Outlier", "R.A.", "Declination", "Flags", NULL};

	list = gtk_object_get_data(GTK_OBJECT(dialog), "sob_list");
	if (list == NULL) {
		scw = gtk_object_get_data(GTK_OBJECT(dialog), "sob_scw");
		g_return_if_fail(scw != NULL);
		list = gtk_clist_new_with_titles(14, titles);
		gtk_scrolled_window_add_with_viewport(scw, list);
		gtk_widget_ref(list);
		gtk_object_set_data_full(GTK_OBJECT(dialog), "sob_list",
					 list, (GtkDestroyNotify) gtk_widget_destroy);
		gtk_clist_set_selection_mode(GTK_CLIST(list), GTK_SELECTION_EXTENDED);
		gtk_signal_connect(GTK_OBJECT(list), "click-column", sob_click_col_cb, dialog);
//		gtk_clist_set_column_justification(GTK_CLIST(list), 5, GTK_JUSTIFY_RIGHT);
//		gtk_clist_set_column_justification(GTK_CLIST(list), 3, GTK_JUSTIFY_RIGHT);
		gtk_widget_show(list);
	} else {
		gtk_clist_clear(GTK_CLIST(list));
	}
	sobs_to_list(list, sol);
}

void add_to_mband(gpointer dialog, char *fn)
{
	struct mband_dataset *mbds;
	FILE * inf;
	struct stf *stf;
	int ret, n=0;

//	d1_printf("loading report file: %s\n", fn);
	inf = fopen(fn, "r");
	if (inf == NULL) {
		mbds_printf(dialog, "Cannot open file %s for reading: %s\n", 
			   fn, strerror(errno));
		error_beep();
		return;
	} 

	mbds = gtk_object_get_data(GTK_OBJECT(dialog), "mbds");
	if (mbds == NULL) {
		mbds = mband_dataset_new();
		d3_printf("mbds: %p\n", mbds);
		mband_dataset_set_bands_by_string(mbds, P_STR(AP_BANDS_SETUP));
		gtk_object_set_data_full(GTK_OBJECT(dialog), "mbds",
					 mbds, (GtkDestroyNotify)(mband_dataset_release));
	}
	while (	(stf = stf_read_frame(inf)) != NULL ) {
//		d3_printf("----------------------------------read stf\n");
//		stf_fprint(stderr, stf, 0, 0);
		ret = mband_dataset_add_stf(mbds, stf);
		if (ret >= 0) {
			n++;
		} 
		mbds_printf(dialog, "%d frames read", n);
		while (gtk_events_pending ())
			gtk_main_iteration ();
		
	} 
	if (n == 0) {
		error_beep();
		mbds_printf(dialog, "%s: %s", fn, last_err());
		return;
	} else {
		mbds_printf(dialog, "Dataset has %d observation(s) %d object(s), %d frame(s)\n",
			    g_list_length(mbds->sobs), g_list_length(mbds->ostars),
			    g_list_length(mbds->ofrs));
	}

	mb_rebuild_ofr_list(dialog);
	mb_rebuild_bands_list(dialog);
}

gboolean mband_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gtk_widget_hide(widget);
//	gtk_object_set_data(GTK_OBJECT(data), "mband_window", NULL);
	return TRUE;
}
static void mb_wclose_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	mband_window_delete(data, NULL, NULL);
}

static GtkItemFactoryEntry mband_menu_items[] = {
	{ "/_File",		NULL,         	NULL,  		0, "<Branch>" },
	{ "/File/_Add to Dataset", "<control>o", 	file_popup_cb, 	
	  		FILE_ADD_TO_MBAND, "<Item>" },
	{ "/File/_Save Dataset", "<control>s", rep_cb, REP_DATASET | REP_ALL, "<Item>"},
	{ "/File/_Close Dataset", "<control>w",	mb_close_cb, 	0, "<Item>" },
	{ "/File/Sep", NULL, NULL,	0, "<Separator>" },
	{ "/File/Report _Targets in AAVSO Format", NULL, rep_cb, REP_TGT | REP_AAVSO, "<Item>" },
	{ "/File/Sep", NULL, NULL,	0, "<Separator>" },
	{ "/File/Close Window", "<control>q",	mb_wclose_cb, 	0, "<Item>" },

	{ "/_Edit", NULL, NULL,	0, "<Branch>" },
	{ "/_Edit/Select _All", "<control>A", select_cb, SEL_ALL, "<Item>" },
	{ "/_Edit/_Unselect All", "<control>U", select_cb, UNSEL_ALL, "<Item>" },
//	{ "/_Edit/Filter by Bands", NULL, NULL, 0, "<Item>" },
//	{ "/_Edit/Filter by Frames", NULL, NULL, 0, "<Item>" },
//	{ "/_Edit/Filter by Stars", NULL, NULL, 0, "<Item>" },
//	{ "/_Edit/Show Outliers Only", NULL, NULL, 0, "<Item>" },

	{ "/_Reduce", NULL, NULL,	0, "<Branch>" },
	{ "/_Reduce/Fit _Zero Points with Null Coefficients", NULL, fit_cb, FIT_ZPOINTS, "<Item>" },
	{ "/_Reduce/Fit Zero Points with _Current Coefficients", NULL, fit_cb, FIT_ZP_WTRANS, "<Item>" },
	{ "/_Reduce/Fit Zero Points and _Transformation Coefficients", NULL, fit_cb, FIT_TRANS, "<Item>" },
	{ "/_Reduce/Fit Extinction and All-Sky Zero points", NULL, fit_cb, FIT_ALL_SKY, "<Item>" },
//	{ "/Fit/Sep", NULL, NULL,	0, "<Separator>" },
//	{ "/Fit/Bands Setup...", NULL, NULL,	2, "<Item>" },

	{ "/_Plot", NULL, NULL,	0, "<Branch>" },
	{ "/_Plot/_Residuals vs Magnitude", NULL, plot_cb, PLOT_RES_SM, "<Item>" },
	{ "/_Plot/Residuals vs _Color", NULL, plot_cb, PLOT_RES_COL, "<Item>" },
	{ "/_Plot/Standard _Errors vs Magnitude", NULL, plot_cb, 
	  PLOT_RES_SM | PLOT_WEIGHTED, "<Item>" },
	{ "/_Plot/_Standard Errors vs Color", NULL, plot_cb,
	  PLOT_RES_COL | PLOT_WEIGHTED, "<Item>" },
	{ "/_Plot/Sep",	NULL, NULL, 0, "<Separator>" },
	{ "/_Plot/_Zeropoints vs Airmass", NULL, plot_cb, PLOT_ZP_AIRMASS, "<Item>" },
	{ "/_Plot/Zeropoints vs _Time", NULL, plot_cb, PLOT_ZP_TIME, "<Item>" },
	{ "/_Plot/Sep",	NULL, NULL, 0, "<Separator>" },
	{ "/_Plot/Star _Magnitude vs Time", NULL, plot_cb, PLOT_STAR, "<Item>" },
//	{ "/_Plot/_Options/Plot _Selected Frames", NULL, NULL, 0, "<RadioItem>" },
//	{ "/_Plot/_Options/Plot _All Frames", NULL, NULL, 0, "/Plot/Options/Plot Selected Frames" },
//	{ "/_Plot/_Options/Sep", NULL, NULL, 0, "<Separator>" },
//	{ "/_Plot/_Options/Plot to _Window", NULL, NULL, 0, "<RadioItem>" },
	{ "/_Plot/Sep",	NULL, NULL, 0, "<Separator>" },
	{ "/_Plot/Plot to _File", NULL, NULL, 0, "<ToggleItem>"},

//	{ "/File/_Close",	"<control>c", 	file_popup_cb, 	FILE_CLOSE, "<Item>" },
};


/* create the menu bar */
static GtkWidget *get_main_menu_bar(GtkWidget *window)
{
	GtkWidget *ret;
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;
	gint nmenu_items = sizeof (mband_menu_items) / 
		sizeof (mband_menu_items[0]);
	accel_group = gtk_accel_group_new ();

	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, 
					     "<main_menu>", accel_group);
	gtk_object_set_data_full(GTK_OBJECT(window), "main_menu_if", item_factory,
				 (GtkDestroyNotify) gtk_object_unref);
	gtk_item_factory_create_items (item_factory, nmenu_items, 
				       mband_menu_items, window);

  /* Attach the new accelerator group to the window. */
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

    /* Finally, return the actual menu bar created by the item factory. */ 
	ret = gtk_item_factory_get_widget (item_factory, "<main_menu>");
	return ret;
}

/* create / open the guiding dialog */
void mband_open_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *window = data;
	GtkWidget *dialog, *vb, *menubar;

	dialog = gtk_object_get_data(GTK_OBJECT(window), "mband_window");
	if (dialog == NULL) {
		dialog = create_mband_dialog();
		gtk_object_set_data_full(GTK_OBJECT(window), "mband_window",
					 dialog, (GtkDestroyNotify)(gtk_widget_destroy));
		gtk_object_set_data(GTK_OBJECT(dialog), "in_window", window);
		vb = gtk_object_get_data(GTK_OBJECT(dialog), "mband_vbox");
		gtk_signal_connect(GTK_OBJECT(dialog), "delete_event", 
				   GTK_SIGNAL_FUNC(mband_window_delete), window);
		menubar = get_main_menu_bar(dialog);
		gtk_box_pack_start(GTK_BOX(vb), menubar, FALSE, TRUE, 0);
		gtk_box_reorder_child(GTK_BOX(vb), menubar, 0);
		gtk_widget_show(menubar);
		gtk_widget_show(dialog);

	} else {
		gtk_widget_show(dialog);
		gdk_window_raise(dialog->window);
	}
}
