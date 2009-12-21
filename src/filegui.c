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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <glob.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "sourcesdraw.h"
#include "params.h"
#include "recipy.h"
#include "filegui.h"
#include "symbols.h"

#define FILE_FILTER_SZ 255

struct file_action {
	GtkWidget *filesel;
	GtkWidget *window;
	void *data;
	int arg1;
	char file_filter[FILE_FILTER_SZ+1];
};

gint destroy_filesel(gpointer data)
{
	gtk_object_set_data(GTK_OBJECT(data), "file_selector", NULL);
	return TRUE;
}

/*
static void filter_files (GtkCList *clist,
			  gint row,
			  gint column,
			  GdkEventButton *event,
			  gpointer user_data)
{
	struct file_action *fa = user_data;
	d3_printf("filter files\n");
	if (fa->file_filter[0] != 0)
		gtk_file_selection_complete(GTK_FILE_SELECTION(fa->filesel), fa->file_filter);
}
*/

GtkWidget * create_file_selection(char *title, GtkSignalFunc file_action,
				  struct file_action *action) 
{
	GtkWidget *file_selector;
	file_selector = gtk_file_selection_new(title);
	gtk_window_set_position (GTK_WINDOW (file_selector), GTK_WIN_POS_CENTER);

	gtk_clist_set_selection_mode (GTK_CLIST (GTK_FILE_SELECTION(file_selector)->file_list), 
				     GTK_SELECTION_EXTENDED);
   
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->ok_button),
			    "clicked", GTK_SIGNAL_FUNC (file_action), action);

//	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->dir_list),
//			    "select-row", GTK_SIGNAL_FUNC (filter_files), action);

	gtk_signal_connect_object (GTK_OBJECT (file_selector), "delete_event",
                        GTK_SIGNAL_FUNC (destroy_filesel), (gpointer) action->window);
   			   
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->ok_button),
				   "clicked", GTK_SIGNAL_FUNC (destroy_filesel),
				   (gpointer) action->window);

	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC (destroy_filesel),
				   (gpointer) action->window);
      
	gtk_object_set_data_full(GTK_OBJECT(file_selector), "file_action", action, 
			    GTK_SIGNAL_FUNC (free));

	gtk_object_set_data_full(GTK_OBJECT(action->window), "file_selector", file_selector, 
			    GTK_SIGNAL_FUNC (gtk_widget_destroy));

	gtk_widget_show (file_selector);
	return file_selector;
}

/* return a malloced string that contains the supplied 
 * filename with ".ext" added (if it didn;t already end in '.ext')
 */
char *add_extension(char *fn, char *ext)
{
	int ns, i;
	char *fne;
	i = strlen(fn);
	ns = strlen(ext) + i + 1;

	fne = calloc(1, ns+1);
	if (fne == NULL) {
		err_printf("add_extension: alloc error\n");
		return NULL;
	}
	while (i > 0) {
		if (fn[i] == '.' || fn[i] == '/')
			break;
		i--;
	}
	if (fn[i] == '.' && !strcasecmp(fn+i+1, ext)) {
		strcpy(fne, fn);
		return fne;
	}
	strcpy(fne, fn);
	i = strlen(fn);
	fne[i] = '.';
	strcpy(fne+i+1, ext);
	return fne;
}


/* return a malloced string that contains the supplied 
 * filename with the extention (if any) replaced by ext
 */
char *force_extension(char *fn, char *ext)
{
	int ns, i;
	char *fne;
	i = strlen(fn);
	ns = strlen(ext) + i + 5;

	fne = calloc(1, ns+1);
	if (fne == NULL) {
		err_printf("force_extension: alloc error\n");
		return NULL;
	}
	while (i > 0) {
		if (fn[i] == '.' || fn[i] == '/')
			break;
		i--;
	}
	if (fn[i] == '.') {
		memcpy(fne, fn, i+1);
		strcpy(fne+i+1, ext);
		return fne;
	}
	strcpy(fne, fn);
	i = strlen(fn);
	fne[i] = '.';
	strcpy(fne+i+1, ext);
	return fne;
}

