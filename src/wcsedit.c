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

/* wcs edit dialog */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <glob.h>
#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "sourcesdraw.h"
#include "params.h"
#include "interface.h"
#include "wcs.h"
#include "misc.h"

#define INV_DBL -10000.0


static int wcsentry_cb( GtkWidget *widget, gpointer data );

static void update_wcs_dialog(GtkWidget *dialog, struct wcs *wcs);
static void wcs_ok_cb( GtkWidget *widget, gpointer data );

static void close_wcsedit( GtkWidget *widget, gpointer data )
{
	g_return_if_fail(data != NULL);
	gtk_object_set_data(GTK_OBJECT(data), "wcs_dialog", NULL);
}

static void close_wcs_dialog( GtkWidget *widget, gpointer data )
{
	GtkWidget *im_window;
	im_window = gtk_object_get_data(GTK_OBJECT(data), "im_window");
	g_return_if_fail(im_window != NULL);
	gtk_object_set_data(GTK_OBJECT(im_window), "wcs_dialog", NULL);
}

/* show the current frame's fits header in a text window */
void wcsedit_cb(gpointer window, guint action, GtkWidget *menu_item)
{
	GtkWidget *dialog;
//	struct image_channel *i_chan;
	struct wcs *wcs;

/*
	i_chan = gtk_object_get_data(GTK_OBJECT(window), "i_channel");
	if (i_chan == NULL || i_chan->fr == NULL) {
		error_message_sb2(window, "No frame loaded");
		return;
	}
*/
	wcs = gtk_object_get_data(GTK_OBJECT(window), "wcs_of_window");
	if (wcs == NULL) {
		wcs = wcs_new();
		gtk_object_set_data_full(GTK_OBJECT(window), "wcs_of_window", 
					 wcs, (GtkDestroyNotify)wcs_release);
	}

	dialog = gtk_object_get_data(GTK_OBJECT(window), "wcs_dialog");
	if (dialog == NULL) {
		dialog = create_wcs_edit();
		gtk_object_set_data(GTK_OBJECT(dialog), "im_window",
					 window);
		gtk_object_set_data_full(GTK_OBJECT(window), "wcs_dialog",
					 dialog, (GtkDestroyNotify)(gtk_widget_destroy));
		gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
				    GTK_SIGNAL_FUNC (close_wcsedit), window);
		set_named_callback (GTK_OBJECT (dialog), "wcs_close_button", "clicked",
				    GTK_SIGNAL_FUNC (close_wcs_dialog));
		set_named_callback (GTK_OBJECT (dialog), "wcs_ok_button", "clicked",
				    GTK_SIGNAL_FUNC (wcs_ok_cb));
		set_named_callback(dialog, "wcs_ra_entry", "activate", wcsentry_cb);
		set_named_callback(dialog, "wcs_dec_entry", "activate", wcsentry_cb);
		set_named_callback(dialog, "wcs_h_scale_entry", "activate", wcsentry_cb);
		set_named_callback(dialog, "wcs_v_scale_entry", "activate", wcsentry_cb);
		set_named_callback(dialog, "wcs_equinox_entry", "activate", wcsentry_cb);
		set_named_callback(dialog, "wcs_rot_entry", "activate", wcsentry_cb);
		update_wcs_dialog(dialog, wcs);
		gtk_widget_show(dialog);
	} else {
//		update_fits_header_dialog(dialog, i_chan->fr);
		update_wcs_dialog(dialog, wcs);
		gdk_window_raise(dialog->window);
	}
}

void wcsedit_refresh(gpointer window)
{
	GtkWidget *dialog;
	struct wcs *wcs;

	dialog = gtk_object_get_data(GTK_OBJECT(window), "wcs_dialog");
	if (dialog == NULL) {
		return;
	}
	wcs = gtk_object_get_data(GTK_OBJECT(window), "wcs_of_window");
	if (wcs == NULL) {
		return;
	}
	update_wcs_dialog(dialog, wcs);
}


static void update_wcs_dialog(GtkWidget *dialog, struct wcs *wcs)
{
	char buf[256];

	switch(wcs->wcsset) {
	case WCS_INVALID:
		set_named_checkb_val(dialog, "wcs_unset_rb", 1);
/* also clear the fields here */
		return;		
	case WCS_INITIAL:
		set_named_checkb_val(dialog, "wcs_initial_rb", 1);
		break;		
	case WCS_FITTED:
		set_named_checkb_val(dialog, "wcs_fitted_rb", 1);
		break;		
	case WCS_VALID:
		set_named_checkb_val(dialog, "wcs_valid_rb", 1);
		break;		
	}
	degrees_to_dms_pr(buf, wcs->xref / 15.0, 2);
	named_entry_set(dialog, "wcs_ra_entry", buf);
	degrees_to_dms_pr(buf, wcs->yref, 1);
	named_entry_set(dialog, "wcs_dec_entry", buf);
	snprintf(buf, 255, "%d", wcs->equinox);
	named_entry_set(dialog, "wcs_equinox_entry", buf);
	snprintf(buf, 255, "%.6f", 3600 * wcs->xinc);
	named_entry_set(dialog, "wcs_h_scale_entry", buf);
	snprintf(buf, 255, "%.6f", 3600 * wcs->yinc);
	named_entry_set(dialog, "wcs_v_scale_entry", buf);
	snprintf(buf, 255, "%.4f", wcs->rot);
	named_entry_set(dialog, "wcs_rot_entry", buf);
}

