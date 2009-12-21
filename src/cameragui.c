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

/* camera/telescope control dialog */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <libgen.h>
#include <ctype.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "camera.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "params.h"
#include "obslist.h"
#include "cameragui.h"
#include "interface.h"
#include "misc.h"
#include "fwheel.h"

#define AUTO_FILE_FORMAT "%s%03d"
#define DEFAULT_FRAME_NAME "frame"
#define DEFAULT_DARK_NAME "dark"

static void auto_filename(GtkWidget *dialog);

void test_camera_open(void)
{
	struct ccd *ccd;
	char **cameras;

	cameras = find_cameras();
	if (cameras == NULL || cameras[0] == NULL)
		return;

	d3_printf("trying to open camera: %s\n", cameras[0]);

	ccd = ccd_open(cameras[0]);
	if (ccd == NULL) {
		err_printf("cannot open camera\n");
		return;
	}
	d3_printf("camera %s is open\n", cameras[0]);
	ccd_read_info(ccd, &ccd->info);
	d3_printf("info read\n");
	ccd_read_status(ccd, &ccd->stat);
	d3_printf("status read\n");
	ccd_read_exposure(ccd, &ccd->exp);
	ccd_close(ccd);
	d3_printf("closed");
	return;
}

static void status_message(GtkWidget *window, char *message, char *context, int time)
{
	GtkWidget *statusbar;
	guint msg_id, context_id;
	gpointer stref;

	statusbar = gtk_object_get_data(GTK_OBJECT(window), "cam_status");
	if (statusbar != NULL) {
		context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar),
						  context );
		msg_id = gtk_statusbar_push(GTK_STATUSBAR(statusbar), context_id, 
				 message);
	if (time) {
			stref = new_status_ref(statusbar, context_id, msg_id);
			gtk_timeout_add(time, (GtkFunction)remove_status_msg, stref);
		}
	}
}
/*
 * pop a message from statusbar 1
 */
static void status_pop(GtkWidget *window, char *context)
{
	GtkWidget *statusbar;
	guint context_id;

	statusbar = gtk_object_get_data(GTK_OBJECT(window), "cam_status");
	if (statusbar) {
		context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar),
						  context );
		gtk_statusbar_pop(GTK_STATUSBAR(statusbar), context_id);
	}
}


void cd_find_cameras(GtkWidget *dialog, GtkWidget *combo)
{
	char **found;
	GList *cam_list = NULL;
	found = find_cameras();
	if (found == NULL)
		return;
	while (*found != NULL) {
		cam_list = g_list_append(cam_list, *found);
		found ++;
	}
	gtk_signal_handler_block_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
	gtk_list_clear_items(GTK_LIST(GTK_COMBO(combo) -> list), 0, -1); 
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), cam_list);
	gtk_signal_handler_unblock_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
}

/* select and open a ccd to be used as default for the 
 * newly created dialog */
void cd_default_ccd(GtkWidget *dialog)
{
	GtkWidget *combo;

	combo = gtk_object_get_data(GTK_OBJECT(dialog), "cameras_combo");
	if (combo == NULL) {
		g_warning("no cameras_combo");
		return;
	}
	cd_find_cameras(dialog, combo);
	gtk_list_select_item(GTK_LIST(GTK_COMBO(combo) -> list) , 0);
}

/* try to open the given camera (if not already open)
 * and add it to the dialog's data
 */
struct ccd * try_open_camera(char *url, GtkWidget *dialog)
{
	struct ccd *ccd;
	int ret;

	ccd = gtk_object_get_data(GTK_OBJECT(dialog), "open_camera");
	if (ccd != NULL) {
#if 0
		if (!strcasecmp(ccd->url, url))
			return ccd;
#endif
		ccd_close(ccd);
		gtk_object_set_data(GTK_OBJECT(dialog), "open_camera", NULL);
	}
	ccd = ccd_open(url);
	if (ccd != NULL) {
		ret = ccd_read_info(ccd, &(ccd->info));
		if (ret < 0) {
			status_message(dialog, "Error reading info, aborting", "open", 3000);
			goto close_and_exit;
		}
		ret = ccd_read_exposure(ccd, &(ccd->exp));
		if (ret < 0) {
			status_message(dialog, "Error reading exp, aborting", "open", 3000);
			goto close_and_exit;
		}
		ret = ccd_read_status(ccd, &(ccd->stat));
		if (ret < 0) {
			status_message(dialog, "Error reading status, aborting", "open", 3000);
			goto close_and_exit;
		}
		ret = ccd_read_cooler(ccd, &(ccd->cooler));
		if (ret < 0) {
			status_message(dialog, "Error reading cooler status, aborting", 
				       "open", 3000);
			goto close_and_exit;
		}
		gtk_object_set_data(GTK_OBJECT(dialog), "open_camera", ccd);
		status_message(dialog, "Camera Opened", "open", 3000);
		return ccd;
	}
	error_beep();
	status_message(dialog, "Cannot Open", "open", 3000);
	return NULL;
close_and_exit:
	error_beep();
	ccd_close(ccd);
	return NULL;
}




static void named_spin_set_limits(GtkWidget *dialog, char *name, double min, double max)
{
	GtkWidget *spin;
	GtkAdjustment *adj;
	g_return_if_fail(dialog != NULL);
	spin = gtk_object_get_data(GTK_OBJECT(dialog), name);
	g_return_if_fail(spin != NULL);
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	g_return_if_fail(adj != NULL);
	adj->lower = min;
	adj->upper = max;
	gtk_adjustment_changed(GTK_ADJUSTMENT(adj));
}

/* get the values from cam into the image dialog page */
void cam_to_img(GtkWidget *dialog)
{
	char buf[256];
	struct ccd * ccd;
	int mxsk, mysk;

	ccd = gtk_object_get_data(GTK_OBJECT(dialog), "open_camera");
	if (ccd == NULL)
		return;

	sprintf(buf, "%dx%d", ccd->exp.bin_x, ccd->exp.bin_y);
	named_entry_set(dialog, "img_bin_combo_entry", buf);
	named_spin_set_limits(dialog, "img_exp_spin", ccd->info.min_exp, ccd->info.max_exp);
	named_spin_set(dialog, "img_exp_spin", ccd->exp.exp_time);
	named_spin_set_limits(dialog, "img_width_spin", 0, 1.0 * ccd->info.active_pixels 
			     / ccd->exp.bin_x);
	named_spin_set(dialog, "img_width_spin", 1.0 * ccd->exp.w);
	named_spin_set_limits(dialog, "img_height_spin", 0, 1.0 * ccd->info.active_lines 
			     / ccd->exp.bin_y);
	named_spin_set(dialog, "img_height_spin", 1.0 * ccd->exp.h);
	mxsk = ccd->info.active_pixels - ccd->exp.w * ccd->exp.bin_x;
	mysk = ccd->info.active_lines - ccd->exp.h * ccd->exp.bin_y;
	named_spin_set_limits(dialog, "img_x_skip_spin", 0, 1.0 * mxsk);
	named_spin_set(dialog, "img_x_skip_spin", 1.0 * ccd->exp.x_skip);
	named_spin_set_limits(dialog, "img_y_skip_spin", 0, 1.0 * mysk);
	named_spin_set(dialog, "img_y_skip_spin", 1.0 * ccd->exp.y_skip);
}