/* set the default paths for the file dialogs */
/* several classes of files exist; when one is set */
/* all the uninitialised classes are also set to that */
/* path should be a malloced string (to which set_last_open */
/* will hold a reference */

static void set_last_open(gpointer object, char *file_class, char *path)
{
	char *op;
	gtk_object_set_data_full(GTK_OBJECT(object), file_class, path, 
			    GTK_SIGNAL_FUNC (free));
	op = gtk_object_get_data(GTK_OBJECT(object), "last_open_fits");
	if (op == NULL) {
		gtk_object_set_data_full(GTK_OBJECT(object), "last_open_fits", strdup(path), 
					 GTK_SIGNAL_FUNC (free));
	}
	op = gtk_object_get_data(GTK_OBJECT(object), "last_open_etc");
	if (op == NULL) {
		gtk_object_set_data_full(GTK_OBJECT(object), "last_open_etc", strdup(path), 
					 GTK_SIGNAL_FUNC (free));
	}
	op = gtk_object_get_data(GTK_OBJECT(object), "last_open_rcp");
	if (op == NULL) {
		gtk_object_set_data_full(GTK_OBJECT(object), "last_open_rcp", strdup(path), 
					 GTK_SIGNAL_FUNC (free));
	}	
	op = gtk_object_get_data(GTK_OBJECT(object), "last_open_mband");
	if (op == NULL) {
		gtk_object_set_data_full(GTK_OBJECT(object), "last_open_mband", strdup(path), 
					 GTK_SIGNAL_FUNC (free));
	}	
}


/* file action functions */

/* create a pnm file from the channel-mapped image 
 * if fa->arg1 = 1, we create a 16-bit pnm file (still TBD)
 */
static void export_pnm(GtkFileSelection *selector, gpointer user_data) 
{
	char *fn, *fne;
	struct file_action *fa = user_data;
	fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(fa->filesel));
	fne = add_extension(fn, "pnm");
	d3_printf("Saving pnm file: %s\n", fne);
	channel_to_pnm_file(fa->data, fa->window, fne);
	free(fne);
}


/* 
 * save to a fits file
 */
static void save_fits(GtkFileSelection *selector, gpointer user_data) 
{
	char *fn;
	struct file_action *fa = user_data;
	struct image_channel *ich;
	int i;

	fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(fa->filesel));
	d3_printf("Saving fits file: %s\n", fn);
	ich = fa->data;
	if (file_is_zipped(fn)) {
		for (i = strlen(fn); i > 0; i--)
			if (fn[i] == '.') {
				fn[i] = 0;
				break;
			}		
		write_gz_fits_frame(ich->fr, fn, P_STR(FILE_COMPRESS));
	} else {
		write_fits_frame(ich->fr, fn);
	}
}


/* open a fits file and display it */
static void open_fits(GtkFileSelection *selector, gpointer user_data) 
{
	char *fn, *fnd;
	struct ccd_frame *fr;
	struct file_action *fa = user_data;

	fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(fa->filesel));
	d3_printf("opening fits file: %s\n", fn);
	fr = read_gz_fits_file(fn, P_STR(FILE_UNCOMPRESS), P_INT(FILE_UNSIGNED_FITS));
	if (fr == NULL) {
		statusbar2_message(fa->window, " Error trying to open fits file", "file", 5000);
		return;
	}
	rescan_fits_wcs(fr, &fr->fim);
	rescan_fits_exp(fr, &fr->exp);
	frame_to_channel(fr, fa->window, "i_channel");
	release_frame(fr);
	set_last_open(fa->window, "last_open_fits", fnd = strdup(fn));
	d3_printf("lastopen_fits set to %s\n", fnd);
}

