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

/* Here we handle the main image window and menus */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
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

/* yes/no modal dialog */

static void yes_no_yes( GtkWidget *widget, gpointer data )
{
	int *retv;
	retv = gtk_object_get_data(GTK_OBJECT(data), "return_val");
	if (retv != NULL)
		*retv = 1;
	gtk_widget_destroy(data);
}

static void yes_no_no( GtkWidget *widget, gpointer data )
{
	gtk_widget_destroy(data);
}

static void close_yes_no( GtkWidget *widget, gpointer data )
{
	gtk_main_quit();
}


/* create the modal y/n dialog and run the mainloop until it exits
 * display text as a bunch of labels. newlines inside the text 
 * return 0/1 for n/y, or -1 for errors */
/* title is the window title; if NULL, 'gcx question' is used */
int modal_yes_no(char *text, char *title)
{
	int retval = 0;

	GtkWidget *dialog, *label, *vbox;
	dialog = create_yes_no();
	gtk_object_set_data(GTK_OBJECT(dialog), "return_val", &retval); 
	vbox = gtk_object_get_data(GTK_OBJECT(dialog), "vbox");
	g_return_val_if_fail(vbox != NULL, 0);
	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    GTK_SIGNAL_FUNC (close_yes_no), dialog);
	set_named_callback (GTK_OBJECT (dialog), "yes_button", "clicked",
			    GTK_SIGNAL_FUNC (yes_no_yes));
	set_named_callback (GTK_OBJECT (dialog), "no_button", "clicked",
			    GTK_SIGNAL_FUNC (yes_no_no));
	if (title != NULL) {
		gtk_window_set_title(GTK_WINDOW(dialog), title);
	}
	if (text != NULL) { /* add the message */
		label = gtk_label_new (text);
		gtk_widget_ref (label);
		gtk_object_set_data_full (GTK_OBJECT (dialog), "message", label,
					  (GtkDestroyNotify) gtk_widget_unref);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		gtk_misc_set_alignment (GTK_MISC (label), 0.5, 1);
		gtk_widget_show (label);
	}
	gtk_widget_show(dialog);
	gtk_main();
	return retval;
}

static void entry_prompt_yes( GtkWidget *widget, gpointer data )
{
	int *retv;
	char **value;
	GtkWidget *entry;

	entry = gtk_object_get_data(GTK_OBJECT(data), "entry");
	g_return_if_fail(entry != NULL);
	value = gtk_object_get_data(GTK_OBJECT(data), "entry_val");
	if (value != NULL) {
		*value = strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	}
	retv = gtk_object_get_data(GTK_OBJECT(data), "return_val");
	if (retv != NULL)
		*retv = 1;

	gtk_widget_destroy(data);
}


/* prompt the user for a value; return 1 if ok was pressed, 0 otehrwise
 * update the value pointer with a maloced string when ok was pressed */

int modal_entry_prompt(char *text, char *title, char *initial, char **value)
{
	int retval = 0;

	GtkWidget *dialog, *label, *entry;
	dialog = create_entry_prompt();
	gtk_object_set_data(GTK_OBJECT(dialog), "return_val", &retval); 
	gtk_object_set_data(GTK_OBJECT(dialog), "entry_val", value); 
	entry = gtk_object_get_data(GTK_OBJECT(dialog), "entry");
	g_return_val_if_fail(entry != NULL, 0);
	label = gtk_object_get_data(GTK_OBJECT(dialog), "label");
	g_return_val_if_fail(entry != NULL, 0);
	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    GTK_SIGNAL_FUNC (close_yes_no), dialog);
	set_named_callback (GTK_OBJECT (dialog), "ok_button", "clicked",
			    GTK_SIGNAL_FUNC (entry_prompt_yes));
	set_named_callback (GTK_OBJECT (dialog), "entry", "activate",
			    GTK_SIGNAL_FUNC (entry_prompt_yes));
	set_named_callback (GTK_OBJECT (dialog), "cancel_button", "clicked",
			    GTK_SIGNAL_FUNC (yes_no_no));
	if (title != NULL) {
		gtk_window_set_title(GTK_WINDOW(dialog), title);
	}
	if (text != NULL) { /* add the message */
		gtk_label_set_text(GTK_LABEL(label), text);
	}
	if (initial != NULL) {
		gtk_entry_set_text(GTK_ENTRY(entry), initial);
	}
	gtk_widget_show(dialog);
	gtk_main();
	return retval;
}





GtkWidget* create_about_cx (void);

/* beep - or otherwise show a user action may be in error */
void error_beep(void)
{
	gdk_beep();
}

void warning_beep(void)
{
//	gdk_beep();
}

/* make a new status ref */
struct status_ref *new_status_ref(gpointer statusbar, guint context_id, guint msg_id)
{
	struct status_ref *newsr;
//	d3_printf("new status ref\n");
	newsr = malloc(sizeof (struct status_ref));
	newsr->statusbar = statusbar;
	newsr->context_id = context_id;
	newsr->msg_id = msg_id;
	return newsr;
}