/* get the values from cam into the cooler dialog page */
static void cam_to_cooler(GtkWidget *dialog)
{
	char buf[64];
	struct ccd * ccd;

	ccd = gtk_object_get_data(GTK_OBJECT(dialog), "open_camera");
	if (ccd == NULL)
		return;

	named_spin_set(dialog, "cooler_tempset_spin", ccd->cooler.set_temp);
	sprintf(buf, "%.1f", ccd->cooler.temp);
	named_entry_set(dialog, "cooler_temp_entry", buf);
	sprintf(buf, "%.1f", ccd->cooler.cooling_power);
	named_entry_set(dialog, "cooler_power_entry", buf);
}

/* pretend to read an image and just dump the data */
static void dump_image_data(struct ccd *ccd)
{
	unsigned char imb[65536];
	int ret;
	do {
		ret = ccd_read_frame(ccd, imb, 0x10000);
		if (ret <= 0)
			d3_printf("read %d\n", ret);
		gtk_main_iteration_do(FALSE);
	} while (ret > 0);
}


/* convert the frame and show it to the given image window */
static void show_the_frame(struct ccd_frame *fr, GtkWidget *imw)
{
	frame_stats(fr);
	frame_to_channel(fr, imw, "i_channel");
}

/* save a frame with the name specified by the dialog; increment seq number, etc */
void save_frame_auto_name(struct ccd_frame *fr, GtkWidget *dialog)
{
	char mb[1024];
	int seq;
	char fn[1024];
	char *text;

	auto_filename(dialog);
	text = named_entry_text(dialog, "file_entry");
	if (text == NULL || strlen(text) < 1) {
		if (text != NULL)
			g_free(text);
		text = g_strdup(DEFAULT_FRAME_NAME);
		named_entry_set(dialog, "file_entry", text);
	}
	seq = named_spin_get_value(dialog, "file_seqn_spin");
	check_seq_number(text, &seq);
	snprintf(fn, 1023, AUTO_FILE_FORMAT, text, seq);
	g_free(text);
	snprintf(mb, 1023, "Writing fits file: %s", fn);
	status_message(dialog, mb, "file", 1000);
	d1_printf("%s\n", mb);
	wcs_to_fits_header(fr);
	if (get_named_checkb_val(GTK_WIDGET(dialog), 
				 "file_compress_checkb")) {
		write_gz_fits_frame(fr, fn, P_STR(FILE_COMPRESS));
	} else {
		write_fits_frame(fr, fn);
	}
	seq ++;
	named_spin_set(dialog, "file_seqn_spin", seq);
}

static void dither_move(gpointer dialog, double amount)
{
	double dr, dd;
	char msg[128];

	do {
		dr = amount / 60.0 * (1.0 * random() / RAND_MAX - 0.5);
		dd = amount / 60.0 * (1.0 * random() / RAND_MAX - 0.5);
	} while (sqr(dr) + sqr(dd) < sqr(0.2 * amount / 60.0));

	snprintf(msg, 127, "Dither move: %.1f %.1f", 3600 * dr, 3600 * dd);
	status_message(dialog, msg, "obs", 1000);

	if (P_DBL(TELE_GUIDE_SPEED) > 0.1) {
		dr = dr * 3600000 / 15.0 / P_DBL(TELE_GUIDE_SPEED);
		dd = dd * 3600000 / 15.0 / P_DBL(TELE_GUIDE_SPEED);
	} else {
		dr = dr * 3600000 / 15.0;
		dd = dd * 3600000 / 15.0;
	}
	lx200_guide_move(dr, dd);

}

static void scope_dither_cb( GtkWidget *widget, gpointer data )
{
	dither_move(data, P_DBL(TELE_DITHER_AMOUNT));
}


/* see if we need to save the frame and save it if we do; update names and
 * start the next frame if required */
static void maybe_save_frame(struct ccd_frame *fr, GtkWidget *dialog)
{
	GtkWidget *togb, *imwin;
	char mb[1024];
	int seq;
	char *text;
	struct ccd *ccd;

	imwin = gtk_object_get_data(GTK_OBJECT(dialog), "image_window");
	togb = gtk_object_get_data(GTK_OBJECT(dialog), "img_get_multiple_button");
	if (togb != NULL && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togb))) {
		if (imwin != NULL 
		    && get_named_checkb_val(GTK_WIDGET(dialog), "file_match_wcs_checkb")
		    && !get_named_checkb_val(GTK_WIDGET(dialog), "img_dark_checkb"))
			match_field_in_window_quiet(imwin);
		save_frame_auto_name(fr, dialog);
/* restart exposure if needed */
		if (P_INT(TELE_DITHER_ENABLE)) {
			dither_move(dialog, P_DBL(TELE_DITHER_AMOUNT));
		}
		text = named_entry_text(dialog, "current_frame_entry");
		seq = strtol(text, NULL, 10);
		g_free(text);
		if (seq > 0) {
			seq --;
			if (seq > 0) {
				ccd = gtk_object_get_data(GTK_OBJECT(dialog), "open_camera");
				if (ccd != NULL) {
					set_exp_from_img_dialog(ccd, GTK_WIDGET(dialog), NULL);
					ccd_set_exposure(ccd, &ccd->exp);
					ccd_read_exposure(ccd, &ccd->exp);
					ccd_start_exposure
						(ccd, get_named_checkb_val(GTK_WIDGET(dialog), 
									   "img_dark_checkb"));
				} else {
					seq = 0;
				}
			}
			sprintf(mb, "%d", seq);
			named_entry_set(dialog, "current_frame_entry", mb);
		}
		if (seq == 0) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togb), 0);
		}
		return;
	}
	togb = gtk_object_get_data(GTK_OBJECT(dialog), "img_focus_button");
	if (togb != NULL && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togb))) {
		ccd = gtk_object_get_data(GTK_OBJECT(dialog), "open_camera");
		if (ccd != NULL) {
			set_exp_from_img_dialog(ccd, GTK_WIDGET(dialog), NULL);
			ccd_set_exposure(ccd, &ccd->exp);
			ccd_read_exposure(ccd, &ccd->exp);
			ccd_start_exposure
				(ccd, get_named_checkb_val(GTK_WIDGET(dialog), 
							   "img_dark_checkb"));
		}
	}
}

