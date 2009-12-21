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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <libgen.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "camera.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "params.h"

#include "obslist.h"
#include "filegui.h"
#include "misc.h"
#include "cameragui.h"
#include "interface.h"
#include "fwheel.h"
#include "multiband.h"

#define STOPPED_WINDOW_RUNNING 0
#define STOPPED_WINDOW_ERROR 1
#define STOPPED_WINDOW_DONE 2

/* TODO: errors correvted for dec */


/* obs command state machine states */
typedef enum {
	OBS_START,
	OBS_DO_COMMAND,
	OBS_NEXT_COMMAND,
	OBS_WAIT_SLEW_END,
	OBS_WAIT_SLEW_END2,
	OBS_CMD_ERROR,
	OBS_WAIT_SLEW_END_SYNC_GET,
	OBS_SKIP_OBJECT,
	OBS_STABILISATION_DELAY,
	OBS_STABILISATION_DELAY_SYNC_GET,
	OBS_STABILISATION_DELAY2,
	OBS_STABILISATION_DELAY_SYNC_GET2,

} ObsState;

struct command {
	char *name;
	int(* do_command)(char *args, GtkWidget *dialog);
};

void show_stopped_window(gpointer window, int action);

static int obs_list_load_file(GtkWidget *dialog, char *name);
static void obs_list_select_cb (GtkList *list, GtkWidget *widget, gpointer user_data);
//static void obs_step_cb(GtkWidget *widget, gpointer data);

static int do_get_cmd (char *args, GtkWidget *dialog);
static int do_dark_cmd (char *args, GtkWidget *dialog);
static int do_goto_cmd (char *args, GtkWidget *dialog);
static int do_match_cmd (char *args, GtkWidget *dialog);
static int do_ckpoint_cmd (char *args, GtkWidget *dialog);
static int do_phot_cmd (char *args, GtkWidget *dialog);
static int do_save_cmd (char *args, GtkWidget *dialog);
static int do_mphot_cmd (char *args, GtkWidget *dialog);
static int do_qmatch_cmd (char *args, GtkWidget *dialog);
static int do_mget_cmd (char *args, GtkWidget *dialog);
static int do_mphot_cmd (char *args, GtkWidget *dialog);
static int do_filter_cmd (char *args, GtkWidget *dialog);
static int do_exp_cmd (char *args, GtkWidget *dialog);
static void status_message(GtkWidget *window, char *message, char *context, int time);
static void status_pop(GtkWidget *window, char *context);

static struct command cmd_table[] = {
	{"get", do_get_cmd}, /* get and display an image without saving */
	{"dark", do_dark_cmd}, /* get and display a dark frame without saving */
	{"goto", do_goto_cmd}, /* goto <obj_name> [<recipy>] set
				* obs and slew telescope to coordinates; load
				* recipy file if present */
	{"match", do_match_cmd}, /* match wcs and repoint telescope to center */
	{"phot", do_phot_cmd}, /* run photometry on the frame */
	{"qmatch", do_qmatch_cmd}, /* quietly match image wcs (don't move telescope) */
	{"ckpoint", do_ckpoint_cmd}, /* if pointing error > max_pointing_error
				      * center telescope, sync it and get another frame */
	{"save", do_save_cmd}, /* save last acquired frame (using auto name) */
	{"mget", do_mget_cmd}, /* mget [<frames>] get and save <frames> frames */
	{"mphot", do_mphot_cmd}, /* mphot [<frames>] get, save and phot <frames> frames */
	{"filter", do_filter_cmd}, /* filter <name> select new filter */
	{"exp", do_exp_cmd}, /* set exposure */
//	{"mdark", do_mdark_cmd}, /* mdark [<frames>] get and save <frames> dark frames */
	{NULL, NULL}
};


/* parse a command header, and update the cmd and arg pointers with 
 * the command name and args, respectively. If args or name cannot be 
 * found, the pointers are set to null. return the length of the
 * command name, or a negative error code.
 */
static int cmd_head(char *cmdline, char **cmd, char **arg)
{
	int cl;

	while (isspace(*cmdline))
		cmdline ++;
	if (*cmdline == 0) {
		*cmd = NULL;
		if (arg)
			*arg = NULL;
		return 0;
	}
	*cmd = cmdline;
	while (isalnum(*cmdline))
		cmdline ++;
	cl = cmdline - *cmd;
	if (*cmdline == 0) {
		if (arg)
			*arg = NULL;
		return cl;
	}
	while (isspace(*cmdline))
		cmdline ++;
	if (*cmdline == 0) {
		if (arg)
			*arg = NULL;
		return cl;
	}
	if (arg)
		*arg = cmdline;
	return cl;
}