/* load stars from a rcp file into the given windows's gsl */
int load_rcp_to_window(gpointer window, char *name)
{
	struct stf *stf;
	GList *rsl;
	int p=0;
	FILE *rfn;
	char *cmd;
	struct wcs *wcs;
	char *text;
	double v;

	if (name == NULL)
		return -1;
//	d3_printf("read_rcp: looking for %s\n", name);

	wcs = gtk_object_get_data(GTK_OBJECT(window), "wcs_of_window");

	if (file_is_zipped(name)) {
		asprintf(&cmd, "%s %s ", P_STR(FILE_UNCOMPRESS), name);
		rfn = popen(cmd, "r");
		free(cmd);
		p=1;
	} else {
		rfn = fopen(name, "r");
	}
	if (rfn == NULL) {
		err_printf("read_rcp: cannot open file %s\n", name);
		return -1;
	}
	stf = stf_read_frame(rfn);

	if (p) {
		pclose(rfn);
	} else {
		fclose(rfn);
	}

	if (stf == NULL)
		return -1;

	if (wcs != NULL && wcs->wcsset < WCS_INITIAL) {
		d3_printf("try wcs from rcp\n");
		if (!stf_find_double(stf, &v, 1, SYM_RECIPY, SYM_EQUINOX))
			wcs->equinox = 2000.0;
		else
			wcs->equinox = v;
		text = stf_find_string(stf, 1, SYM_RECIPY, SYM_RA);
		if (text != NULL && !dms_to_degrees(text, &v)) {
			wcs->xref = 15.0 * v;
			d3_printf("got ra\n");
			text = stf_find_string(stf, 1, SYM_RECIPY, SYM_DEC);
			if (text != NULL && !dms_to_degrees(text, &v)) {
				d3_printf("got dec\n");
				wcs->yref = v;
				wcs->xinc = - P_DBL(WCS_SEC_PER_PIX) / 3600.0;
				wcs->yinc = - P_DBL(WCS_SEC_PER_PIX) / 3600.0;
				if (P_INT(OBS_FLIPPED))
					wcs->yinc = -wcs->yinc;
				wcs->wcsset = WCS_INITIAL;
			}
		}
	} 

	rsl = stf_find_glist(stf, 0, SYM_STARS);
	if (rsl == NULL)
		return -1;

	gtk_object_set_data_full(GTK_OBJECT(window), "recipy", stf, 
				 (GtkDestroyNotify)stf_free_all);
	merge_cat_star_list_to_window(window, rsl);
	return g_list_length(rsl);
}

#define MAX_GSC2_STARS 100000
/* load stars from a rcp file into the given windows's gsl */
int load_gsc2_to_window(gpointer window, char *name)
{
	struct cat_star *csl[MAX_GSC2_STARS];
	int ret, p=0;
	FILE *rfn;
	char *cmd;

	if (name == NULL)
		return -1;
	d3_printf("read_gsc2: looking for %s\n", name);

	if (file_is_zipped(name)) {
		asprintf(&cmd, "%s %s ", P_STR(FILE_UNCOMPRESS), name);
		rfn = popen(cmd, "r");
		free(cmd);
		p=1;
	} else {
		rfn = fopen(name, "r");
	}
	if (rfn == NULL) {
		err_printf("read_gsc2: cannot open file %s\n", name);
		return -1;
	}

	ret = read_gsc2(csl, rfn, MAX_GSC2_STARS, NULL, NULL);

	d3_printf("read_gsc2: got %d\n", ret);

	if (p) {
		pclose(rfn);
	} else {
		fclose(rfn);
	}

	if (ret >= 0) {
//		remove_stars(window, TYPE_MASK_PHOT, 0);
		add_cat_stars_to_window(window, csl, ret);
	}
	return ret;
}


/* open a rcp file and load stars; */
static void open_rcp(GtkFileSelection *selector, gpointer user_data) 
{
	char *fn, *fnd;
	struct file_action *fa = user_data;

	fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(fa->filesel));
	g_return_if_fail(fn != NULL);

	load_rcp_to_window(fa->window, fn);

	set_last_open(fa->window, "last_open_rcp", fnd = strdup(fn));
	d3_printf("lastopen_rcp set to %s\n", fnd);
	gtk_widget_queue_draw(fa->window);
}