/* check if the filter wheel has a pending operation and run
 * poll on it if it does; return 0 if the fw is idle */
int fwheel_poll(gpointer dialog)
{
	struct fwheel *fw;
	GtkWidget *combo;
	int ostat;

	fw = gtk_object_get_data(GTK_OBJECT(dialog), "open_fwheel");
	combo = gtk_object_get_data(GTK_OBJECT(dialog), "obs_filter_combo");
		
	if ((fw == NULL)) {
		gtk_widget_set_sensitive(combo, 1);
		return 0;
	}
	ostat = fw->state;
	if ((fwheel_poll_status(fw) == FWHEEL_READY)) {
		if (ostat != fw->state) {
			status_pop(dialog, "status");
			status_message(dialog, "Filter change complete", "status", 0);
		}
		gtk_widget_set_sensitive(combo, 1);
		return 0;
	}

	if (fw->state == FWHEEL_ERROR) {
		err_printf("fwheel error\n");
		status_message(dialog, "Filter wheel error, please reconnect", "status", 5000);
		error_beep();
		fwheel_close(fw);
		gtk_object_set_data(GTK_OBJECT(dialog), "open_fwheel", NULL);
		gtk_widget_set_sensitive(combo, 1);
		return 0;
	}
	return 1;
}

/* the camera control state machine timeout function 
 * called 10 times/sec, it polls the camera's status and 
 * updates the dialog. Also reads images and handles their 
 * display/saving */

/* we also call the filter wheel control and the obslist 
 * state machine from here */

gint camera_sm(gpointer data)
{
	GtkWidget *dialog = data;
	struct ccd *ccd;
	struct ccd_frame *fr;
	struct obs_data *obs;
	GtkWidget *imwin;
	char buf[64];
	int ret, all;
	void *frp;
	static int ostat = -1;

	ccd = gtk_object_get_data(GTK_OBJECT(dialog), "open_camera");
	if (ccd == NULL) {
		if (ostat != 0) {
			status_pop(dialog, "status");
			status_message(dialog, "No CCD", "status", 0);
		}
		ostat = 0;
		/* run the obs list even if we don't have a camera */
		if (!fwheel_poll(dialog))
			obs_list_sm(dialog);
		return TRUE;
	}
	fr = gtk_object_get_data(GTK_OBJECT(dialog), "current_frame");
	imwin = gtk_object_get_data(GTK_OBJECT(dialog), "image_window");
	obs = gtk_object_get_data(GTK_OBJECT(dialog), "obs_data");

	ccd_read_cooler(ccd, &ccd->cooler);
	cam_to_cooler(dialog);

	ret = ccd_read_status(ccd, &ccd->stat);
	if (ccd->stat.state != ostat || ccd->stat.state > CAM_IDLE) {
		cam_status_string(ccd, buf, 63);
		status_pop(dialog, "status");
		status_message(dialog, buf, "status", 0);
		ostat = ccd->stat.state;
	}

	if (gtk_main_level() > 1)
		return TRUE;

	if (ccd->stat.state == CAM_READ_END) {
		if (fr == NULL) {
			dump_image_data(ccd);
		} else {
			frp = fr->dat;
			all = fr->w * fr->h * 2; /* we download at 16bpp */
			ccd_read_exposure(ccd, &ccd->exp);
			ccd_frame_set_exp_data(fr, ccd);
			if (!ccd->exp.dark)
				ccd_frame_add_obs_info(fr, obs);
			do {
			        ret = ccd_read_frame(ccd, frp, all);

				if (ret <= 0)
					d3_printf("read %d\n", ret);
				else {
					frp += ret;
					all -= ret;
				}
				gtk_main_iteration_do(FALSE);
			} while (ret > 0 && all > 0);
			fr->data_valid = frp - fr->dat;
			if (imwin != NULL) {
				show_the_frame(fr, imwin);
			}
			maybe_save_frame(fr, dialog);
		}
	} 
	if (ccd->stat.state == CAM_IDLE) {
/* run obs list processing state machine */
		if (!fwheel_poll(dialog))
			obs_list_sm(dialog);
	}
	return TRUE;
}


/* a new camera url is entered, try to open it */
void camera_select_cb( GtkWidget *widget, gpointer data )
{
	GtkCombo *combo;
	char *url;
	struct ccd *ccd;

	combo = gtk_object_get_data(GTK_OBJECT(data), "cameras_combo");
	url = gtk_editable_get_chars(GTK_EDITABLE(combo->entry), 0, -1);
	ccd = try_open_camera(url, data);
	g_free(url);
/* change the window label */
	if (ccd == NULL) {
		gtk_window_set_title(GTK_WINDOW(data), "Camera and Telescope Control");
	} else {
		gtk_window_set_title(GTK_WINDOW(data), ccd->info.name);
	}
}
void camera_select2_cb( GtkList *list, GtkWidget *widget, gpointer data )
{
	camera_select_cb(widget, data);
}



/*
 * filter wheel code
 */
static void set_default_filter_list(GtkWidget *dialog)
{
	GtkWidget *combo;
	GList *filter_list = NULL, *fl;
	char *text, *start, *end, *filt;
	int token;

	combo = gtk_object_get_data(GTK_OBJECT(dialog), "obs_filter_combo");
	g_return_if_fail(combo != NULL);
	gtk_combo_disable_activate(GTK_COMBO(combo));
	text = P_STR(OBS_FILTER_LIST);
	while (*text != 0) {
		token = next_token(&text, &start, &end);
//			d3_printf("token %d text %x start %x end %x\n", token, text, start, end);
		if (token == TOK_WORD || token == TOK_STRING || token == TOK_NUMBER) {
			if (end > start) {
				filt = malloc(end-start+1);
				strncpy(filt, start, end-start);
				filter_list = g_list_append(filter_list, filt);
			}
		}
	}
	if (filter_list != NULL) {
		gtk_signal_handler_block_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
		gtk_list_clear_items(GTK_LIST(GTK_COMBO(combo)->list), 0, -1); 
		gtk_combo_set_popdown_strings(GTK_COMBO(combo), filter_list);
		gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry), 1);
		gtk_signal_handler_unblock_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
	}
	for (fl = filter_list; fl != NULL; fl = fl->next) {
		free(fl->data);
	}
	g_list_free(filter_list);
}