/* return the command code, or -1 if it couldn't be found
 */
static int cmd_lookup(char *cmd, int len)
{
	int i = 0;
	while (cmd_table[i].name != NULL) {
		if (name_matches(cmd_table[i].name, cmd, len)) {
			if (cmd_table[i].do_command == NULL)
				return -1;
			return i;
		}
		i++;
	}
	return -1;
}

static int do_command(char *cmdline, GtkWidget *dialog)
{
	int cl;
	char *cmd, *arg;
	int command;

	if (cmdline == NULL)
		return OBS_NEXT_COMMAND;

	cl = cmd_head(cmdline, &cmd, &arg);

	if (cl == 0) { /* skip empty lines */
		return OBS_NEXT_COMMAND;
	}
	command = cmd_lookup(cmd, cl);
	if (command < 0)
		return OBS_CMD_ERROR;
	return (* cmd_table[command].do_command)(arg, dialog);
}


/* return a copy of the i-th command string (the text in the label in the index'th
 * list child */
static char *get_cmd_line(GtkList *list, int index)
{
	GList *ilist = NULL, *cmd_el;
	char *cmd = NULL;

	ilist = gtk_container_children(GTK_CONTAINER(list));
	cmd_el = g_list_nth(ilist, index);
	if (cmd_el != NULL) {
		gtk_label_get(GTK_LABEL(GTK_BIN(cmd_el->data)->child), &cmd);
		cmd = strdup(cmd);
	}
	g_list_free(ilist);
	return cmd;
}

/* actual obs commands - which are really parts of the state machine 
 * they all get called from the OBS_DO_COMMAND state, and return the state
 * the machine should jump to. The commands generally manipulate the cam_control
 * widgets */

static int do_get_cmd (char *args, GtkWidget *dialog)
{
	struct ccd *ccd;
	struct ccd_frame *fr;
	int ret;

	set_named_checkb_val(dialog, "img_get_multiple_button", 0);
	set_named_checkb_val(dialog, "img_dark_checkb", 0);
	ccd = gtk_object_get_data(GTK_OBJECT(dialog), "open_camera");
	if (ccd != NULL) {
		set_exp_from_img_dialog(ccd, GTK_WIDGET(dialog), NULL);
		ccd_set_exposure(ccd, &ccd->exp);
		ccd_read_exposure(ccd, &ccd->exp);
		cam_to_img(GTK_WIDGET(dialog));
		fr = new_frame(ccd->exp.w, ccd->exp.h);
		if (fr == NULL) {
			err_printf("Cannot create %dx%d frame\n", 
				   ccd->exp.w, ccd->exp.h);
			return OBS_CMD_ERROR;
		}
		gtk_object_set_data_full(GTK_OBJECT(dialog), "current_frame", 
					 fr, (GtkDestroyNotify)(release_frame));
		ret = ccd_start_exposure (ccd, 0);
	} else {
		ret = -1;
		err_printf("No CCD\n");
	}
	if (ret < 0)
		return OBS_CMD_ERROR;
	else
		return OBS_NEXT_COMMAND;
}

static int do_filter_cmd (char *args, GtkWidget *dialog)
{
	struct fwheel *fw;
	GtkWidget *combo;
	int i;
	char **filters;

	fw = gtk_object_get_data(GTK_OBJECT(dialog), "open_fwheel");
	if (fw == NULL) {
		err_printf("Filter wheel error or no wheel connected\n");
		return OBS_CMD_ERROR;
	}
	i = 0;
	while (args[i]) {
		if (isspace(args[i])) {
			args[i] = 0;
			break;
		}
		i++;
	} 
	filters = fwheel_get_filter_names(fw);
	i = 0;
	while (*filters != NULL) {
		if (!strcasecmp(*filters, args)) {
			combo = gtk_object_get_data(GTK_OBJECT(dialog), "obs_filter_combo");
			gtk_list_select_item(GTK_LIST(GTK_COMBO(combo) -> list), i);
			return OBS_NEXT_COMMAND;
		}
		filters ++;
		i++;
	}
	err_printf("Bad filter name: %s\n", args);
	return OBS_CMD_ERROR;
}

static int do_exp_cmd (char *args, GtkWidget *dialog)
{
	double nexp;
	char *endp;

	nexp = strtod(args, &endp);
	if (args == endp) {
		err_printf("Bad exposure value: %s\n", args);
		return OBS_CMD_ERROR;
	}

	named_spin_set(dialog, "img_exp_spin", nexp);
	return OBS_NEXT_COMMAND;
}