/* push values from the dialog back into the wcs
 * if some values are missing, we try to guess them
 * return 0 if we could set the wcs from values, 1 if some
 * values were defaulted, 2 if there were changes, 
 * -1 if we didn;t have enough data*/
static int wcs_dialog_to_wcs(GtkWidget *dialog, struct wcs *wcs)
{
	char *text = NULL, *end;
	double ra = INV_DBL, dec = INV_DBL, equ = INV_DBL, 
		xs = INV_DBL, ys = INV_DBL, rot = INV_DBL, d;
	int ret = 0, chg = 0;

/* parse the fields */
	text = named_entry_text(dialog, "wcs_ra_entry");
//	d3_printf("ra text is |%s|\n", text);
	if (!dms_to_degrees(text, &d)) 
		ra = d * 15.0;
//	d3_printf("got %.4f for ra\n", ra);
	g_free(text);
	text = named_entry_text(dialog, "wcs_dec_entry");
	if (!dms_to_degrees(text, &d)) 
		dec = d ;
//	d3_printf("got %.4f for dec\n", dec);
	g_free(text);
	text = named_entry_text(dialog, "wcs_equinox_entry");
	d = strtod(text, &end);
	if (text != end)
		equ = d ;
	g_free(text);
	text = named_entry_text(dialog, "wcs_h_scale_entry");
	d = strtod(text, &end);
	if (text != end)
		xs = d ;
	g_free(text);
	text = named_entry_text(dialog, "wcs_v_scale_entry");
	d = strtod(text, &end);
	if (text != end)
		ys = d ;
	g_free(text);
	text = named_entry_text(dialog, "wcs_rot_entry");
	d = strtod(text, &end);
	if (text != end)
		rot = d ;
	g_free(text);
/* now see what we can do with them */
	if (ra == INV_DBL || dec == INV_DBL) {
		err_printf("cannot set wcs: invalid ra/dec\n");
		return -1; /* can't do anything without those */
	}
	if (clamp_double(&ra, 0, 360.0))
		ret = 1;
	if (clamp_double(&dec, -90.0, 90.0))
		ret = 1;
	if (equ == INV_DBL) {
		equ = 2000.0;
		ret = 1;
	}
	if (xs == INV_DBL) {
		if (ys == INV_DBL) 
			xs = -P_DBL(WCS_SEC_PER_PIX);
		else
			xs = ys;
		ret = 1;
	}
	if (ys == INV_DBL) {
		if (xs == INV_DBL) 
			ys = -P_DBL(WCS_SEC_PER_PIX);
		else
			ys = xs;
		ret = 1;
	}
	if (rot == INV_DBL) {
		rot = 0.0;
		ret = 1;
	}
	if (clamp_double(&rot, 0, 360.0))
		ret = 1;
/* set chg if the wcs has been changed significantly */
//	d3_printf("diff is %f\n", fabs(ra - wcs->xref));
	if (fabs(ra - wcs->xref) > (1.5 / 36000)) {
		chg = 1;
		wcs->xref = ra;
	}
	if (fabs(dec - wcs->yref) > (1.0 / 36000)) {
		chg = 2;
		wcs->yref = dec;
	}
	if (fabs(rot - wcs->rot) > (1.0 / 9900)) {
		chg = 3;
		wcs->rot = rot;
	}
	if (fabs(equ - 1.0*wcs->equinox) > 2) {
		chg = 4;
		wcs->equinox = equ;
	}
	if (fabs(xs - 3600 * wcs->xinc) > (1.0 / 990000)) {
		chg = 5;
		wcs->xinc = xs / 3600.0;
	}
	if (fabs(ys - 3600 * wcs->yinc) > (1.0 / 990000)) {
		chg = 6;
		wcs->yinc = ys / 3600.0;
	}
	if (chg || ret)
		wcs->wcsset = WCS_INITIAL;
	if (ret)
		return ret;
	if (chg) {
//		d3_printf("chg is %d\n", chg);
		return 2;
	}
	return 0;
}