static void set_filter_list(GtkWidget *dialog, struct fwheel *fw)
{
	GtkWidget *combo;
	GList *filter_list = NULL;
	char **filters;

	combo = gtk_object_get_data(GTK_OBJECT(dialog), "obs_filter_combo");
	g_return_if_fail(combo != NULL);
	gtk_combo_disable_activate(GTK_COMBO(combo));
	filters = fwheel_get_filter_names(fw);
	while (*filters != NULL) {
		filter_list = g_list_append(filter_list, *filters);
		filters ++;
	}
	if (filter_list != NULL) {
		gtk_signal_handler_block_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
		gtk_list_clear_items(GTK_LIST(GTK_COMBO(combo) -> list), 0, -1); 
		gtk_combo_set_popdown_strings(GTK_COMBO(combo), filter_list);
		gtk_list_select_item(GTK_LIST(GTK_COMBO(combo) -> list), fw->filter);
		gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry), 0);
		gtk_signal_handler_unblock_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
	}
}

static void filter_list_select_cb(GtkList *list, GtkWidget *widget, 
				  gpointer dialog)
{
	int pos, ret;
	struct fwheel *fw;
	GtkWidget *combo;
	char msg[128];

	fw = gtk_object_get_data(GTK_OBJECT(dialog), "open_fwheel");
	if (fw == NULL) 
		return;
	pos = gtk_list_child_position(list, widget);
	if (pos == fw->filter)
		return;
	combo = gtk_object_get_data(GTK_OBJECT(dialog), "obs_filter_combo");
	if (fw->state != FWHEEL_READY) {
		gtk_signal_handler_block_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
		gtk_list_select_item(GTK_LIST(GTK_COMBO(combo) -> list), fw->filter);
		gtk_signal_handler_unblock_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
		return;
	}
	ret = fwheel_goto_filter(fw, pos);
	if (!ret) {
		snprintf(msg, 128, "Selecting filter %d", pos);
		status_pop(dialog, "status");
		status_message(dialog, msg, "status", 0);
	} else {
		error_beep();
		/* reverting switching to the true filter */
		gtk_signal_handler_block_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
		gtk_list_select_item(GTK_LIST(GTK_COMBO(combo) -> list), fw->filter);
		gtk_signal_handler_unblock_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
	}
	gtk_widget_set_sensitive(combo, 0);
}


/* try to open the given fwheel (if not already open)
 * and add it to the dialog's data
 */
static struct fwheel * try_open_fwheel(char *url, GtkWidget *dialog)
{
	struct fwheel *fw;

	d3_printf("trying to open fwheel %s\n", url);
	fw = gtk_object_get_data(GTK_OBJECT(dialog), "open_fwheel");
	if (fw != NULL) {
		fwheel_close(fw);
		gtk_object_set_data(GTK_OBJECT(dialog), "open_fwheel", NULL);
	}
	fw = fwheel_open(url);
	if (fw != NULL) {
		d3_printf("current filter is %d\n", fwheel_get_filter(fw));
		set_filter_list(dialog, fw);
		gtk_object_set_data(GTK_OBJECT(dialog), "open_fwheel", fw);
		status_message(dialog, "Filter wheel opened successfully", "open", 3000);
		return fw;
	} else {
		set_default_filter_list(dialog);
		error_beep();
		status_message(dialog, "Cannot open filter wheel", "open", 3000);
		return NULL;
	}
}

void cd_find_fwheels(GtkWidget *dialog, GtkWidget *combo)
{
	char **found;
	GList *cam_list = NULL;
	found = find_fwheels();
	if (found == NULL)
		return;
	while (*found != NULL) {
		cam_list = g_list_append(cam_list, *found);
		found ++;
	}
	gtk_signal_handler_block_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
	gtk_list_clear_items(GTK_LIST(GTK_COMBO(combo) -> list), 0, -1); 
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), cam_list);
	gtk_signal_handler_unblock_by_data(GTK_OBJECT(GTK_COMBO(combo)->list), dialog);
}

/* select and open a filter wheel to be used as default for the 
 * newly created dialog */
void cd_default_fwheel(GtkWidget *dialog)
{
	GtkWidget *combo;

	combo = gtk_object_get_data(GTK_OBJECT(dialog), "fwheel_combo");
	if (combo == NULL) {
		g_warning("no fwheel_combo");
		return;
	}
	cd_find_fwheels(dialog, combo);
	gtk_list_select_item(GTK_LIST(GTK_COMBO(combo)->list) , 0);
}

/* a new filter wheel url is entered, try to open it */
void fwheel_select_cb( GtkWidget *widget, gpointer data )
{
	GtkCombo *combo;
	char *url;
	struct fwheel *fw;

	combo = gtk_object_get_data(GTK_OBJECT(data), "fwheel_combo");
	url = gtk_editable_get_chars(GTK_EDITABLE(combo->entry), 0, -1);
	fw = try_open_fwheel(url, data);
	g_free(url);
/* change the window label */
}
void fwheel_select2_cb( GtkList *list, GtkWidget *widget, gpointer data )
{
	fwheel_select_cb(widget, data);
}


void close_cam_dialog( GtkWidget *widget, gpointer data )
{
	struct ccd * ccd;
	guint timeout_id;
	d3_printf("destroying cam_dialog\n");
	ccd = gtk_object_get_data(GTK_OBJECT(widget), "open_camera");
	if (ccd != NULL) {
		ccd_close(ccd);
	}
	timeout_id = (guint)gtk_object_get_data(GTK_OBJECT(widget), "timeout");
	gtk_timeout_remove(timeout_id);
	gtk_object_set_data(GTK_OBJECT(data), "cam_dialog", NULL);
}

/* we just hide the camera dialog, so it retains the settings for later */
gint delete_event( GtkWidget *widget, GdkEvent  *event, gpointer data )
{
	gtk_widget_hide(widget);
	return TRUE;
}