/* open a report file */
static void open_mband(GtkFileSelection *selector, gpointer user_data) 
{
	char *fn, *fnd;
	struct file_action *fa = user_data;
	gpointer mwin;

	fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(fa->filesel));
	g_return_if_fail(fn != NULL);

	add_to_mband(fa->window, fn);

	mwin = gtk_object_get_data(GTK_OBJECT(fa->window), "im_window");
	if (mwin != NULL)
		set_last_open(mwin, "last_open_mband", fnd = strdup(fn));
	else 
		set_last_open(fa->window, "last_open_mband", fnd = strdup(fn));
	d3_printf("lastopen_mband set to %s\n", fnd);
	gtk_widget_queue_draw(fa->window);
}


/* open a gsc2 file and load stars; star that have v photometry are
 * marked as object, while the other are made field stars */
static void open_gsc2(GtkFileSelection *selector, gpointer user_data) 
{
	char *fn, *fnd;
	struct file_action *fa = user_data;

	fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(fa->filesel));
	g_return_if_fail(fn != NULL);

	load_gsc2_to_window(fa->window, fn);

	set_last_open(fa->window, "last_open_etc", fnd = strdup(fn));
	d3_printf("lastopen_etc set to %s\n", fnd);
	gtk_widget_queue_draw(fa->window);
}


/* set the entry with the file name */
static void set_entry(GtkFileSelection *selector, gpointer user_data) 
{
	char *fn, *fnd;
	struct file_action *fa = user_data;

	fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(fa->filesel));
	g_return_if_fail(fn != NULL);

	gtk_entry_set_text(GTK_ENTRY(fa->data), fn);
	if (fa->arg1)
		gtk_signal_emit_by_name(GTK_OBJECT(fa->data), "activate", fa->data);

	set_last_open(fa->window, "last_open_fits", fnd = strdup(fn));
	d3_printf("lastopen_fits set to %s\n", fnd);
}


/* get the file list and call the callback in fa->data */
static void get_list(GtkFileSelection *selector, gpointer user_data) 
{
	GtkWidget *clist;
	GList *sl=NULL;
	GList *fl=NULL;
	char *text;
	char *fn, *fo, *fnd;
	char *dn;
	struct file_action *fa = user_data;
	get_file_list_type cb;

	fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(fa->filesel));
	g_return_if_fail(fn != NULL);

	fnd = strdup(fn);
	dn = dirname(fn);

	clist = GTK_FILE_SELECTION(fa->filesel)->file_list;
	sl = GTK_CLIST(clist) -> selection;
	while(sl != NULL) {
		if (gtk_clist_get_text(GTK_CLIST(clist), (gint)sl->data, 0, &text)) {
			if ((asprintf(&fo, "%s/%s", dn, text)) > 0)
				fl = g_list_append(fl, fo);
		}
		sl = g_list_next(sl);
	}
	cb = fa->data;
	if (cb)
		(* cb)(fl, fa->window);
	if (fl != NULL) {
		fnd = strdup(fl->data);
		set_last_open(fa->window, "last_open_fits", fnd);
		d3_printf("lastopen_fits set to %s\n", fnd);
	} 
	sl = fl; 		/* we reuse sl here */
	while(sl != NULL) {
		free(sl->data);
		sl = g_list_next(sl);
	}
	g_list_free(fl);

}

/* get the file name and call the callback in fa->data */
static void get_file(GtkFileSelection *selector, gpointer user_data) 
{
	char *fn, *fnd;
	struct file_action *fa = user_data;
	get_file_type cb;
	gpointer mwin;

	fn = gtk_file_selection_get_filename (GTK_FILE_SELECTION(fa->filesel));
	g_return_if_fail(fn != NULL);

	fnd = strdup(fn);
	cb = fa->data;
	if (cb)
		(* cb)(fn, fa->window, fa->arg1);

	mwin = gtk_object_get_data(GTK_OBJECT(fa->window), "im_window");
	if (mwin != NULL)
		set_last_open(mwin, "last_open_etc", fnd = strdup(fn));
	else 
		set_last_open(fa->window, "last_open_etc", fnd = strdup(fn));
	d3_printf("lastopen_etc set to %s\n", fnd);
}




/* entry points */

/* make the file selector set the text in entry */
/* if activate is true, the activate signal is emitted for the entry
 * when the user presses ok in the file dialog */