gboolean remove_status_msg(struct status_ref *data)
{
	g_return_val_if_fail(data != NULL, FALSE);
	if (data->statusbar == NULL)
		return FALSE;
//	d3_printf("status remove\n");
	gtk_statusbar_remove(GTK_STATUSBAR(data->statusbar), 
			     data->context_id, data->msg_id);
	free(data);
	return FALSE;
}

/*
 * print a time message in statusbar 1
 * time = 0 -> infinite time
 */
void statusbar1_message(GtkWidget *window, char *message, char *context, int time)
{
	GtkWidget *statusbar;
	guint msg_id, context_id;
	gpointer stref;

	statusbar = gtk_object_get_data(GTK_OBJECT(window), "statusbar1");
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
void statusbar1_pop(GtkWidget *window, char *context)
{
	GtkWidget *statusbar;
	guint context_id;

	statusbar = gtk_object_get_data(GTK_OBJECT(window), "statusbar1");
	if (statusbar) {
		context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar),
						  context );
		gtk_statusbar_pop(GTK_STATUSBAR(statusbar), context_id);
	}
}


/*
 * print a time message in statusbar 2
 * time = 0 -> infinite time
 */
void statusbar2_message(GtkWidget *window, char *message, char *context, int time)
{
	GtkWidget *statusbar;
	guint msg_id, context_id;
	gpointer stref;

	statusbar = gtk_object_get_data(GTK_OBJECT(window), "statusbar2");
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
 * pop a message from statusbar 2
 */
void statusbar2_pop(GtkWidget *window, char *context)
{
	GtkWidget *statusbar;
	guint context_id;

	statusbar = gtk_object_get_data(GTK_OBJECT(window), "statusbar2");
	if (statusbar) {
		context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar),
						  context );
		gtk_statusbar_pop(GTK_STATUSBAR(statusbar), context_id);
	}
}

int error_message_sb2(gpointer window, char *message)
{
	error_beep();
	statusbar2_message(window, message, "error", 7500);
	return 0;
}

#define ERR_SIZE 1024
/* print the error string and save it to storage */
int info_printf_sb2(gpointer window, char *context, int time, const char *fmt, ...)
{
	va_list ap, ap2;
	int ret;
	char err_string[ERR_SIZE+1];
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
	ret = vfprintf(stderr, fmt, ap);
	statusbar2_message(window, err_string, context, time);
	va_end(ap);
	return ret;
}


#define ERR_SIZE 1024
/* print the error string and save it to storage */
int err_printf_sb2(gpointer window, const char *fmt, ...)
{
	va_list ap, ap2;
	int ret;
	char err_string[ERR_SIZE+1];
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
	ret = vfprintf(stderr, fmt, ap);
	error_message_sb2(window, err_string);
	va_end(ap);
	return ret;
}


static void about_cx(gpointer data)
{
	GtkWidget *about_cx;
	about_cx = create_about_cx();
	gtk_widget_ref(about_cx);
	gtk_widget_show(about_cx);
}

void close_about_cx( GtkWidget *widget, gpointer data )
{
	gtk_widget_destroy(GTK_WIDGET(data));
}

static int destroy_cb(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
	return 1;
}

static void user_quit_action(gpointer data, guint action, GtkWidget *menu_item)
{
	d3_printf("user quit\n");
	gtk_widget_destroy(GTK_WIDGET(data));
}

int window_auto_pairs(gpointer window)
{
	struct gui_star_list *gsl;
	int ret;
	char buf[256];

	gsl = gtk_object_get_data(GTK_OBJECT(window), "gui_star_list");
	if (gsl == NULL)
		return -1;
	statusbar2_message(window, "Looking for Star Pairs...", "sources", 0);
	while (gtk_events_pending ())
		gtk_main_iteration ();
	remove_pairs(window, 0);
	ret = auto_pairs(gsl);
	statusbar2_pop(window, "sources");
	if (ret < 1) {
		sprintf(buf, "%s", last_err());
	} else {
		sprintf(buf, "Found %d Matching Pairs", ret);
	}
	statusbar2_message(window, buf, "sources", 5000);
	gtk_widget_queue_draw(window);
	return ret;
}

void star_pairs_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *window = data;
	switch(action) {
	case PAIRS_AUTO:
		window_auto_pairs(window);
		break;
	default:
		err_printf("unknown action %d in star_pairs_cb\n", action);
	}
}

static void new_frame_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	struct ccd_frame *fr;
	fr = new_frame(P_INT(FILE_NEW_WIDTH), P_INT(FILE_NEW_HEIGHT));
	frame_to_channel(fr, data, "i_channel");
}