static int do_dark_cmd (char *args, GtkWidget *dialog)
{
	struct ccd *ccd;
	int ret;

	set_named_checkb_val(dialog, "img_get_multiple_button", 0);
	set_named_checkb_val(dialog, "img_dark_checkb", 1);
	ccd = gtk_object_get_data(GTK_OBJECT(dialog), "open_camera");
	if (ccd != NULL) {
		ret = ccd_start_exposure (ccd, 1);
	} else {
		ret = -1;
		err_printf("No CCD\n");
	}
	if (ret < 0)
		return OBS_CMD_ERROR;
	else
		return OBS_NEXT_COMMAND;
}

/* check that the object in obs in within the limits set in the dialog
 * return 0 if it is, do an err_printf and retun -1 if it isn't */
int obs_check_limits(struct obs_data *obs, gpointer dialog)
{
	double lim, ha;
	ha = obs_current_hour_angle(obs);

	if (get_named_checkb_val(GTK_WIDGET(dialog), "e_limit_checkb")) {
		lim = named_spin_get_value(dialog, "e_limit_spin");
		if (ha < lim) {
			err_printf("E limit reached (%.1f < %.1f)\n", ha, lim);
			return -1;
		}
	}
	if (get_named_checkb_val(GTK_WIDGET(dialog), "w_limit_checkb")) {
		lim = named_spin_get_value(dialog, "w_limit_spin");
		if (ha > lim) {
			err_printf("W limit reached (%.1f > %.1f)\n", ha, lim);
			return -1;
		}
	}
	if (get_named_checkb_val(GTK_WIDGET(dialog), "n_limit_checkb")) {
		lim = named_spin_get_value(dialog, "n_limit_spin");
		if (obs->dec > lim) {
			err_printf("N limit reached (%.1f > %.1f)\n", obs->dec, lim);
			return -1;
		}
	}
	if (get_named_checkb_val(GTK_WIDGET(dialog), "s_limit_checkb")) {
		lim = named_spin_get_value(dialog, "s_limit_spin");
		if (obs->dec < lim) {
			err_printf("S limit reached (%.1f < %.1f)\n", obs->dec, lim);
			return -1;
		}
	}
	return 0;
}

static int do_goto_cmd (char *args, GtkWidget *dialog)
{
	int ret;
	char *text, *start, *end, *start2, *end2;
	int token;
	gpointer window;
	struct obs_data *obs;

	text = args;
	next_token(NULL, NULL, NULL);
	token = next_token(&text, &start, &end);
	if (token != TOK_WORD && token != TOK_STRING) {
		err_printf("No object\n");
		return OBS_CMD_ERROR;
	}
	token = next_token(&text, &start2, &end2);
	*(end) = 0;

	obs = obs_data_new();
	if (obs == NULL) {
		err_printf("Cannot create obs\n");
		return OBS_CMD_ERROR;
	}

	ret = obs_set_from_object(obs, start);
	if (ret < 0) {
		err_printf("Cannot find object\n");
		obs_data_release(obs);
		return OBS_CMD_ERROR;
	}

	ret = obs_check_limits(obs, dialog);
	obs_data_release(obs);
	if (ret)
		return OBS_SKIP_OBJECT;

	set_obs_object(dialog, start);
	if (token == TOK_WORD || token == TOK_STRING) {
		*(end2) = 0;
		window =  gtk_object_get_data(GTK_OBJECT(dialog), "image_window");
		if (window == NULL) {
			err_printf("no image window\n");
			return OBS_CMD_ERROR;
		}
		ret = load_rcp_to_window(window, start2);
		if (ret < 0) {
			err_printf("error loading rcp file\n");
			return OBS_CMD_ERROR;
		}
	}
	goto_dialog_obs(dialog);
	return OBS_WAIT_SLEW_END;
}

static int do_match_cmd (char *args, GtkWidget *dialog)
{
	void *imw;
	int ret;
	imw = gtk_object_get_data(GTK_OBJECT(dialog), "image_window");
	g_return_val_if_fail(imw != NULL, OBS_CMD_ERROR);
	ret = match_field_in_window(imw);
	if (ret < 0) {
		d3_printf("Cannot match\n");
		return OBS_CMD_ERROR;
	}
	center_matched_field(dialog);
	if (P_INT(TELE_USE_CENTERING)) 
		return OBS_STABILISATION_DELAY;
	else
		return OBS_WAIT_SLEW_END;
}