void file_select_to_entry(gpointer data, GtkWidget *entry, char *title, char *filter,
			  int activate)
{
	GtkWidget *window = data;
	GtkWidget *filesel;
	struct file_action *fa;

	char *lastopen;
	lastopen = gtk_object_get_data(GTK_OBJECT(window), "last_open_fits");
	fa = calloc(1, sizeof(struct file_action));
	fa->window = window;
	fa->data = entry;
	fa->arg1 = activate;

	if (title == NULL)
		filesel = create_file_selection("Select file", set_entry, fa);
	else
		filesel = create_file_selection(title, set_entry, fa);

	strncpy(fa->file_filter, filter, FILE_FILTER_SZ);
	fa->filesel = filesel;
	if (lastopen != NULL) {
		gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), 
						 lastopen);
	}
}

/* run the file selector configured to call cb on ok with the 
 * selected file; arg is passed on to the called function 
 * the file selector is run in a modal window */
void file_select(gpointer data, char *title, char *filter,
		 get_file_type cb, unsigned arg)
{
	GtkWidget *window = data;
	GtkWidget *filesel;
	struct file_action *fa;

	char *lastopen;
	lastopen = gtk_object_get_data(GTK_OBJECT(window), "last_open_etc");
	fa = calloc(1, sizeof(struct file_action));
	fa->window = window;
	fa->data = cb;
	fa->arg1 = arg;

	if (title == NULL)
		filesel = create_file_selection("Select file", get_file, fa);
	else
		filesel = create_file_selection(title, get_file, fa);

	if (filter != NULL)
		strncpy(fa->file_filter, filter, FILE_FILTER_SZ);
	else
		fa->file_filter[0] = 0;
	fa->filesel = filesel;
	if (lastopen != NULL) {
		gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), 
						 lastopen);
	}
}


/* run the file selector configured to call cb on ok with a list of strings
 * selected files; the strings should _not_ be changed, and are not expected
 * to last after cb returns */
void file_select_list(gpointer data, char *title, char *filter,
		      get_file_list_type cb)
{
	GtkWidget *window = data;
	GtkWidget *filesel;
	struct file_action *fa;

	char *lastopen;
	lastopen = gtk_object_get_data(GTK_OBJECT(window), "last_open_fits");
	fa = calloc(1, sizeof(struct file_action));
	fa->window = window;
	fa->data = cb;

	if (title == NULL)
		filesel = create_file_selection("Select file", get_list, fa);
	else
		filesel = create_file_selection(title, get_list, fa);

	strncpy(fa->file_filter, filter, FILE_FILTER_SZ);
	fa->filesel = filesel;
	if (lastopen != NULL) {
		gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), 
						 lastopen);
	}
}


void file_popup_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *window = data;
	GtkWidget *filesel;
	struct image_channel *channel;
	struct file_action *fa;
	char *fn;
	char *lastopen;
	switch(action) {
	case FILE_OPEN:
		lastopen = gtk_object_get_data(GTK_OBJECT(window), "last_open_fits");
		fa = calloc(1, sizeof(struct file_action));
		fa->window = window;
		filesel = create_file_selection("Select fits file to open", open_fits, fa);
		fa->filesel = filesel;
		strncpy(fa->file_filter, "*.fits*", FILE_FILTER_SZ);
		if (lastopen != NULL) {
			gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), 
							 lastopen);
		}
		break;
	case FILE_OPEN_RCP:
		lastopen = gtk_object_get_data(GTK_OBJECT(window), "last_open_rcp");
		fa = calloc(1, sizeof(struct file_action));
		fa->window = window;
		filesel = create_file_selection("Select fits file to open", open_rcp, fa);
		fa->filesel = filesel;
		if (lastopen != NULL) {
			gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), 
							 lastopen);
		}
//		gtk_file_selection_complete(GTK_FILE_SELECTION(filesel), "*.fits*");
		break;
	case FILE_ADD_TO_MBAND:
		lastopen = gtk_object_get_data(GTK_OBJECT(window), "last_open_mband");
		fa = calloc(1, sizeof(struct file_action));
		fa->window = window;
		filesel = create_file_selection("Select report file to open", open_mband, fa);
		fa->filesel = filesel;
		if (lastopen != NULL) {
			gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), 
							 lastopen);
		}