/* convert a bin string like "1 1" or "1,1" or "1x2" to 
 * two ints; return 0 if successfull, -1 if couldn't parse
 */
static int parse_bin_string(char *text, int *bx, int *by)
{
	char *fe;
	*bx = strtol(text, &fe, 10);
	if (fe == text)
		return -1;
	while ((*fe != 0) && !isdigit(*fe))
		fe ++;
	if (*fe == 0)
		return -1;
	text = fe;
	*by = strtol(text, &fe, 10);
	if (fe == text)
		return -1;
	return 0;
}



/* read the exp settings from the img page into the cam structure
 * also do interaction-handling. If widget is non-null, only read 
 * data from that particular widget */
void set_exp_from_img_dialog(struct ccd *ccd, GtkWidget *dialog, void *widget)
{
	GtkCombo *combo;
	char *text;
	int bx, by, ret;

	combo = gtk_object_get_data(GTK_OBJECT(dialog), "img_bin_combo");
	g_return_if_fail(combo != NULL);
	if (widget == NULL || combo->list == widget || combo->entry == widget) {
		text = gtk_editable_get_chars(GTK_EDITABLE(combo->entry), 0, -1);
		ret = parse_bin_string(text, &bx, &by);
		g_free(text);
		if (!ret) {
			ccd->exp.bin_x = bx;
			ccd->exp.bin_y = by;
		}
	}
	if (widget == NULL) {
		ccd->exp.exp_time = named_spin_get_value(dialog, "img_exp_spin");
		ccd->exp.w = named_spin_get_value(dialog, "img_width_spin");
		ccd->exp.h = named_spin_get_value(dialog, "img_height_spin");
		ccd->exp.x_skip = named_spin_get_value(dialog, "img_x_skip_spin");
		ccd->exp.y_skip = named_spin_get_value(dialog, "img_y_skip_spin");
	} else {
		if (widget == gtk_object_get_data(GTK_OBJECT(dialog), "img_exp_spin"))
			ccd->exp.exp_time = 
				gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON(widget));
		if (widget == gtk_object_get_data(GTK_OBJECT(dialog), "img_width_spin"))
			ccd->exp.w = 
				gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(widget));
		if (widget == gtk_object_get_data(GTK_OBJECT(dialog), "img_height_spin"))
			ccd->exp.h = 
				gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(widget));
		if (widget == gtk_object_get_data(GTK_OBJECT(dialog), "img_x_skip_spin"))
			ccd->exp.x_skip = 
				gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(widget));
		if (widget == gtk_object_get_data(GTK_OBJECT(dialog), "img_y_skip_spin"))
			ccd->exp.y_skip = 
				gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(widget));
	}
	cam_to_img(dialog);
}

static void img_get_image_cb(GtkWidget *widget, gpointer data)
{
	struct ccd * ccd;
	struct ccd_frame *fr;

	ccd = gtk_object_get_data(GTK_OBJECT(data), "open_camera");
	if (ccd == NULL) {
		return;
	}
	set_exp_from_img_dialog(ccd, GTK_WIDGET(data), NULL);
	ccd_set_exposure(ccd, &ccd->exp);
	ccd_read_exposure(ccd, &ccd->exp);
	cam_to_img(GTK_WIDGET(data));
	fr = new_frame(ccd->exp.w, ccd->exp.h);
	if (fr == NULL) {
		err_printf("img_get_image_cb: cannot create %dx%d frame\n", 
			   ccd->exp.w, ccd->exp.h);
		return;
	}
	gtk_object_set_data_full(GTK_OBJECT(data), "current_frame", 
				 fr, (GtkDestroyNotify)(release_frame));
	ccd_start_exposure(ccd, get_named_checkb_val(GTK_WIDGET(data), "img_dark_checkb"));
}

/* called when the temp setpoint is changed */
static void cooler_temp_cb( GtkWidget *widget, gpointer data )
{
	struct ccd * ccd;
	float temp_set;

	ccd = gtk_object_get_data(GTK_OBJECT(data), "open_camera");
	if (ccd == NULL) {
		return;
	}
	temp_set = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(widget));
	ccd_set_temperature(ccd, temp_set);
}


/* called when something on the img page is changed */
static void img_changed_cb( GtkWidget *widget, gpointer data )
{
	struct ccd * ccd;
	ccd = gtk_object_get_data(GTK_OBJECT(data), "open_camera");
	if (ccd == NULL) {
		return;
	}
	set_exp_from_img_dialog(ccd, GTK_WIDGET(data), widget);
}

static void img_changed2_cb( GtkList *list, GtkWidget *widget, gpointer data )
{
	img_changed_cb(GTK_WIDGET(list), data);
}

/* update obs fields in the dialog */
static void update_obs_entries( GtkWidget *dialog, struct obs_data *obs)
{
	char buf[256];
	double ha, airm;

	if (obs->objname != NULL)
		named_entry_set(dialog, "obs_object_entry", obs->objname);
	snprintf(buf, 256, "%.0f", obs->equinox);
	named_entry_set(dialog, "obs_epoch_entry", buf);
	degrees_to_dms(buf, obs->ra / 15.0);
	named_entry_set(dialog, "obs_ra_entry", buf);
	degrees_to_dms_pr(buf, obs->dec, 1);
	named_entry_set(dialog, "obs_dec_entry", buf);

	if (obs->filter != NULL)
		named_entry_set(dialog, "obs_filter_combo_entry", obs->filter);
	ha = obs_current_hour_angle(obs);
	airm = obs_current_airmass(obs);
	snprintf(buf, 255, "HA: %.3f, Airmass: %.2f", ha, airm);
	named_label_set(dialog, "obj_comment_label", buf);

}


/* if we are in auto filename mode, generate a new name based on the 
 * current obs. Reset the sequence number if the name has changed */
static void auto_filename(GtkWidget *dialog)
{
	struct obs_data *obs;
	char *text;
	char name[256];
	int seq = 1;

	if (!get_named_checkb_val(dialog, "file_auto_name_checkb"))
		return;

	text = named_entry_text(dialog, "file_entry");
	if (get_named_checkb_val(dialog, "img_dark_checkb")) {
		if (text == NULL || strcmp(text, DEFAULT_DARK_NAME)) {
			named_entry_set(dialog, "file_entry", DEFAULT_DARK_NAME);
			check_seq_number(DEFAULT_DARK_NAME, &seq);
			named_spin_set(dialog, "file_seqn_spin", seq);
		}
		g_free(text);
		return;
	}
	obs = gtk_object_get_data(GTK_OBJECT(dialog), "obs_data");
	if (obs == NULL) {
		d3_printf("no obs to set the filename from\n");
		g_free(text);
		return;
	}
	if (obs->filter != NULL)
		snprintf(name, 255, "%s-%s-", obs->objname, obs->filter);
	else
		snprintf(name, 255, "%s-", obs->objname);
	if (text == NULL || strcmp(text, name)) {
		named_entry_set(dialog, "file_entry", name);
		check_seq_number(name, &seq);
		named_spin_set(dialog, "file_seqn_spin", seq);
	}
	g_free(text);
}