static int do_mget_cmd (char *args, GtkWidget *dialog)
{
	struct ccd *ccd;
	struct ccd_frame *fr;
	int ret;
	char *text, *start, *end;
	int token, n;

	text = args;
	next_token(NULL, NULL, NULL);
	token = next_token(&text, &start, &end);
	if (token == TOK_NUMBER) {
		n = strtol(start, NULL, 10);
		d3_printf("do_mget_cmd: setting frame count to %d\n", n);
		named_spin_set(dialog, "img_number_spin", 1.0 * n);
	}
	set_named_checkb_val(dialog, "img_get_multiple_button", 1);
	set_named_checkb_val(dialog, "img_dark_checkb", 0);
	ccd = gtk_object_get_data(GTK_OBJECT(dialog), "open_camera");
	if (ccd != NULL) {
		set_exp_from_img_dialog(ccd, GTK_WIDGET(dialog), NULL);
		ccd_set_exposure(ccd, &ccd->exp);
		ccd_read_exposure(ccd, &ccd->exp);
		cam_to_img(GTK_WIDGET(dialog));
		fr = new_frame(ccd->exp.w, ccd->exp.h);
		if (fr == NULL) {
			err_printf("do_get_cmd: cannot create %dx%d frame\n", 
				   ccd->exp.w, ccd->exp.h);
			return OBS_CMD_ERROR;
		}
		gtk_object_set_data_full(GTK_OBJECT(dialog), "current_frame", 
					 fr, (GtkDestroyNotify)(release_frame));
		ret = ccd_start_exposure (ccd, 0);
	} else {
		ret = -1;
		err_printf("No CCD\n");
	}
	if (ret < 0)
		return OBS_CMD_ERROR;
	else
		return OBS_NEXT_COMMAND;
}

/* 1/cos(dec), clamped at 5 */
static double dec_factor(double dec)
{
	if (cos(dec) > 0.2)
		return (1.0 / cos(dec));
	else
		return 5.0;

}

static int do_ckpoint_cmd (char *args, GtkWidget *dialog)
{
	int ret;
	void *imw;
	struct wcs *wcs;
	struct obs_data *obs;
	double cerr;


	imw = gtk_object_get_data(GTK_OBJECT(dialog), "image_window");
	g_return_val_if_fail(imw != NULL, OBS_CMD_ERROR);
	ret = match_field_in_window_quiet(imw);
	if (ret < 0) {
		err_printf("Cannot match\n");
		return OBS_CMD_ERROR;
	}

	obs = gtk_object_get_data(GTK_OBJECT(dialog), "obs_data");
	if (obs == NULL) {
		err_printf("No obs data for centering\n");
		return OBS_CMD_ERROR;
	}
	wcs = gtk_object_get_data(GTK_OBJECT(imw), "wcs_of_window");
	if (wcs == NULL || wcs->wcsset == WCS_INVALID) {
		err_printf("No wcs for centering\n");
		return OBS_CMD_ERROR;
	}
	cerr = sqrt(sqr(wcs->xref - obs->ra) / dec_factor (obs->dec)
		    + sqr(wcs->yref - obs->dec));
	d3_printf("centering error is %.3f\n", cerr);
	if (cerr > P_DBL(MAX_POINTING_ERR)) {
		center_matched_field(dialog);
		if (P_INT(TELE_USE_CENTERING)) 
			return OBS_STABILISATION_DELAY_SYNC_GET;
		else
			return OBS_WAIT_SLEW_END_SYNC_GET;
	}
	return OBS_NEXT_COMMAND;
}

static int do_phot_cmd (char *args, GtkWidget *dialog)
{
	void *imw;
	char *srep;
	FILE *fp;
	int ret;

	imw = gtk_object_get_data(GTK_OBJECT(dialog), "image_window");
	if (imw == NULL) {
		err_printf("No image window\n");
		return OBS_CMD_ERROR;
	}
	ret = match_field_in_window_quiet(imw);
	if (ret < 0) {
		err_printf("Cannot match\n");
		return OBS_CMD_ERROR;
	}
	fp = fopen(P_STR(FILE_PHOT_OUT), "a");
	if (fp == NULL) {
		err_printf("Cannot open report file\n");
		return OBS_CMD_ERROR;
	}
	srep = phot_to_fd(imw, fp, REP_ALL|REP_DATASET);
	fflush(fp);
	fclose(fp);
	if (srep != NULL) {
		statusbar2_message(imw, srep, "phot_result", 10000);
		free(srep);
	}
	return OBS_NEXT_COMMAND;
}

static int do_save_cmd (char *args, GtkWidget *dialog)
{	
	void *imw;
	struct image_channel *imch;

	imw = gtk_object_get_data(GTK_OBJECT(dialog), "image_window");
	if (imw == NULL) {
		err_printf("No image window\n");
		return OBS_CMD_ERROR;
	}
	imch = gtk_object_get_data(GTK_OBJECT(imw), "i_channel");
	if ((imch == NULL) || (imch->fr == NULL)) {
		err_printf("No frame to save\n");
		return OBS_CMD_ERROR;
	}
	save_frame_auto_name(imch->fr, dialog);
	return OBS_NEXT_COMMAND;
}