//		gtk_file_selection_complete(GTK_FILE_SELECTION(filesel), "*.fits*");
		break;
	case FILE_LOAD_GSC2:
		lastopen = gtk_object_get_data(GTK_OBJECT(window), "last_open_etc");
		fa = calloc(1, sizeof(struct file_action));
		fa->window = window;
		filesel = create_file_selection("Select GSC-2 data file to open", open_gsc2, fa);
		fa->filesel = filesel;
		if (lastopen != NULL) {
			gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), 
							 lastopen);
		}
//		gtk_file_selection_complete(GTK_FILE_SELECTION(filesel), "*.fits*");
		break;
	case FILE_EXPORT_PNM8:
	case FILE_EXPORT_PNM16:
		channel = gtk_object_get_data(GTK_OBJECT(window), "i_channel");
		if (channel == NULL || channel->fr == NULL) {
			error_message_sb2(window, "No frame loaded");
			return ;
		}
		fn = force_extension(channel->fr->name, "pnm");
		fa = calloc(1, sizeof(struct file_action));
		fa->window = window;
		fa->data = channel;
//		fa->arg1 = (action == FILE_EXPORT_PNM16);
		filesel = create_file_selection("Select pnm file name", export_pnm, fa);
		fa->filesel = filesel;
		gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), fn);
		free(fn);
		break;
	case FILE_SAVE_AS:
		channel = gtk_object_get_data(GTK_OBJECT(window), "i_channel");
		if (channel == NULL || channel->fr == NULL) {
			error_message_sb2(window, "No frame");
			return ;
		}
		wcs_to_fits_header(channel->fr);
		fn = channel->fr->name;
		fa = calloc(1, sizeof(struct file_action));
		fa->window = window;
		fa->data = channel;
		filesel = create_file_selection("Select output file name", save_fits, fa);
		fa->filesel = filesel;
		gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), fn);
		break;
	default:
		err_printf("unknown action %d in file_popup_cb\n", action);
	}

}

/* return the first filename matching pattern that is found in path,
   or null if it couldn't be found */
char *find_file_in_path(char *pattern, char *path)
{
	char *dir;
	char *fn = NULL;
	char buf[1024];
	char pathc[1024];
	glob_t gl;
	int ret;

	strncpy(pathc, path, 1023);
	pathc[1023] = 0;
	dir = strtok(pathc, ":");
	while (dir != NULL) {
		snprintf(buf, 1024, "%s/%s", dir, pattern);
		d3_printf("looking for %s\n", buf);
		gl.gl_offs = 0;
		gl.gl_pathv = NULL;
		gl.gl_pathc = 0;
		ret = glob(buf, GLOB_TILDE, NULL, &gl);
//		d3_printf("glob returns %d\n", ret);
		if (ret == 0) {
			fn = strdup(gl.gl_pathv[0]);
//			d3_printf("found: %s\n", fn);
			globfree(&gl);
			return fn;
		}
		globfree(&gl);
		dir = strtok(NULL, ":");	
	}
	return NULL;
}

/* check if the file is zipped (ends with .gz, .z or .zip);
 * return 1 if it does */
int file_is_zipped(char *fn)
{
	int len;
	len = strlen(fn);
	if ((len > 4) && (strcasecmp(fn + len - 3, ".gz") == 0)) {
		return 1;
	}
	if ((len > 3) && (strcasecmp(fn + len - 2, ".z") == 0)) {
		return 1;
	}
	if ((len > 5) && (strcasecmp(fn + len - 4, ".zip") == 0)) {
		return 1;
	}
	return 0;
}

/* open the first file in the expanded path */
FILE * open_expanded(char *path, char *mode)
{
	glob_t gl;
	int ret;
	FILE *fp = NULL;

	gl.gl_offs = 0;
	gl.gl_pathv = NULL;
	gl.gl_pathc = 0;
	ret = glob(path, GLOB_TILDE, NULL, &gl);
	if (ret == 0) {
		fp = fopen(gl.gl_pathv[0], mode);
	}
	globfree(&gl);
	return fp;
}