/* called when the object or filter on the obs page is changed */
static void obsdata_cb( GtkWidget *widget, gpointer data )
{
	struct obs_data *obs;
	int ret;
	char *text, *end;
	char buf[128];
	GtkWidget *wid;
	double d;

	obs = gtk_object_get_data(GTK_OBJECT(data), "obs_data");
	if (obs == NULL) { 
		obs = obs_data_new();
		if (obs == NULL) {
			err_printf("cannot create new obs\n");
			return;
		}
		gtk_object_set_data_full(GTK_OBJECT(data), "obs_data", obs, 
					 (GtkDestroyNotify)(obs_data_release));
		text = named_entry_text(GTK_WIDGET(data), "obs_filter_combo_entry");
		replace_strval(&obs->filter, text);
	}
	wid = gtk_object_get_data(GTK_OBJECT(data), "obs_object_entry");
	if (widget == wid) {
		text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
		d3_printf("looking up %s\n", text);
		ret = obs_set_from_object(obs, text);
		if (ret < 0) {
			snprintf(buf, 128, "Cannot find object %s", text);
			status_message(data, buf, "obs", 3000);
			replace_strval(&obs->objname, text);
			auto_filename(data);
			return;
		}
		g_free(text);
		update_obs_entries(data, obs);
		auto_filename(data);
		return;
	}
	wid = gtk_object_get_data(GTK_OBJECT(data), "obs_filter_combo_entry");
	if (widget == wid) {
		text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
		d3_printf("obs_cb: setting filter to %s\n", text);
		replace_strval(&obs->filter, text);
		update_obs_entries(data, obs);
		auto_filename(data);
		return;
	}
	wid = gtk_object_get_data(GTK_OBJECT(data), "obs_ra_entry");
	if (widget == wid) {
		text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
		if (!dms_to_degrees(text, &d)) {
			obs->ra = d * 15.0;
			update_obs_entries(data, obs);
		} else {
			error_beep();
			status_message(data, "Bad R.A. Value", "obs", 3000);
		}
		g_free(text);
		return;
	}
	wid = gtk_object_get_data(GTK_OBJECT(data), "obs_dec_entry");
	if (widget == wid) {
		text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
		if (!dms_to_degrees(text, &d)) {
			obs->dec = d;
			update_obs_entries(data, obs);
		} else {
			error_beep();
			status_message(data, "Bad Declination Value", "obs", 3000);
		}
		g_free(text);
		return;
	}
	wid = gtk_object_get_data(GTK_OBJECT(data), "obs_epoch_entry");
	if (widget == wid) {
		text = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
		d = strtod(text, &end);
		if (text != end) {
			obs->equinox = d;
			update_obs_entries(data, obs);
		} else {
			error_beep();
			status_message(data, "Bad Equinox Value", "obs", 3000);
		}
		g_free(text);
		return;
	}
}

static gboolean obsdata_focus_out_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	obsdata_cb(GTK_WIDGET(widget), data);
	return FALSE;
}


/* external interface for setting the obs object */
/* returns 0 if the object was found */
int set_obs_object(GtkWidget *dialog, char *objname)
{
	GtkWidget *wid;
	wid = gtk_object_get_data(GTK_OBJECT(dialog), "obs_object_entry");
	g_return_val_if_fail(wid != NULL, -1);
	named_entry_set(dialog, "obs_object_entry", objname);
	obsdata_cb(wid, dialog);
	return 0;
}


/* called when the filter list selection on the obs page is changed */
static void obs_filter_list_cb(GtkList *list, GtkWidget *widget, gpointer data )
{
	struct obs_data *obs;
	char *text;
	GtkWidget *wid;

	obs = gtk_object_get_data(GTK_OBJECT(data), "obs_data");
	if (obs == NULL)
		return;
	wid = gtk_object_get_data(GTK_OBJECT(data), "obs_filter_combo_entry");
	text = gtk_editable_get_chars(GTK_EDITABLE(wid), 0, -1);
//	d3_printf("obs_filter_list_cb: got %s was: %s\n", text, obs->filter);
	if (obs->filter == NULL || strcmp(obs->filter, text)) {
//		d3_printf("obs_filter_list_cb: replacing filter name\n");
		replace_strval(&obs->filter, text);
		auto_filename(data);
//		update_obs_entries(data, obs);
	} else {
		free(text);
	}
}

/* set the image window to a ratio of the full sensor area */
static void reset_img_window_cb( GtkWidget *widget, gpointer data )
{
	struct ccd * ccd;
	ccd = gtk_object_get_data(GTK_OBJECT(data), "open_camera");
	if (ccd == NULL) {
		return;
	}

	ccd->exp.w = ccd->info.active_pixels / ccd->exp.bin_x;
	ccd->exp.h = ccd->info.active_lines / ccd->exp.bin_y;
	if (widget == gtk_object_get_data(GTK_OBJECT(data), "img_halfsz")) {
		ccd->exp.w = ccd->exp.w * 1000 / 1414;
		ccd->exp.h = ccd->exp.h * 1000 / 1414;
	}
	if (widget == gtk_object_get_data(GTK_OBJECT(data), "img_quartersz")) {
		ccd->exp.w = ccd->exp.w * 1000 / 2000;
		ccd->exp.h = ccd->exp.h * 1000 / 2000;
	}
	if (widget == gtk_object_get_data(GTK_OBJECT(data), "img_eighthsz")) {
		ccd->exp.w = ccd->exp.w * 1000 / 2 / 1414;
		ccd->exp.h = ccd->exp.h * 1000 / 2 / 1414;
	}

	ccd->exp.x_skip = (ccd->info.active_pixels - 
			   ccd->exp.bin_x * ccd->exp.w) / 2;
	clamp_int(&ccd->exp.x_skip, 0, ccd->info.active_pixels);
	ccd->exp.y_skip = (ccd->info.active_lines - 
			   ccd->exp.bin_y * ccd->exp.h) / 2;
	clamp_int(&ccd->exp.y_skip, 0, ccd->info.active_lines);
	cam_to_img(data);
}