void star_rm_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *window = data;
	int ret;

	switch(action) {
	case STAR_RM_ALL:
		remove_stars(window, TYPE_MASK_ALL, 0);
		break;
	case STAR_RM_FR:
		remove_stars(window, TYPE_MASK(STAR_TYPE_SIMPLE), 0);
		break;
	case STAR_RM_USER:
		remove_stars(window, TYPE_MASK(STAR_TYPE_USEL), 0);
		break;
	case STAR_RM_FIELD:
		remove_stars(window, TYPE_MASK(STAR_TYPE_SREF), 0);
		break;
	case STAR_RM_CAT:
		remove_stars(window, TYPE_MASK(STAR_TYPE_CAT), 0);
		break;
	case STAR_RM_SEL:
		remove_stars(window, TYPE_MASK_ALL, STAR_SELECTED);
		break;
	case STAR_RM_PAIRS_ALL:
		remove_pairs(window, 0);
		break;
	case STAR_RM_PAIRS_SEL:
		remove_pairs(window, STAR_SELECTED);
		break;
	case STAR_RM_OFF:
		ret = remove_off_frame_stars(window);
		if (ret >= 0) {
			info_printf_sb2(window, "stars", 5000, 
				       " %d off-frame stars removed", ret);
		}
		break;
	default:
		err_printf("unknown action %d in star_rm_cb\n", action);
	}
}

static GtkItemFactoryEntry star_popup_menu_items[] = {
	{"/_Edit Star", NULL, star_popup_cb, STARP_EDIT_AP, "<Item>"},
//	{"/Make _Std Star", NULL, star_popup_cb, STARP_MAKE_STD, "<Item>"},
//	{"/Make C_at Star", NULL, star_popup_cb, STARP_MAKE_CAT, "<Item>"},
	{"/_Remove Star", NULL, star_popup_cb, STARP_UNMARK_STAR, "<Item>"},
	{"/_Create Pair", NULL, star_popup_cb, STARP_PAIR, "<Item>"},
	{"/_Remove Pair", NULL, star_popup_cb, STARP_PAIR_RM, "<Item>"},
//	{"/_Information", NULL, star_popup_cb, STARP_INFO, "<Item>"},
};