static int do_mphot_cmd (char *args, GtkWidget *dialog)
{
	err_printf("Not implemented yet\n");
	return OBS_CMD_ERROR;
}

static int do_qmatch_cmd (char *args, GtkWidget *dialog)
{
	void *imw;
	int ret;
	imw = gtk_object_get_data(GTK_OBJECT(dialog), "image_window");
	g_return_val_if_fail(imw != NULL, OBS_CMD_ERROR);
	ret = match_field_in_window_quiet(imw);
	if (ret < 0) {
		err_printf("Cannot match\n");
		return OBS_CMD_ERROR;
	}
	return OBS_NEXT_COMMAND;
}

/* tell the scope it's pointing at the object in obs */
static int lx_sync_to_obs(gpointer dialog)
{
	struct obs_data *obs;

	obs = gtk_object_get_data(GTK_OBJECT(dialog), "obs_data");

	if (obs == NULL) {
		err_printf("No obs data for syncing\n");
		return -1;
	}
	return lx200_sync_coords(obs->ra, obs->dec, obs->equinox);
}

/* center item at index in window */
static void center_selected(gpointer user_data, GtkList *list, int index)
{
	GtkWidget *scw;
	GtkAdjustment *vadj;
	double nv;
	int all;

	all = (int)gtk_object_get_data(GTK_OBJECT(user_data), "commands");
	scw = gtk_object_get_data(GTK_OBJECT(user_data), "obs_list_scrolledwin");
	vadj =  gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scw));
	d3_printf("vadj at %.3f\n", vadj->value);
	if (all != 0) {
		nv = (vadj->upper + vadj->lower) * index / all - vadj->page_size / 2.5;
		clamp_double(&nv, vadj->lower, vadj->upper - vadj->page_size);
		gtk_adjustment_set_value(vadj, nv);
		d3_printf("vadj set to %.3f\n", vadj->value);
	}
}

#define MAX_LX_ERRORS 16 /* how many times we retry lx communications */
/* called from the cam_dialog state machine whenever the camera is idle (all 
 * camera ops have ended; tries to go through the obs list */