static void img_multiple_cb( GtkWidget *widget, gpointer data )
{
	char buf[64];
	int nf;
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ) {
		gtk_label_set_text(GTK_LABEL(GTK_BIN(widget)->child), "Multiple");
		return;
	}
	gtk_label_set_text(GTK_LABEL(GTK_BIN(widget)->child), "Stop");
	nf = named_spin_get_value(data, "img_number_spin");
	snprintf(buf, 63, "%d", nf);
	named_entry_set(data, "current_frame_entry", buf);
	img_get_image_cb(widget, data);
}

static void img_focus_cb( GtkWidget *widget, gpointer data )
{
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ) {
		gtk_label_set_text(GTK_LABEL(GTK_BIN(widget)->child), "Focus");
		return;
	}
	gtk_label_set_text(GTK_LABEL(GTK_BIN(widget)->child), "Stop");
	img_get_image_cb(widget, data);
}

/* send an abort slew message */
static void scope_abort_cb( GtkWidget *widget, gpointer data )
{
	lx200_abort_slew();
}


/* slew to the current obs coordinates */
static void scope_goto_cb( GtkWidget *widget, gpointer data )
{
	struct obs_data *obs;
	int ret;
	char msg[512];
	obs = gtk_object_get_data(GTK_OBJECT(data), "obs_data");
	if (obs == NULL)
		return;

	ret = obs_check_limits(obs, data);
	if (ret) {
		snprintf(msg, 511, "Object %s (ha=%.2f dec=%.2f)\n"
			 "is outside slew limits\n"
			 "%s\nStart slew?", obs->objname, 
			 obs_current_hour_angle(obs), obs->dec, last_err());
		if (modal_yes_no(msg, NULL) != 1)
			return;
	}

	lx200_set_object(obs->ra, obs->dec, obs->equinox, obs->objname);
	lx200_start_slew();
}

/* sync to the current obs coordinates */
static void scope_sync_cb( GtkWidget *widget, gpointer data )
{
	struct obs_data *obs;
	obs = gtk_object_get_data(GTK_OBJECT(data), "obs_data");
	if (obs == NULL) {
		error_beep();
		status_message(data, "Set obs first", "obs", 3000);
		return;
	}
	lx200_sync_coords(obs->ra, obs->dec, obs->equinox);
	status_message(data, "Synchronised", "obs", 3000);
}

/* align to the current obs coordinates */
static void scope_align_cb( GtkWidget *widget, gpointer data )
{
	struct obs_data *obs;
	obs = gtk_object_get_data(GTK_OBJECT(data), "obs_data");
	if (obs == NULL) {
		error_beep();
		status_message(data, "Set obs first", "obs", 3000);
		return;
	}
	lx200_align_coords(obs->ra, obs->dec, obs->equinox);
	status_message(data, "Aligned", "obs", 3000);
}


/* external interface for slew_cb */
void goto_dialog_obs(GtkWidget *dialog)
{
	scope_goto_cb(NULL, dialog);
}


/* correct the scope's pointing with the difference between 
 * the current image window wcs (if fitted) and the telescope
 * position */
static void scope_auto_cb( GtkWidget *widget, gpointer data )
{
	struct wcs *wcs;
	GtkWidget *imw;
	double ra, dec, epoch;
	struct obs_data *obs;

	obs = gtk_object_get_data(GTK_OBJECT(data), "obs_data");
	if (obs == NULL)
		return;
	imw = gtk_object_get_data(GTK_OBJECT(data), "image_window");
	g_return_if_fail(imw != NULL);
	wcs = gtk_object_get_data(GTK_OBJECT(imw), "wcs_of_window");
	g_return_if_fail(wcs != NULL);
	g_return_if_fail(wcs->wcsset != WCS_INVALID);

	if (P_INT(TELE_USE_CENTERING)) {
		if ((P_DBL(TELE_GEAR_PLAY) > 0.001) && (obs->ra > wcs->xref)) {
			lx200_centering_move(obs->ra - wcs->xref + P_DBL(TELE_GEAR_PLAY)
					     , obs->dec - wcs->yref + P_DBL(TELE_GEAR_PLAY));
			lx200_centering_move(-P_DBL(TELE_GEAR_PLAY), -P_DBL(TELE_GEAR_PLAY));
		} else {
			lx200_centering_move(obs->ra - wcs->xref, 
					     obs->dec - wcs->yref);
		}
	} else {
		if (lx200_get_target(&ra, &dec, &epoch))
			return;
		ra = ra - wcs->xref + obs->ra;
		dec = dec - wcs->yref + obs->dec;
		
		lx200_set_object(ra, dec, epoch, obs->objname);
		lx200_start_slew();
	}
}

/* external interface for auto_cb */
void center_matched_field(GtkWidget *dialog)
{
	scope_auto_cb(NULL, dialog);
}