static GtkItemFactoryEntry image_popup_menu_items[] = {
	{ "/_File",		NULL,         	NULL,  		0, "<Branch>" },
/*  	{ "/File/tear",  	NULL,         	NULL,  		0, "<Tearoff>" }, */
	{ "/File/_New Frame",	"<control>n", 	new_frame_cb, 	0, "<Item>" },
	{ "/File/_Open Fits",	"<control>o", 	file_popup_cb, 	FILE_OPEN, "<Item>" },
	{ "/File/_Save Fits As...", "<control>s", file_popup_cb, FILE_SAVE_AS, "<Item>" },
	{ "/File/_Export Image",	NULL, 	NULL, 	0, "<Branch>" },
	{ "/File/_Export Image/_8-bit pnm", NULL, file_popup_cb, FILE_EXPORT_PNM8, "<Item>" },
	{ "/File/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/File/Load _Field Stars", NULL, NULL, 0, "<Branch>"},
	{ "/File/Load _Field Stars/From GSC-_2 File", "<control>g", file_popup_cb, 
	  FILE_LOAD_GSC2, "<Item>"},
	{ "/File/Load _Field Stars/From _GSC Catalog", "g", find_stars_cb, 
	  ADD_STARS_GSC, "<Item>"},
	{ "/File/Load _Field Stars/From _Tycho2 Catalog", "<control>t", find_stars_cb, 
	  ADD_STARS_TYCHO2, "<Item>"},
	{ "/File/Load _Recipy",	NULL, file_popup_cb, FILE_OPEN_RCP, "<Item>" },
	{ "/File/_Create Recipy",	NULL, create_recipy_cb, 0, "<Item>" },
//	{ "/File/_Close",	"<control>c", 	file_popup_cb, 	FILE_CLOSE, "<Item>" },

	{ "/File/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/File/Show Fits _Header...",  "<shift>h", 	fits_header_cb, 1, "<Item>" },
	{ "/File/Edit O_ptions...",	"o", 	edit_options_cb, 1, "<Item>" },
	{ "/File/Camera and Telescope...",	"<shift>c", 	camera_cb, 1, "<Item>" },
	{ "/File/Guiding...",	"<shift>t", 	open_guide_cb, 1, "<Item>" },
	{ "/File/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/File/_Quit",	"<control>Q", 	user_quit_action, 0, "<Item>" },


	{ "/_Image",      	NULL,   	NULL, 		0, "<Branch>" },
//	{ "/Image/_Show Stats",  	"s",    	stats_cb,       1, "<Item>" },
	{ "/Image/sep",		NULL,   	NULL,  		0, "<Separator>" },
	{ "/Image/Curves&Histogram...",  "c", 	histogram_cb, 1, "<Item>" },
	{ "/Image/sep",		NULL,   	NULL,  		0, "<Separator>" },
	{ "/Image/Zoom _In",  	"equal",    	view_option_cb, VIEW_ZOOM_IN, "<Item>" },
	{ "/Image/Zoom _Out",  	"minus",	view_option_cb, VIEW_ZOOM_OUT, "<Item>" },
//	{ "/Image/Zoom A_ll", 	"bracketleft", view_option_cb, VIEW_ZOOM_FIT, "<Item>" },
	{ "/Image/Actual _Pixels", "bracketright", view_option_cb, VIEW_PIXELS, "<Item>" },
	{ "/Image/sep1",		NULL,   	NULL,  		0, "<Separator>" },
	{ "/Image/Pan _Center",	"<control>l",	view_option_cb, VIEW_PAN_CENTER, "<Item>" },
	{ "/Image/_Pan Cursor",	"space", view_option_cb, VIEW_PAN_CURSOR, "<Item>" },
	{ "/Image/sep2",		NULL,   	NULL,  		0, "<Separator>" },
	{ "/Image/_Auto Cuts", 	"0",		cuts_option_cb, CUTS_AUTO, "<Item>" },
	{ "/Image/_Min-Max Cuts", "9",		cuts_option_cb, CUTS_MINMAX, "<Item>" },
	{ "/Image/_Flatter", 	"f",		cuts_option_cb, CUTS_FLATTER, "<Item>" },
	{ "/Image/S_harper", 	"h",		cuts_option_cb, CUTS_SHARPER, "<Item>" },
	{ "/Image/_Brighter", 	"b",		cuts_option_cb, CUTS_BRIGHTER, "<Item>" },
	{ "/Image/_Darker", 	"d",		cuts_option_cb, CUTS_DARKER, "<Item>" },

	{ "/Image/Set _Contrast", NULL, 		NULL, 		0, "<Branch>" },
	{ "/Image/Set Contrast/_4 sigma", "1", 	cuts_option_cb, CUTS_CONTRAST|1, "<Item>" },
	{ "/Image/Set Contrast/5_.6 sigma", "2", cuts_option_cb, CUTS_CONTRAST|2, "<Item>" },
	{ "/Image/Set Contrast/_8 sigma", "3", 	cuts_option_cb, CUTS_CONTRAST|3, "<Item>" },
	{ "/Image/Set Contrast/_11 sigma", "4", cuts_option_cb, CUTS_CONTRAST|4, "<Item>" },
	{ "/Image/Set Contrast/1_6 sigma", "5", cuts_option_cb, CUTS_CONTRAST|5, "<Item>" },
	{ "/Image/Set Contrast/22 _sigma", "6", cuts_option_cb, CUTS_CONTRAST|6, "<Item>" },
	{ "/Image/Set Contrast/45 s_igma", "7", cuts_option_cb, CUTS_CONTRAST|7, "<Item>" },
	{ "/Image/Set Contrast/90 si_gma", "8", cuts_option_cb, CUTS_CONTRAST|8, "<Item>" },
	{ "/Image/Set Contrast/_Min-Max", NULL, 	cuts_option_cb, CUTS_MINMAX, "<Item>" },


	{ "/_Stars",      	NULL,   	NULL, 		0, "<Branch>" },
	{ "/Stars/_Detect Sources",  "s", find_stars_cb, ADD_STARS_DETECT, "<Item>" },
	{ "/Stars/Show _Target", "t", find_stars_cb, ADD_STARS_OBJECT, "<Item>"},
	{ "/Stars/Add From _Catalog", "a", find_stars_cb, ADD_FROM_CATALOG, "<Item>"},
//	{ "/Stars/sep",		NULL,         	NULL,  		0, "<Separator>" },
//	{ "/Stars/_Mark Stars", NULL, selection_mode_cb, SEL_ACTION_MARK_STARS, "<Item>" },
//	{ "/Stars/Reset Sel Mode", "Escape", selection_mode_cb, SEL_ACTION_NORMAL, "<Item>" },
//	{ "/Stars/Show _Sources",  NULL,	NULL,  		0, "<CheckItem>" },
	{ "/Stars/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/Stars/_Edit",  "<control>e", star_edit_cb, STAR_EDIT, "<Item>" },
	{ "/Stars/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/Stars/Remove Selecte_d",  "<control>d", star_rm_cb, STAR_RM_SEL, "<Item>" },
	{ "/Stars/Remove _Detected Stars", "<shift>s", star_rm_cb, STAR_RM_FR, "<Item>" },
	{ "/Stars/Remove _User Stars",  "<shift>u", star_rm_cb, STAR_RM_USER, "<Item>" },
	{ "/Stars/Remove Fie_ld Stars",  "<shift>f", star_rm_cb, STAR_RM_FIELD, "<Item>" },
	{ "/Stars/_Remove Catalog Objects",  "<shift>g", star_rm_cb, STAR_RM_CAT, "<Item>" },
	{ "/Stars/Remo_ve Off-Frame",  "<shift>o", star_rm_cb, STAR_RM_OFF, "<Item>" },
	{ "/Stars/Remove _All", "<shift>a",	star_rm_cb, STAR_RM_ALL, "<Item>" },
	{ "/Stars/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/Stars/Remove All Pairs", "<shift>p", star_rm_cb, STAR_RM_PAIRS_ALL, "<Item>" },
	{ "/Stars/Remove Selected _Pairs",  NULL, star_rm_cb, STAR_RM_PAIRS_SEL, "<Item>" },
	{ "/Stars/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/Stars/_Brighter",  "<control>b", star_display_cb, STAR_BRIGHTER, "<Item>" },
	{ "/Stars/_Fainter",  "<control>f", star_display_cb, STAR_FAINTER, "<Item>" },
	{ "/Stars/Redra_w",  "<control>r", star_display_cb, STAR_REDRAW, "<Item>" },

//	{ "/Stars/D_etection Options...",  NULL,	NULL,  	0, "<Item>" },
//	{ "/Stars/S_tar Display Options...",  NULL,	NULL,  	0, "<Item>" },
//	{ "/Stars/_Catalogs...",  NULL,	NULL,  	0, "<Item>" },

	{ "/_Wcs",      	NULL,   	NULL, 		0, "<Branch>" },
	{ "/Wcs/_Edit Wcs",  "<shift>w", wcsedit_cb, 0, "<Item>" },
	{ "/Wcs/_Auto Wcs",  "m", wcs_cb, WCS_AUTO, "<Item>" },
	{ "/Wcs/_Quiet Auto Wcs",  "<shift>m", wcs_cb, WCS_QUIET_AUTO, "<Item>" },
	{ "/Wcs/Auto _Pairs", "p",	star_pairs_cb, PAIRS_AUTO, "<Item>" },
	{ "/Wcs/Fit _Wcs from Pairs",  "<shift>w", wcs_cb, WCS_FIT, "<Item>" },
	{ "/Wcs/_Reload from Frame",  "<shift>r", wcs_cb, WCS_RELOAD, "<Item>" },
	{ "/Wcs/_Force Validate",  NULL, wcs_cb, WCS_FORCE_VALID, "<Item>" },

	{ "/_Processing",         	NULL,         	NULL, 		0, "<Branch>" },
	{ "/Processing/_Next Frame",  "n",    	switch_frame_cb, SWF_NEXT, "<Item>" },
	{ "/Processing/_Skip Frame",  "k",	switch_frame_cb, SWF_SKIP, "<Item>" },
	{ "/Processing/_Previous Frame",  "j",	switch_frame_cb, SWF_PREV, "<Item>" },
	{ "/Processing/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/Processing/_Center Stars",  NULL, photometry_cb, PHOT_CENTER_STARS, "<Item>" },
//	{ "/Processing/_Center Stars and Plot Errors",  NULL, photometry_cb, PHOT_CENTER_PLOT, "<Item>" },
	{ "/Processing/Quick Aperture P_hotometry",  "<shift>p", photometry_cb, PHOT_RUN, "<Item>" },
//	{ "/Processing/AP to _Multi-Frame",  "<control>p", photometry_cb, PHOT_RUN|PHOT_TO_MBDS, "<Item>" },
	{ "/Processing/Aperture Photometry to _File", NULL, photometry_cb, PHOT_RUN|PHOT_TO_FILE, "<Item>" },
	{ "/Processing/Aperture Photometry to _AAVSO File", NULL, photometry_cb, PHOT_RUN|PHOT_TO_FILE_AA, "<Item>" },
	{ "/Processing/Aperture Photometry to _stdout", NULL, photometry_cb, PHOT_RUN|PHOT_TO_STDOUT, "<Item>" },
	{ "/Processing/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/Processing/_CCD Reduction...", 	"l",  	processing_cb, 		1, "<Item>" },
	{ "/Processing/_Multi-frame Reduction...", "<control>m", mband_open_cb,1, "<Item>" },

	{ "/_Help",         	NULL,         	NULL, 		0, "<Branch>" },
	{ "/Help/_GUI help", "F1",  	help_page_cb,  HELP_BINDINGS, "<Item>" },
	{ "/Help/Show _Command Line Options", NULL,  	help_page_cb,  HELP_USAGE, "<Item>" },
	{ "/Help/Show _Obscript Commands", NULL,  	help_page_cb,  HELP_OBSCRIPT, "<Item>" },
	{ "/Help/Show _Report Converter Help", NULL,  	help_page_cb,  HELP_REPCONV, "<Item>" },
	{ "/Help/_About",   	NULL, 		about_cx, 0, "<Item>" },
};