void obs_list_sm(GtkWidget *dialog)
{
	static ObsState state = OBS_START;
	int index, all, ret;
	GtkWidget *list;
	char *cmd, *cmdo, *cmdh, *cmdho;
	int cl, clo;
	double ddec, dra;
	char mbuf[256];
	static int lx_err_count = 0;
	static struct timeval cmdtimer;

	list = gtk_object_get_data(GTK_OBJECT(dialog), "list1");
	g_return_if_fail(list != NULL);

	if (state != OBS_START)
		d4_printf("obs_list_sm state: %d\n", state);
	switch(state) {
	case OBS_START:
		if (get_named_checkb_val(dialog, "obs_list_run_button")) {
			state = OBS_DO_COMMAND;
			break;
		}
		if (get_named_checkb_val(dialog, "obs_list_step_button")) {
			set_named_checkb_val(dialog, "obs_list_step_button", 0);
			state = OBS_DO_COMMAND;
			break;
		}
		break;
	case OBS_DO_COMMAND:
		index = (int)gtk_object_get_data(GTK_OBJECT(dialog), "index");
		show_stopped_window(dialog, STOPPED_WINDOW_RUNNING);
		cmd = get_cmd_line(GTK_LIST(list), index);
		d3_printf("Command: %s\n", cmd);
		state = do_command(cmd, dialog);
		free(cmd);
		break;
	case OBS_STABILISATION_DELAY_SYNC_GET:
		update_timer(&cmdtimer);
		state = OBS_STABILISATION_DELAY_SYNC_GET2;
		break;
	case OBS_STABILISATION_DELAY_SYNC_GET2:
		d3_printf("stab delay: [%d] %d\n", get_timer_delta(&cmdtimer),
			  P_INT(TELE_STABILISATION_DELAY));
		if (get_timer_delta(&cmdtimer) < P_INT(TELE_STABILISATION_DELAY) ) {
			status_pop(dialog, "obs");
			status_message(dialog, "Waiting to stabilise", 
				       "obs", 10000);
			break;
		} 
		if (!lx_sync_to_obs(dialog)) {	
			state = do_get_cmd(NULL, dialog);
		} else {
			state = OBS_CMD_ERROR;
		}
		break;
	case OBS_STABILISATION_DELAY:
		update_timer(&cmdtimer);
		state = OBS_STABILISATION_DELAY2;
		break;
	case OBS_STABILISATION_DELAY2:
	case OBS_WAIT_SLEW_END2:
		d3_printf("owse2: stab delay: [%d] %d\n", get_timer_delta(&cmdtimer),
			  P_INT(TELE_STABILISATION_DELAY));
		if (get_timer_delta(&cmdtimer) < P_INT(TELE_STABILISATION_DELAY) ) {
			status_pop(dialog, "obs");
			status_message(dialog, "Waiting for mount to stabilise", 
				       "obs", 10000);
		} else {
			state = OBS_NEXT_COMMAND;
		}
		break;
	case OBS_WAIT_SLEW_END:
		ret = lx200_poll_slew_status(&dra, &ddec);
		if (ret == -2) {
			state = OBS_CMD_ERROR;
		} else if (ret < 0) {
			lx_err_count ++;
			status_pop(dialog, "obs");
			status_message(dialog, "Telecope comm err, retrying", 
				       "obs", 10000);
			if (lx_err_count > MAX_LX_ERRORS) 
				state = OBS_CMD_ERROR;
			break;
		} 
		lx_err_count = 0;
		if (ret == 0) {
			status_pop(dialog, "obs");
			status_message(dialog, "Slew finished", "obs", 0);
			d3_printf("slew finished");
			update_timer(&cmdtimer);
			state = OBS_WAIT_SLEW_END2;		
			break;
		}
		snprintf(mbuf, 255, "Slew in progress: %.1f,%.1f to go", dra, ddec);
		status_pop(dialog, "obs");
		status_message(dialog, mbuf, "obs", 0);
		break;
	case OBS_WAIT_SLEW_END_SYNC_GET:
		ret = lx200_poll_slew_status(&dra, &ddec);
		if (ret == -2) {
			state = OBS_CMD_ERROR;
		} else if (ret < 0) {
			lx_err_count ++;
			status_pop(dialog, "obs");
			status_message(dialog, "Telecope comm err, retrying", 
				       "obs", 10000);
			if (lx_err_count > MAX_LX_ERRORS) 
				state = OBS_CMD_ERROR;
			break;
		} 
		lx_err_count = 0;
		if (ret == 0) {
			status_pop(dialog, "obs");
			status_message(dialog, "Slew finished", "obs", 0);
			d3_printf("slew finished, syncing");
			if (!lx_sync_to_obs(dialog)) {	
				state = do_get_cmd(NULL, dialog);
			} else {
				state = OBS_CMD_ERROR;
			}
			break;
		}
		snprintf(mbuf, 255, "Slew in progress: %.1f,%.1f to go", dra, ddec);
		status_pop(dialog, "obs");
		status_message(dialog, mbuf, "obs", 0);
		break;
	case OBS_CMD_ERROR:
		error_beep();
		show_stopped_window(dialog, STOPPED_WINDOW_ERROR);
		status_pop(dialog, "obs");
		status_message(dialog, last_err(), "obs", 10000);
		if (get_named_checkb_val(dialog, "obs_list_err_stop_checkb")) {
			set_named_checkb_val(dialog, "obs_list_run_button", 0);
			state = OBS_START;
//			if (gtk_object_get_data(GTK_OBJECT(dialog), "batch_mode")) {
//				err_printf("Error in obs file processing, exiting\n");
//				gtk_exit(2);
//			}
		} else {
			state = OBS_NEXT_COMMAND;
		}
		break;
	case OBS_SKIP_OBJECT:
		error_beep();
		status_pop(dialog, "obs");
		status_message(dialog, last_err(), "obs", 10000);
		index = (int)gtk_object_get_data(GTK_OBJECT(dialog), "index");
		all = (int)gtk_object_get_data(GTK_OBJECT(dialog), "commands");
		cmdo = get_cmd_line(GTK_LIST(list), index);
		clo = cmd_head(cmdo, &cmdho, NULL);
		index ++;
		while(index < all) {
			cmd = get_cmd_line(GTK_LIST(list), index);
			cl = cmd_head(cmd, &cmdh, NULL);
			if (cl == clo && !strncasecmp(cmdh, cmdho, cl)) {
				state = OBS_NEXT_COMMAND;
				free(cmd);
				break;
			}
			gtk_list_select_item(GTK_LIST(list), index);
			center_selected(dialog, GTK_LIST(list), index);
			index ++;
			d3_printf("skipping %s\n", cmd);
			free(cmd);
		}
		free(cmdo);

		if (index >= all) {
			set_named_checkb_val(dialog, "obs_list_run_button", 0);
			state = OBS_START;
			if (gtk_object_get_data(GTK_OBJECT(dialog), "batch_mode")) {
				err_printf("obs file processing finished successfully\n");
				gtk_exit(0);
			}
			break;
		}
		break;
	case OBS_NEXT_COMMAND:
		index = (int)gtk_object_get_data(GTK_OBJECT(dialog), "index");
		all = (int)gtk_object_get_data(GTK_OBJECT(dialog), "commands");
		if (index + 1 >= all) {
			set_named_checkb_val(dialog, "obs_list_run_button", 0);
			show_stopped_window(dialog, STOPPED_WINDOW_DONE);
			state = OBS_START;
			if (gtk_object_get_data(GTK_OBJECT(dialog), "batch_mode")) {
				err_printf("obs file processing finished successfully\n");
				gtk_exit(0);
			}
			break;
		}
		gtk_list_select_item(GTK_LIST(list), index+1);
		center_selected(dialog, GTK_LIST(list), index+1);
		if (get_named_checkb_val(dialog, "obs_list_run_button")) {
			state = OBS_DO_COMMAND;
		} else {
			state = OBS_START;
		}
		break;
	}
}