void cam_set_callbacks(GtkWidget *dialog)
{
	GtkWidget *combo, *centry;

	gtk_signal_connect (GTK_OBJECT (dialog), "delete_event",
                        GTK_SIGNAL_FUNC (delete_event), dialog);
	combo = gtk_object_get_data(GTK_OBJECT(dialog), "cameras_combo");
	if (combo == NULL) {
		g_warning("no cameras_combo");
		return;
	}
	gtk_combo_disable_activate(GTK_COMBO(combo));
	centry = GTK_COMBO(combo)->entry;
	gtk_signal_connect (GTK_OBJECT (centry), "activate",
			    GTK_SIGNAL_FUNC (camera_select_cb), dialog);
	gtk_signal_connect (GTK_OBJECT (GTK_COMBO(combo)->list), "select_child",
			    GTK_SIGNAL_FUNC (camera_select2_cb), dialog);

	combo = gtk_object_get_data(GTK_OBJECT(dialog), "fwheel_combo");
	if (combo == NULL) {
		g_warning("no fwheel_combo");
		return;
	}
	gtk_combo_disable_activate(GTK_COMBO(combo));
	centry = GTK_COMBO(combo)->entry;
	gtk_signal_connect (GTK_OBJECT (centry), "activate",
			    GTK_SIGNAL_FUNC (fwheel_select_cb), dialog);
	gtk_signal_connect (GTK_OBJECT (GTK_COMBO(combo)->list), "select_child",
			    GTK_SIGNAL_FUNC (fwheel_select2_cb), dialog);

	set_named_callback(dialog, "scope_goto_button", "clicked", scope_goto_cb);
	set_named_callback(dialog, "scope_auto_button", "clicked", scope_auto_cb);
	set_named_callback(dialog, "scope_sync_button", "clicked", scope_sync_cb);
	set_named_callback(dialog, "scope_align_button", "clicked", scope_align_cb);
	set_named_callback(dialog, "scope_abort_button", "clicked", scope_abort_cb);
	set_named_callback(dialog, "scope_dither_button", "clicked", scope_dither_cb);
	set_named_callback(dialog, "obs_list_abort_button", "clicked", scope_abort_cb);
	set_named_callback(dialog, "obs_list_file_button", "clicked", obs_list_select_file_cb);
	set_named_callback(dialog, "img_get_img_button", "clicked", img_get_image_cb);
	set_named_callback(dialog, "img_fullsz", "clicked", reset_img_window_cb);
	set_named_callback(dialog, "img_halfsz", "clicked", reset_img_window_cb);
	set_named_callback(dialog, "img_quartersz", "clicked", reset_img_window_cb);
	set_named_callback(dialog, "img_eighthsz", "clicked", reset_img_window_cb);
	set_named_callback(dialog, "img_focus_button", "toggled", img_focus_cb);
	set_named_callback(dialog, "img_get_multiple_button", "toggled", img_multiple_cb);
	set_named_callback(dialog, "img_width_spin", "changed", img_changed_cb);
	set_named_callback(dialog, "img_height_spin", "changed", img_changed_cb);
	set_named_callback(dialog, "img_exp_spin", "changed", img_changed_cb);
	set_named_callback(dialog, "img_bin_combo_entry", "activate", img_changed_cb);
	combo = gtk_object_get_data(GTK_OBJECT(dialog), "img_bin_combo");
	gtk_combo_disable_activate(GTK_COMBO(combo));
	gtk_signal_connect (GTK_OBJECT (GTK_COMBO(combo)->list), "select_child",
			    GTK_SIGNAL_FUNC (img_changed2_cb), dialog);
	combo = gtk_object_get_data(GTK_OBJECT(dialog), "obs_filter_combo");
	gtk_signal_connect (GTK_OBJECT (GTK_COMBO(combo)->list), "select_child",
			    GTK_SIGNAL_FUNC (filter_list_select_cb), dialog);
	set_named_callback(dialog, "obs_object_entry", "activate", obsdata_cb);
	set_named_callback(dialog, "obs_object_entry", "focus-out-event", 
			   obsdata_focus_out_cb);
	set_named_callback(dialog, "obs_ra_entry", "activate", obsdata_cb);
	set_named_callback(dialog, "obs_ra_entry", "focus-out-event", 
			   obsdata_focus_out_cb);
	set_named_callback(dialog, "obs_dec_entry", "activate", obsdata_cb);
	set_named_callback(dialog, "obs_dec_entry", "focus-out-event", 
			   obsdata_focus_out_cb);
	set_named_callback(dialog, "obs_epoch_entry", "activate", obsdata_cb);
	set_named_callback(dialog, "obs_epoch_entry", "focus-out-event", 
			   obsdata_focus_out_cb);
	set_named_callback(dialog, "obs_list_fname", "activate", obs_list_fname_cb);
	set_named_callback(dialog, "obs_filter_combo_entry", "activate", obsdata_cb);
	set_named_callback(dialog, "obs_filter_combo_entry", "focus-out-event", 
			   obsdata_focus_out_cb);
	set_named_callback(dialog, "cooler_tempset_spin", "changed", cooler_temp_cb);
	combo = gtk_object_get_data(GTK_OBJECT(dialog), "obs_filter_combo");
	g_return_if_fail(combo != NULL);
	gtk_signal_connect (GTK_OBJECT (GTK_COMBO(combo)->list), "select_child",
			    GTK_SIGNAL_FUNC (obs_filter_list_cb), dialog);
	obs_list_callbacks(dialog);
}

/* initialise the telescope page params from pars */
static void set_scope_params_from_par(gpointer dialog)
{
	named_spin_set(dialog, "e_limit_spin", P_DBL(TELE_E_LIMIT));
	set_named_checkb_val(dialog, "e_limit_checkb", P_INT(TELE_E_LIMIT_EN));
	named_spin_set(dialog, "w_limit_spin", P_DBL(TELE_W_LIMIT));
	set_named_checkb_val(dialog, "w_limit_checkb", P_INT(TELE_W_LIMIT_EN));
	named_spin_set(dialog, "n_limit_spin", P_DBL(TELE_N_LIMIT));
	set_named_checkb_val(dialog, "n_limit_checkb", P_INT(TELE_N_LIMIT_EN));
	named_spin_set(dialog, "s_limit_spin", P_DBL(TELE_S_LIMIT));
	set_named_checkb_val(dialog, "s_limit_checkb", P_INT(TELE_S_LIMIT_EN));
	named_entry_set(dialog, "tele_port_entry", P_STR(FILE_SCOPE_SERIAL));
}

void camera_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *window = data;
	GtkWidget *dialog;
	GtkWidget* create_camera_control (void);
	guint timeout_id;

	dialog = gtk_object_get_data(GTK_OBJECT(window), "cam_dialog");
	if (dialog == NULL) {
		dialog = create_camera_control();
		gtk_widget_ref(dialog);
		gtk_object_set_data_full(GTK_OBJECT(window), "cam_dialog", dialog,
					 (GtkDestroyNotify)gtk_widget_destroy);
		gtk_object_set_data(GTK_OBJECT(dialog), "image_window", window); 
		gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
				    GTK_SIGNAL_FUNC (close_cam_dialog), window);
//		set_filter_list(dialog);
		cam_set_callbacks(dialog);
		set_scope_params_from_par(dialog);
		cd_default_ccd(dialog);
		cd_default_fwheel(dialog);
		cam_to_img(dialog);
//		cam_to_focus(dialog);
		cam_to_cooler(dialog);
		timeout_id = gtk_timeout_add(400, (GtkFunction)camera_sm, dialog);
		gtk_object_set_data(GTK_OBJECT(dialog), "timeout", (gpointer)(timeout_id));
		gtk_widget_show(dialog);
	} else {
		gtk_widget_show(dialog);
		gdk_window_raise(dialog->window);
	}
//	cam_dialog_update(dialog);
//	cam_dialog_edit(dialog);
}