void destroy( GtkWidget *widget,
              gpointer   data )
{
	gtk_main_quit ();
}

void show_xy_status(GtkWidget *window, double x, double y)
{
	char buf[64];
	GtkWidget *statusbar;
	guint msg_id, context_id;
	gpointer stref;

	statusbar = gtk_object_get_data(GTK_OBJECT(window), "statusbar1");
	if (statusbar == NULL)
		return;

	context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar),
						  "coords" );
	sprintf(buf, "%.0f, %.0f", x, y);
	msg_id = gtk_statusbar_push(GTK_STATUSBAR(statusbar), context_id, buf);
	stref = new_status_ref(statusbar, context_id, msg_id);
	gtk_timeout_add(1000, (GtkFunction)remove_status_msg, stref);
}

/*
static void pop_free(gpointer p)
{
	d3_printf("pop_free\n");
}
*/

/*
 * mouse button event callback. It is normally called after the callback in
 * sourcesdraw has decided not to act on the click
 */
static gboolean image_clicked_cb(GtkWidget *w, GdkEventButton *event, gpointer data)
{
	GtkMenu *menu;
	GSList *found;
	GtkItemFactory *star_if;

//	printf("button press : %f %f state %08x button %08x \n",
//	       event->x, event->y, event->state, event->button);
	if (event->button == 3) {
		show_region_stats(data, event->x, event->y);
		found = stars_under_click(GTK_WIDGET(data), event);
		star_if = gtk_object_get_data(GTK_OBJECT(data), "star_popup_if");
		menu = gtk_object_get_data(GTK_OBJECT(data), "image_popup");
		if (found != NULL && star_if != NULL) {
			if (found != NULL)
				g_slist_free(found);
			return FALSE;
		}
		if(menu) {
			printf("menu=%08x\n", (unsigned int)menu);
			gtk_menu_popup(menu, NULL, NULL, NULL, NULL, 
				       event->button, event->time);
		}
	}
	if (event->button == 2) {
		show_region_stats(data, event->x, event->y);
		pan_cursor(data);
	}
	if (event->button == 1) {
		found = stars_under_click(GTK_WIDGET(data), event);
		if (!(event->state & GDK_SHIFT_MASK) && (found == NULL)) 
			gsl_unselect_all(data);
		if ((event->state & GDK_CONTROL_MASK) || (found != NULL)) {
			g_slist_free(found);
			return FALSE;
		}
		show_region_stats(data, event->x, event->y);
	}
	return TRUE;
}