static void browse_cb( GtkWidget *widget, gpointer dialog)
{
	GtkWidget *entry;

	entry = gtk_object_get_data(GTK_OBJECT(dialog), "obs_list_fname");
	g_return_if_fail(entry != NULL);
	file_select_to_entry(dialog, entry, "Select Obslist File Name", "*.obs", 1);
}


void obs_list_callbacks(GtkWidget *dialog)
{
	GtkWidget *combo;
	set_named_callback(dialog, "list1", "select-child", obs_list_select_cb);
	set_named_callback(dialog, "obs_list_file_button", "clicked", browse_cb);
	combo = gtk_object_get_data(GTK_OBJECT(dialog), "obs_list_fname_combo");
	gtk_combo_disable_activate(GTK_COMBO(combo));

}

/* callbacks from cameragui.c */
void obs_list_fname_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog = data;
	char *fname, *file = NULL;

	fname = named_entry_text(data, "obs_list_fname");
//	d3_printf("Reusing old obs list\n");
	if (fname == NULL)
		return;
	if ((strchr(fname, '/') == NULL)) {
		file = find_file_in_path(fname, P_STR(FILE_OBS_PATH));
		if (file != NULL) {
			free(fname);
			fname = file;
		}
	}
	obs_list_load_file(dialog, fname);
	free(fname);
}

void obs_list_select_file_cb(GtkWidget *widget, gpointer data)
{
}


static void obs_list_select_cb (GtkList *list, GtkWidget *widget, gpointer user_data)
{
	char *text;
	int index, all;

	gtk_label_get(GTK_LABEL(GTK_BIN(widget)->child), &text);
	index = gtk_list_child_position(list, widget);
	gtk_object_set_data(GTK_OBJECT(user_data), "index", (gpointer)index);
	all = (int)gtk_object_get_data(GTK_OBJECT(user_data), "commands");
	d3_printf("obslist cmd[%d/%d]: %s\n", index, all, text);
}

/* helper functions (that do the real work) */

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
 * pop a message from statusbar
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

/* load a obs list file into the obslist dialog
 */
static int obs_list_load_file(GtkWidget *dialog, char *name)
{
	FILE *fp;
	GtkList *list;
	GtkWidget *list_item;
	GList *dlist = NULL;

	char *line = NULL;
	int len = 0, ret, items = 0;

	d3_printf("obs filename: %s\n", name);
	list = gtk_object_get_data(GTK_OBJECT(dialog), "list1");
	g_return_val_if_fail(list != NULL, -1);

	fp = fopen(name, "r");
	if (fp == NULL) {
		error_beep();
		status_message(dialog, "Cannot open obslist file", "file", 5000);
		return -1;
	}
	gtk_list_clear_items(GTK_LIST(list), 0, -1);
	while ((ret = getline(&line, &len, fp)) > 0) {
		if (line[ret-1] == '\n')
			line[ret-1] = 0; 
		list_item=gtk_list_item_new_with_label(line);
		dlist=g_list_append(dlist, list_item);
		gtk_widget_show(list_item);
		items ++;
	}
	gtk_list_append_items(GTK_LIST(list), dlist);
	gtk_object_set_data(GTK_OBJECT(dialog), "commands", (gpointer)items);
//	gtk_list_select_item(GTK_LIST(list), 0);
	fclose(fp);
	if (line)
		free(line);
	return 0;
}

/* run an obs file; return 0 if the file launches successfuly
 * the obs state machine is instructed to exit the main loop 
 * when the obs list is finished */