static void wcs_ok_cb(GtkWidget *wid, gpointer dialog)
{
	int ret;

	ret = wcsentry_cb(NULL, dialog);
	if (ret == 0)
		close_wcs_dialog(NULL, dialog);
}

/* called on entry activate */
static int wcsentry_cb(GtkWidget *widget, gpointer dialog)
{
	GtkWidget *window;
	int ret;
	struct wcs *wcs;
	struct gui_star_list *gsl;

	window = gtk_object_get_data(GTK_OBJECT(dialog), "im_window");
	g_return_val_if_fail(window != NULL, 0);
	wcs = gtk_object_get_data(GTK_OBJECT(window), "wcs_of_window");
	g_return_val_if_fail(wcs != NULL, 0);

	ret = wcs_dialog_to_wcs(dialog, wcs);
//	d3_printf("wdtw returns %d\n", ret);
	if (ret > 0) {
		update_wcs_dialog(dialog, wcs);
		warning_beep();
		gsl = gtk_object_get_data(GTK_OBJECT(window), "gui_star_list");
		if (gsl != NULL) {
			cat_change_wcs(gsl->sl, wcs);
		} 
		gtk_widget_queue_draw(window);
	} else if (ret < 0) {
		error_beep();
	}
	return ret;
}

void wcs_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *window = data;
	struct image_channel *i_chan;
	struct wcs *wcs;
	struct gui_star_list *gsl;
	switch(action) {
	case WCS_AUTO:
/* we just do the "sgpw" here */
		find_stars_cb(data, ADD_STARS_DETECT, NULL);
		find_stars_cb(data, ADD_STARS_GSC, NULL);
		find_stars_cb(data, ADD_STARS_TYCHO2, NULL);
		if (window_auto_pairs(data) < 1)
			return;
	/* fallthrough */
	case WCS_FIT:
		window_fit_wcs(window);
		gtk_widget_queue_draw(window);
		break;
	case WCS_QUIET_AUTO:
/* we just do the "sgpw<shift>f<shift>s" here */
		wcs = gtk_object_get_data(GTK_OBJECT(data), "wcs_of_window");
		if (wcs != NULL && wcs->wcsset == WCS_VALID) 
			break;
		find_stars_cb(data, ADD_STARS_DETECT, NULL);
		find_stars_cb(data, ADD_STARS_GSC, NULL);
		find_stars_cb(data, ADD_STARS_TYCHO2, NULL);
		if (window_auto_pairs(data) < 1)
			return;
		window_fit_wcs(window);
		star_rm_cb(data, STAR_RM_FIELD, NULL);
		star_rm_cb(data, STAR_RM_FR, NULL);
		break;
	case WCS_RELOAD:
		wcs = gtk_object_get_data(GTK_OBJECT(window), "wcs_of_window");
		i_chan = gtk_object_get_data(GTK_OBJECT(window), "i_channel");
		if (wcs == NULL || i_chan == NULL || i_chan->fr == NULL)
			break;
		wcs_from_frame(i_chan->fr, wcs);
		gsl = gtk_object_get_data(GTK_OBJECT(window), "gui_star_list");
		if (gsl != NULL) {
			cat_change_wcs(gsl->sl, wcs);
		} 
		gtk_widget_queue_draw(window);
		break;
	case WCS_FORCE_VALID:
		wcs = gtk_object_get_data(GTK_OBJECT(window), "wcs_of_window");
		if (wcs != NULL) {
			wcs->wcsset = WCS_VALID;
		}
		break;
	default:
		err_printf("unknown action %d in wcs_cb\n", action);
	}
	wcsedit_refresh(window);
}

/* a simulated wcs 'auto match' command */
/* should return -1 if no match found */
int match_field_in_window(void * image_window)
{
	struct wcs *wcs;
	wcs_cb(image_window, WCS_AUTO, NULL);
	wcs = gtk_object_get_data(GTK_OBJECT(image_window), "wcs_of_window");
	if (wcs == NULL) {
		err_printf("No WCS found\n");
		return -1;
	}
	if (wcs->wcsset != WCS_VALID) {
		err_printf("Cannot match field\n");
		return -1;
	}
	return 0;
}

/* a simulated wcs 'quiet auto match' command */
/* returns -1 if no match found */
int match_field_in_window_quiet(void * image_window)
{
	struct wcs *wcs;
	wcs_cb(image_window, WCS_QUIET_AUTO, NULL);
	wcs = gtk_object_get_data(GTK_OBJECT(image_window), "wcs_of_window");
	if (wcs == NULL) {
		err_printf("No WCS found\n");
		return -1;
	}
	if (wcs->wcsset != WCS_VALID) {
		err_printf("Cannot match field\n");
		return -1;
	}
	return 0;
}