#define DRAG_MIN_MOVE 2
static gint motion_event_cb (GtkWidget *widget, GdkEventMotion *event, gpointer window)
{
	int x, y, dx, dy;
	GdkModifierType state;
	static int ox, oy;

	if (event->is_hint)
		gdk_window_get_pointer (event->window, &x, &y, &state);
	else
	{
		x = event->x;
		y = event->y;
		state = event->state;
	}
//	d3_printf("motion %d %d state %d\n", x - ox, y - oy, state);
//	show_xy_status(window, 1.0 * x, 1.0 * y);
	if (state & GDK_BUTTON1_MASK) {
		dx = x - ox;
		dy = y - oy;
//		printf("moving by %d %d\n", x - ox, y - oy);
		if (abs(dx) > DRAG_MIN_MOVE || abs(dy) > DRAG_MIN_MOVE) {
			if (dx > DRAG_MIN_MOVE)
				dx -= DRAG_MIN_MOVE;
			else if (dx < -DRAG_MIN_MOVE)
				dx += DRAG_MIN_MOVE;
			else 
				dx = 0;
			if (dy > DRAG_MIN_MOVE)
				dy -= DRAG_MIN_MOVE;
			else if (dy < -DRAG_MIN_MOVE)
				dy += DRAG_MIN_MOVE;
			else 
				dy = 0;
			drag_adjust_cuts(window, dx, dy);
			ox = x; 
			oy = y;
		}
	} else {
		ox = x; 
		oy = y;
	}
	return TRUE;
}

GtkItemFactory *get_star_popup_menu(gpointer data)
{
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;
	gint nmenu_items = sizeof (star_popup_menu_items) / 
		sizeof (star_popup_menu_items[0]);
	accel_group = gtk_accel_group_new ();

	item_factory = gtk_item_factory_new (GTK_TYPE_MENU, 
					     "<star_popup>", accel_group);
	gtk_item_factory_create_items (item_factory, nmenu_items, 
				       star_popup_menu_items, data);

	return item_factory;
}



GtkWidget *get_image_popup_menu(GtkWidget *window)
{
	GtkWidget *ret;
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;
	gint nmenu_items = sizeof (image_popup_menu_items) / 
		sizeof (image_popup_menu_items[0]);
	accel_group = gtk_accel_group_new ();

	item_factory = gtk_item_factory_new (GTK_TYPE_MENU, 
					     "<image_popup>", accel_group);
	gtk_object_set_data_full(GTK_OBJECT(window), "image_popup_if", item_factory,
				 (GtkDestroyNotify) gtk_object_unref);
	gtk_item_factory_create_items (item_factory, nmenu_items, 
				       image_popup_menu_items, window);

  /* Attach the new accelerator group to the window. */
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

    /* Finally, return the actual menu bar created by the item factory. */ 
	ret = gtk_item_factory_get_widget (item_factory, "<image_popup>");
	return ret;
}