int run_obs_file(gpointer window, char *obsf)
{
	GtkWidget *dialog;
	int ret;

	d3_printf("run obs: %s\n", obsf);

/* launch the cam dialog */
	camera_cb(window, 0, NULL);
	dialog = gtk_object_get_data(GTK_OBJECT(window), "cam_dialog");
	if (dialog == NULL) {
		err_printf("Could not create camera dialog\n");
		return -1;
	}
	ret = obs_list_load_file(dialog, obsf);
	if (ret) {
		err_printf("Could not load obs file %s\n", obsf);
		return ret;
	}
	gtk_object_set_data(GTK_OBJECT(dialog), "batch_mode", (void *) 1);
	set_named_checkb_val(dialog, "obs_list_run_button", 1);
	return 0;
}


static void close_stopped_window( GtkWidget *widget, gpointer data )
{
	g_return_if_fail(data != NULL);
	gtk_object_set_data(GTK_OBJECT(data), "stopped_window", NULL);
}

static void close_stopped( GtkWidget *widget, gpointer data )
{
	GtkWidget *im_window;
	im_window = gtk_object_get_data(GTK_OBJECT(data), "window");
	g_return_if_fail(im_window != NULL);
	gtk_object_set_data(GTK_OBJECT(im_window), "stopped_window", NULL);
}

static void get_color_action(int action, GdkColor *color, GdkColormap *cmap)
{
	switch(action) {
	case STOPPED_WINDOW_RUNNING:
		color->red = 0;
		color->green = 0xffff;
		color->blue = 0;
		break;
	case STOPPED_WINDOW_ERROR:
		color->red = 0xffff;
		color->green = 0;
		color->blue = 0;
		break;
	case STOPPED_WINDOW_DONE:
		color->red = 0;
		color->green = 0;
		color->blue = 0xffff;
		break;
	default:
		color->red = 0xffff;
		color->green = 0xffff;
		color->blue = 0;
		break;
	}
	if (!gdk_color_alloc(cmap, color)) {
		g_error("couldn't allocate color");
	}
}

static gboolean on_area_expose(GtkWidget *darea, GdkEventExpose *event, gpointer frm)
{
	GdkGC *gc;
	GdkColormap *cmap;
	GdkColor color;
	int action;

	cmap = gdk_colormap_get_system();
	action = (int)gtk_object_get_data(GTK_OBJECT(darea), "action");
	get_color_action(action, &color, cmap);

	gc = gdk_gc_new(darea->window);
	gdk_gc_set_foreground(gc, &color);
	gdk_draw_rectangle(darea->window, gc, 1, event->area.x, event->area.y, 
			   event->area.width, event->area.height);
	gdk_gc_destroy(gc);

	return TRUE;
}

static void set_stopped_color(GtkWidget *window, int action)
{
	GtkWidget *darea;

	darea = gtk_object_get_data(GTK_OBJECT(window), "drawing_area");
	g_return_if_fail(darea != NULL);
	gtk_object_set_data(GTK_OBJECT(darea), "action", (gpointer)action);
}

/* show the stopped window */
void show_stopped_window(gpointer window, int action)
{
	GtkWidget *dialog;
	GtkWidget *darea;

	dialog = gtk_object_get_data(GTK_OBJECT(window), "stopped_window");
	if (action == STOPPED_WINDOW_RUNNING && dialog != NULL) {
		gtk_widget_hide(dialog);
		return;
	}
	if (P_INT(SHOW_STATUS_WINDOW) == 0)
		return;
	if (dialog == NULL) {
		dialog = create_obscript_stopped();
		darea = gtk_object_get_data(GTK_OBJECT(dialog), "drawing_area");
		gtk_object_set_data(GTK_OBJECT(dialog), "window",
					 window);
		gtk_object_set_data_full(GTK_OBJECT(window), "stopped_window",
					 dialog, (GtkDestroyNotify)(gtk_widget_destroy));
		gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
				    GTK_SIGNAL_FUNC (close_stopped_window), window);
		set_named_callback (GTK_OBJECT (dialog), "close_button", "clicked",
				    GTK_SIGNAL_FUNC (close_stopped));
		gtk_signal_connect(GTK_OBJECT(darea), "expose_event", 
				   GTK_SIGNAL_FUNC(on_area_expose), NULL);
		set_stopped_color(dialog, action);
		gtk_widget_show(dialog);
	} else {
		set_stopped_color(dialog, action);
		gtk_widget_show(dialog);
		gtk_widget_queue_draw(dialog);
		gdk_window_raise(dialog->window);
	}
}