static GtkWidget *get_main_menu_bar(GtkWidget *window)
{
	GtkWidget *ret;
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;
	gint nmenu_items = sizeof (image_popup_menu_items) / 
		sizeof (image_popup_menu_items[0]);
	accel_group = gtk_accel_group_new ();

	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, 
					     "<main_menu>", accel_group);
	gtk_object_set_data_full(GTK_OBJECT(window), "main_menu_if", item_factory,
				 (GtkDestroyNotify) gtk_object_unref);
	gtk_item_factory_create_items (item_factory, nmenu_items, 
				       image_popup_menu_items, window);

  /* Attach the new accelerator group to the window. */
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

    /* Finally, return the actual menu bar created by the item factory. */ 
	ret = gtk_item_factory_get_widget (item_factory, "<main_menu>");
	return ret;
}


GtkWidget* create_hpaned1 (GtkWidget *window)
{
	GtkWidget *hpaned1;
	GtkWidget *statusbar1;
	GtkWidget *statusbar2;
	
	hpaned1 = gtk_hpaned_new ();
	gtk_widget_ref (hpaned1);
	gtk_object_set_data_full (GTK_OBJECT (window), "hpaned1", hpaned1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (hpaned1);
	gtk_paned_set_gutter_size (GTK_PANED (hpaned1), 7);
	gtk_paned_set_position (GTK_PANED (hpaned1), 200);

	statusbar1 = gtk_statusbar_new ();
	gtk_widget_ref (statusbar1);
	gtk_object_set_data_full (GTK_OBJECT (window), "statusbar1", statusbar1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (statusbar1);
	gtk_paned_pack1 (GTK_PANED (hpaned1), statusbar1, FALSE, TRUE);
	
	statusbar2 = gtk_statusbar_new ();
	gtk_widget_ref (statusbar2);
	gtk_object_set_data_full (GTK_OBJECT (window), "statusbar2", statusbar2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (statusbar2);
	gtk_paned_pack2 (GTK_PANED (hpaned1), statusbar2, FALSE, TRUE);

	return hpaned1;
}


GtkWidget * create_image_window()
{	
	GtkWidget *window;
	GtkWidget *scrolled_window;
	GtkWidget *image;
	GtkWidget *alignment;
	GtkWidget *image_popup;
	GtkWidget *vbox;
	GtkWidget *hpaned1;
	GtkWidget *menubar;
	GtkItemFactory *star_popup_factory;

	image = gtk_drawing_area_new();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  GTK_SIGNAL_FUNC (destroy_cb), NULL);

	gtk_window_set_title (GTK_WINDOW (window), "gcx");
	gtk_container_set_border_width (GTK_CONTAINER (window), 0);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	vbox = gtk_vbox_new(0, 0);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
//					GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
	alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_widget_show(alignment);

	hpaned1 = create_hpaned1(window);
	gtk_widget_show(hpaned1);

	menubar = get_main_menu_bar(window);
	gtk_widget_show(menubar);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, 1, 1, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hpaned1, 0, 0, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), alignment);
	gtk_container_add(GTK_CONTAINER(alignment), image);

	gtk_signal_connect(GTK_OBJECT(scrolled_window), "button_press_event", 
			 GTK_SIGNAL_FUNC(sources_clicked_cb), window);
	gtk_signal_connect(GTK_OBJECT(scrolled_window), "button_press_event", 
			 GTK_SIGNAL_FUNC(image_clicked_cb), window);
	gtk_signal_connect(GTK_OBJECT(image), "motion_notify_event", 
			 GTK_SIGNAL_FUNC(motion_event_cb), window);
	gtk_signal_connect(GTK_OBJECT(image), "expose_event", 
			 GTK_SIGNAL_FUNC(image_expose_cb), window);

	gtk_widget_set_events(image,  GDK_BUTTON_PRESS_MASK 
			      | GDK_POINTER_MOTION_MASK
			      | GDK_POINTER_MOTION_HINT_MASK);

	gtk_object_set_data(GTK_OBJECT(window), "scrolled_window", scrolled_window);
	gtk_object_set_data(GTK_OBJECT(window), "image_alignment", alignment);
	gtk_object_set_data(GTK_OBJECT(window), "image", image);

  	gtk_window_set_default_size(GTK_WINDOW(window), 700, 500); 

	gtk_widget_show(image);
	gtk_widget_show (scrolled_window);
	gtk_widget_show(vbox);

	image_popup = get_image_popup_menu(window);
	gtk_object_set_data_full(GTK_OBJECT(window), "image_popup", image_popup,
			    (GtkDestroyNotify) gtk_widget_unref);
	star_popup_factory = get_star_popup_menu(window);
	gtk_object_set_data_full(GTK_OBJECT(window), "star_popup_if", star_popup_factory,
			    (GtkDestroyNotify) gtk_object_unref);

//	gtk_widget_show_all(window);
	gtk_widget_show(image_popup);
	gdk_rgb_init();
	return window;
}

GtkWidget* create_about_cx (void)
{
	GtkWidget *about_cx;
	GtkWidget *dialog_vbox1;
	GtkWidget *vbox1;
	GtkWidget *frame1;
	GtkWidget *vbox2;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *label5;
	GtkWidget *label6;
	GtkWidget *label7;
	GtkWidget *label12;
	GtkWidget *hseparator1;
	GtkWidget *label8;
	GtkWidget *label9;
	GtkWidget *label10;
	GtkWidget *label11;
	GtkWidget *dialog_action_area1;
	GtkWidget *button1;

	about_cx = gtk_dialog_new ();
	gtk_object_set_data (GTK_OBJECT (about_cx), "about_cx", about_cx);
	gtk_window_set_title (GTK_WINDOW (about_cx), ("About gcx"));
	GTK_WINDOW (about_cx)->type = GTK_WINDOW_TOPLEVEL;
	gtk_window_set_position (GTK_WINDOW (about_cx), GTK_WIN_POS_CENTER);
	gtk_window_set_policy (GTK_WINDOW (about_cx), TRUE, TRUE, FALSE);

	dialog_vbox1 = GTK_DIALOG (about_cx)->vbox;
	gtk_object_set_data (GTK_OBJECT (about_cx), "dialog_vbox1", dialog_vbox1);
	gtk_widget_show (dialog_vbox1);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_ref (vbox1);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "vbox1", vbox1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);

	frame1 = gtk_frame_new ("gcx version " VERSION);
	gtk_widget_ref (frame1);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "frame1", frame1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (frame1);
	gtk_box_pack_start (GTK_BOX (vbox1), frame1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (frame1), 5);

	vbox2 = gtk_vbox_new (TRUE, 0);
	gtk_widget_ref (vbox2);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "vbox2", vbox2,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame1), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 5);

	label3 = gtk_label_new (("A program that controls astronomical"));
	gtk_widget_ref (label3);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "label3", label3,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label3);
	gtk_box_pack_start (GTK_BOX (vbox2), label3, FALSE, FALSE, 0);

	label4 = gtk_label_new (("CCD cameras and does things on images."));
	gtk_widget_ref (label4);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "label4", label4,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label4);
	gtk_box_pack_start (GTK_BOX (vbox2), label4, FALSE, FALSE, 0);

	label5 = gtk_label_new (("(c) 2002, 2003 Radu Corlan"));
	gtk_widget_ref (label5);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "label5", label5,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label5);
	gtk_box_pack_start (GTK_BOX (vbox2), label5, FALSE, FALSE, 0);

	label6 = gtk_label_new (("Tycho2 routines (c) 2002, 2003 Alexandru Dan Corlan"));
	gtk_widget_ref (label6);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "label6", label6,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label6);
	gtk_box_pack_start (GTK_BOX (vbox2), label6, FALSE, FALSE, 0);

	label12 = gtk_label_new (("WCS conversion routines from Classic AIPS"));
	gtk_widget_ref (label12);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "label12", label12,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label12);
	gtk_box_pack_start (GTK_BOX (vbox2), label12, FALSE, FALSE, 0);

	label7 = gtk_label_new (("Sidereal time routines from libnova (c) 2000 Liam Girdwood"));
	gtk_widget_ref (label7);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "label7", label7,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label7);
	gtk_box_pack_start (GTK_BOX (vbox2), label7, FALSE, FALSE, 0);

	hseparator1 = gtk_hseparator_new ();
	gtk_widget_ref (hseparator1);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "hseparator1", hseparator1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (hseparator1);
	gtk_box_pack_start (GTK_BOX (vbox2), hseparator1, FALSE, FALSE, 0);

	label8 = gtk_label_new (("gcx comes with ABSOLUTELY NO WARANTY"));
	gtk_widget_ref (label8);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "label8", label8,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label8);
	gtk_box_pack_start (GTK_BOX (vbox2), label8, FALSE, FALSE, 0);

	label9 = gtk_label_new (("This is free software, distributed under"));
	gtk_widget_ref (label9);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "label9", label9,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label9);
	gtk_box_pack_start (GTK_BOX (vbox2), label9, FALSE, FALSE, 0);

	label10 = gtk_label_new (("the GNU General Public License v2 or later;"));
	gtk_widget_ref (label10);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "label10", label10,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label10);
	gtk_box_pack_start (GTK_BOX (vbox2), label10, FALSE, FALSE, 0);

	label11 = gtk_label_new (("See file 'COPYING' for details."));
	gtk_widget_ref (label11);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "label11", label11,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label11);
	gtk_box_pack_start (GTK_BOX (vbox2), label11, FALSE, FALSE, 0);

	dialog_action_area1 = GTK_DIALOG (about_cx)->action_area;
	gtk_object_set_data (GTK_OBJECT (about_cx), "dialog_action_area1", dialog_action_area1);
	gtk_widget_show (dialog_action_area1);
	gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);

	button1 = gtk_button_new_with_label("Close");
	gtk_widget_ref (button1);
	gtk_object_set_data_full (GTK_OBJECT (about_cx), "button1", button1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (button1);
	gtk_box_pack_start (GTK_BOX (dialog_action_area1), button1, FALSE, FALSE, 0);

	gtk_signal_connect (GTK_OBJECT (button1), "clicked",
			    GTK_SIGNAL_FUNC (close_about_cx),
			    about_cx);

	return about_cx;
}

