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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "interface.h"
#include "params.h"
#include "misc.h"
#include "reduce.h"
#include "filegui.h"
#include "sourcesdraw.h"

static void imf_display_cb(GtkWidget *wid, gpointer dialog);
static void imf_next_cb(GtkWidget *wid, gpointer dialog);
static void imf_skip_cb(GtkWidget *wid, gpointer dialog);
static void imf_rm_cb(GtkWidget *wid, gpointer dialog);
static void imf_unskip_cb(GtkWidget *wid, gpointer dialog);
static void imf_selall_cb(GtkWidget *wid, gpointer dialog);
static void imf_add_cb(GtkWidget *wid, gpointer dialog);
static void imf_reload_cb(GtkWidget *wid, gpointer dialog);
static void select_cb(GtkList *list, GtkWidget *wid, gpointer dialog);
//static void list_button_cb(GtkWidget *wid, GdkEventButton *event, gpointer dialog);
static void imf_update_status_label(gpointer dialog, struct image_file *imf);
static void imf_red_browse_cb(GtkWidget *wid, gpointer dialog);
static void ccdred_run_cb(GtkWidget *wid, gpointer dialog);
static void show_align_cb(GtkWidget *wid, gpointer dialog);
static void update_selected_status_label(gpointer dialog);
static void imf_red_activate_cb(GtkWidget *wid, gpointer dialog);
static void set_processing_dialog_ccdr(GtkWidget *dialog, struct ccd_reduce *ccdr);


static gboolean close_processing_window( GtkWidget *widget, gpointer data )
{
	g_return_val_if_fail(data != NULL, TRUE);
//	gtk_object_set_data(GTK_OBJECT(data), "processing", NULL);
	gtk_widget_hide(widget);
	return TRUE;
}
/* create an image processing dialog and set the callbacks, but don't 
 * show it */
static GtkWidget *make_image_processing(gpointer window)
{
	GtkWidget *dialog;

	dialog = create_image_processing();
	gtk_object_set_data(GTK_OBJECT(dialog), "im_window", window);
	gtk_object_set_data_full(GTK_OBJECT(window), "processing",
				 dialog, (GtkDestroyNotify)(gtk_widget_destroy));
	gtk_signal_connect (GTK_OBJECT (dialog), "delete_event",
			    GTK_SIGNAL_FUNC (close_processing_window), window);
	set_named_callback (GTK_OBJECT (dialog), "imf_display_button", "clicked",
			    GTK_SIGNAL_FUNC (imf_display_cb));
	set_named_callback (GTK_OBJECT (dialog), "imf_next_button", "clicked",
			    GTK_SIGNAL_FUNC (imf_next_cb));
	set_named_callback (GTK_OBJECT (dialog), "imf_skip_button", "clicked",
			    GTK_SIGNAL_FUNC (imf_skip_cb));
	set_named_callback (GTK_OBJECT (dialog), "imf_unskip_button", "clicked",
			    GTK_SIGNAL_FUNC (imf_unskip_cb));
	set_named_callback (GTK_OBJECT (dialog), "imf_rm_button", "clicked",
			    GTK_SIGNAL_FUNC (imf_rm_cb));
	set_named_callback (GTK_OBJECT (dialog), "imf_select_all", "clicked",
			    GTK_SIGNAL_FUNC (imf_selall_cb));
	set_named_callback (GTK_OBJECT (dialog), "imf_add_button", "clicked",
			    GTK_SIGNAL_FUNC (imf_add_cb));
	set_named_callback (GTK_OBJECT (dialog), "imf_reload_button", "clicked",
			    GTK_SIGNAL_FUNC (imf_reload_cb));
	set_named_callback (GTK_OBJECT (dialog), "image_file_list", "select-child",
			    GTK_SIGNAL_FUNC (select_cb));
//	set_named_callback (GTK_OBJECT (dialog), "image_file_list", "button-press-event",
//			    GTK_SIGNAL_FUNC (list_button_cb));
	set_named_callback (GTK_OBJECT (dialog), "bias_browse", "clicked",
			    GTK_SIGNAL_FUNC (imf_red_browse_cb));
	set_named_callback (GTK_OBJECT (dialog), "dark_browse", "clicked",
			    GTK_SIGNAL_FUNC (imf_red_browse_cb));
	set_named_callback (GTK_OBJECT (dialog), "flat_browse", "clicked",
			    GTK_SIGNAL_FUNC (imf_red_browse_cb));
	set_named_callback (GTK_OBJECT (dialog), "align_browse", "clicked",
			    GTK_SIGNAL_FUNC (imf_red_browse_cb));
	set_named_callback (GTK_OBJECT (dialog), "output_file_browse", "clicked",
			    GTK_SIGNAL_FUNC (imf_red_browse_cb));
	set_named_callback (GTK_OBJECT (dialog), "run_button", "clicked",
			    GTK_SIGNAL_FUNC (ccdred_run_cb));
	set_named_callback (GTK_OBJECT (dialog), "show_align_button", "clicked",
			    GTK_SIGNAL_FUNC (show_align_cb));
	set_named_callback (GTK_OBJECT (dialog), "bias_entry", "activate",
			    GTK_SIGNAL_FUNC (imf_red_activate_cb));
	set_named_callback (GTK_OBJECT (dialog), "dark_entry", "activate",
			    GTK_SIGNAL_FUNC (imf_red_activate_cb));
	set_named_callback (GTK_OBJECT (dialog), "flat_entry", "activate",
			    GTK_SIGNAL_FUNC (imf_red_activate_cb));
	set_named_callback (GTK_OBJECT (dialog), "align_entry", "activate",
			    GTK_SIGNAL_FUNC (imf_red_activate_cb));
	set_processing_dialog_ccdr(dialog, NULL);
	return dialog;
}

/* show the current frame's fits header in a text window */
void processing_cb(gpointer window, guint action, GtkWidget *menu_item)
{
	GtkWidget *dialog;

	dialog = gtk_object_get_data(GTK_OBJECT(window), "processing");
	if (dialog == NULL) {
		dialog = make_image_processing(window);
		gtk_widget_show(dialog);
	} else {
		gtk_widget_show(dialog);
		gdk_window_raise(dialog->window);
	}
}

static void stack_method_activate(GtkWidget *wid, gpointer data)
{
	P_INT(CCDRED_STACK_METHOD) = (int)data;
	par_touch(CCDRED_STACK_METHOD);
}
 
/* update the dialog to match the supplied ccdr */
/* in ccdr is null, just update the settings from the pars */
static void set_processing_dialog_ccdr(GtkWidget *dialog, struct ccd_reduce *ccdr)
{
	GtkWidget *menu, *omenu;
	GtkWidget *menuitem;
	char **c;
	int i;

	omenu = gtk_object_get_data(GTK_OBJECT(dialog), "stack_method_optmenu");
	g_return_if_fail(omenu != NULL);
	menu = gtk_menu_new();
	g_return_if_fail(menu != NULL);
	
	c = stack_methods;
	i = 0;
	while (*c != NULL) {
		menuitem = gtk_menu_item_new_with_label (*c);
		gtk_widget_show (menuitem);
		gtk_menu_append (GTK_MENU (menu), menuitem);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (stack_method_activate), (gpointer)i);
		d3_printf("add %s to stak method menu\n", *c);
		c++;
		i++;
	}
	gtk_widget_show(menu);
	gtk_widget_show(omenu);
	gtk_option_menu_remove_menu(GTK_OPTION_MENU (omenu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU (omenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), 
				     P_INT(CCDRED_STACK_METHOD));
	named_spin_set(dialog, "stack_sigmas_spin", P_DBL(CCDRED_SIGMAS));
	named_spin_set(dialog, "stack_iter_spin", 1.0 * P_INT(CCDRED_ITER));

	if (ccdr == NULL)
		return;
	if ((ccdr->bias) && (ccdr->ops & IMG_OP_BIAS)) {
		named_entry_set(dialog, "bias_entry", ccdr->bias->filename);
		set_named_checkb_val(dialog, "bias_checkb", 1);
	}
	if ((ccdr->dark) && (ccdr->ops & IMG_OP_DARK)) {
		named_entry_set(dialog, "dark_entry", ccdr->dark->filename);
		set_named_checkb_val(dialog, "dark_checkb", 1);
	}
	if ((ccdr->flat) && (ccdr->ops & IMG_OP_FLAT)) {
		named_entry_set(dialog, "flat_entry", ccdr->flat->filename);
		set_named_checkb_val(dialog, "flat_checkb", 1);
	}
	if ((ccdr->alignref) && (ccdr->ops & IMG_OP_ALIGN)) {
		named_entry_set(dialog, "align_entry", ccdr->alignref->filename);
		set_named_checkb_val(dialog, "align_checkb", 1);
	}
	if ((ccdr->ops & IMG_OP_BLUR)) {
		named_spin_set(dialog, "blur_spin", ccdr->blurv);
		set_named_checkb_val(dialog, "blur_checkb", 1);
	}
	if ((ccdr->ops & IMG_OP_ADD)) {
		named_spin_set(dialog, "add_spin", ccdr->addv);
		set_named_checkb_val(dialog, "add_checkb", 1);
	}
	if ((ccdr->ops & IMG_OP_MUL)) {
		named_spin_set(dialog, "mul_spin", ccdr->addv);
		set_named_checkb_val(dialog, "mul_checkb", 1);
	}
	if ((ccdr->ops & IMG_OP_STACK)) {
		if ((ccdr->ops & IMG_OP_BG_ALIGN_MUL)) {
			set_named_checkb_val(dialog, "bg_match_mul_rb", 1);
		} else {
			set_named_checkb_val(dialog, "bg_match_add_rb", 1);
		} 
		set_named_checkb_val(dialog, "stack_checkb", 1);
	}
	if ((ccdr->ops & IMG_OP_INPLACE)) {
		set_named_checkb_val(dialog, "overwrite_checkb", 1);
	}
}

/* replace the file list in the dialog with the supplied one */
static void set_processing_dialog_imfl(GtkWidget *dialog, struct image_file_list *imfl)
{
	GtkList *list;
	GtkWidget *item;
	GList *il = NULL;
	GList *gl;
	struct image_file *imf;
	char label[256];

	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);
	gtk_list_clear_items(list, -1, -1);

	gl = imfl->imlist;
	while (gl != NULL) {
		imf = gl->data;
		gl = g_list_next(gl);

		if (imf->flags & IMG_SKIP) {
			snprintf(label, 255, "[ %s ]", imf->filename);
			item=gtk_list_item_new_with_label(label);
		} else {
			item=gtk_list_item_new_with_label(imf->filename);
		}
		image_file_ref(imf);
		gtk_object_set_data_full(GTK_OBJECT(item), "imf",
					 imf, (GtkDestroyNotify)(image_file_release));

		il=g_list_append(il, item);
		gtk_widget_show(item);

		if (imf->flags & IMG_SKIP)
			continue;
	}
	gtk_list_append_items(list, il);
}

void set_imfl_ccdr(gpointer window, struct ccd_reduce *ccdr, 
		   struct image_file_list *imfl)
{
	GtkWidget *dialog;
	dialog = gtk_object_get_data(GTK_OBJECT(window), "processing");
	if (dialog == NULL) {
		dialog = make_image_processing(window);
	}
	g_return_if_fail(dialog != NULL);

	if (imfl) {
		image_file_list_ref(imfl);
		gtk_object_set_data_full(GTK_OBJECT(dialog), "imfl",
					 imfl, (GtkDestroyNotify)(image_file_list_release));
		set_processing_dialog_imfl(dialog, imfl);
	}
	if (ccdr) {
		ccd_reduce_ref(ccdr);
		gtk_object_set_data_full(GTK_OBJECT(dialog), "ccdred",
					 ccdr, (GtkDestroyNotify)(ccd_reduce_release));
		set_processing_dialog_ccdr(dialog, ccdr);
	}
}

/* mark selected files to be skipped */
static void imf_skip_cb(GtkWidget *wid, gpointer dialog)
{
	GList *sel = NULL;
	GtkList *list;
	GtkListItem *item;
	struct image_file *imf;
	char label[256];

	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	sel = list->selection;
	while(sel != NULL) {
		item = sel->data;
		sel = g_list_next(sel);
		imf = gtk_object_get_data(GTK_OBJECT(item), "imf");
		if (imf == NULL) {
			err_printf("null imf\n");
			continue;
		}
		d3_printf("skipping %s\n", imf->filename);
		imf->flags |= IMG_SKIP;
		snprintf(label, 255, "[ %s ]", imf->filename);
		gtk_label_set_text(GTK_LABEL(GTK_BIN(item)->child), label);
	}
	if (g_list_length(list->selection) == 1) 
		imf_next_cb(wid, dialog);
	update_selected_status_label(dialog);
}

/* remove skip marks from selected files */
static void imf_unskip_cb(GtkWidget *wid, gpointer dialog)
{
	GList *sel = NULL;
	GtkList *list;
	GtkListItem *item;
	struct image_file *imf;

	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	sel = list->selection;
	while(sel != NULL) {
		item = sel->data;
		sel = g_list_next(sel);
		imf = gtk_object_get_data(GTK_OBJECT(item), "imf");
		if (imf == NULL) {
			err_printf("null imf\n");
			continue;
		}
		d3_printf("unskipping %s\n", imf->filename);
		imf->flags &= ~IMG_SKIP;
		gtk_label_set_text(GTK_LABEL(GTK_BIN(item)->child), imf->filename);
	}
	update_selected_status_label(dialog);
}

/* remove selected files */
static void imf_rm_cb(GtkWidget *wid, gpointer dialog)
{
	GList *sel = NULL;
	GList *sel2 = NULL;
	GtkList *list;
	GtkListItem *item;
	struct image_file *imf;
	struct image_file_list *imfl;

	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	imfl = gtk_object_get_data(GTK_OBJECT(dialog), "imfl");
	g_return_if_fail(imfl != NULL);

	sel = list->selection;
	sel2 = g_list_copy(list->selection);
	while(sel != NULL) {
		item = sel->data;
		sel = g_list_next(sel);
		imf = gtk_object_get_data(GTK_OBJECT(item), "imf");
		if (imf == NULL) {
			err_printf("null imf\n");
			continue;
		}
		d3_printf("removing %s\n", imf->filename);
		imfl->imlist = g_list_remove(imfl->imlist, imf);
		image_file_release(imf);
	}
	gtk_list_unselect_all(list);
	gtk_list_remove_items(list, sel2);
	g_list_free(sel2);
	update_selected_status_label(dialog);
}

/* select and display next frame in list */
static void imf_next_cb(GtkWidget *wid, gpointer dialog)
{
	GList *sel = NULL;
	GtkList *list;
	int index, len;
	GtkWidget *scw;
	GtkAdjustment *vadj;
	double nv;

	d3_printf("next\n");

	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	sel = list->selection;
	if (sel == NULL) {
		gtk_list_select_item(list, 0);
		imf_display_cb(wid, dialog);
		return;
	}
	index = gtk_list_child_position(list, GTK_WIDGET(sel->data));
	d3_printf("initial position is %d\n", index);
	/* we reuse sel here! */
	sel = gtk_container_children(GTK_CONTAINER(list));
	len = g_list_length(sel);
	g_list_free(sel);
	if (index + 1 < len) { 
		gtk_list_unselect_all(list);
		index ++;
		gtk_list_select_item(list, index);
		imf_display_cb(wid, dialog);
	} else if (index + 1 == len) {
		gtk_list_unselect_all(list);
		gtk_list_select_item(list, index);
		imf_display_cb(wid, dialog);
	}

	scw = gtk_object_get_data(GTK_OBJECT(dialog), "scrolledwindow");
	g_return_if_fail(scw != NULL);
	vadj =  gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scw));
	d3_printf("vadj at %.3f\n", vadj->value);
	if (len != 0) {
		nv = (vadj->upper + vadj->lower) * index / len - vadj->page_size / 2;
		clamp_double(&nv, vadj->lower, vadj->upper - vadj->page_size);
		gtk_adjustment_set_value(vadj, nv);
		d3_printf("vadj set to %.3f\n", vadj->value);
	}
	update_selected_status_label(dialog);
}


/* select and display previous frame in list */
static void imf_prev_cb(GtkWidget *wid, gpointer dialog)
{
	GList *sel = NULL;
	GtkList *list;
	int index, len;
	GtkWidget *scw;
	GtkAdjustment *vadj;
	double nv;

	d3_printf("prev\n");

	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	sel = list->selection;
	if (sel == NULL) {
		gtk_list_select_item(list, 0);
		imf_display_cb(wid, dialog);
		return;
	}
	index = gtk_list_child_position(list, GTK_WIDGET(sel->data));
	d3_printf("initial position is %d\n", index);
	/* we reuse sel here! */
	sel = gtk_container_children(GTK_CONTAINER(list));
	len = g_list_length(sel);
	g_list_free(sel);
	if (index > 0) { 
		gtk_list_unselect_all(list);
		index --;
		gtk_list_select_item(list, index);
		imf_display_cb(wid, dialog);
	} else if (index == 0) {
		gtk_list_unselect_all(list);
		gtk_list_select_item(list, index);
		imf_display_cb(wid, dialog);
	}

	scw = gtk_object_get_data(GTK_OBJECT(dialog), "scrolledwindow");
	g_return_if_fail(scw != NULL);
	vadj =  gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scw));
	d3_printf("vadj at %.3f\n", vadj->value);
	if (len != 0) {
		nv = (vadj->upper + vadj->lower) * index / len - vadj->page_size / 2;
		clamp_double(&nv, vadj->lower, vadj->upper - vadj->page_size);
		gtk_adjustment_set_value(vadj, nv);
		d3_printf("vadj set to %.3f\n", vadj->value);
	}
	update_selected_status_label(dialog);
}


static void imf_display_cb(GtkWidget *wid, gpointer dialog)
{
	GtkWidget *im_window;
	GtkList *list;
	GtkListItem *item;
	struct image_file *imf;
	struct image_file_list *imfl;

	d3_printf("display\n");

	im_window = gtk_object_get_data(GTK_OBJECT(dialog), "im_window");
	g_return_if_fail(im_window != NULL);
	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	if (list->selection != NULL) {
		item = list->selection->data;
		imf = gtk_object_get_data(GTK_OBJECT(item), "imf");
		if (imf == NULL) {
			err_printf("null imf\n");
			return;
		}
		if (load_image_file(imf)) {
			return;
		}
		g_return_if_fail(imf->fr != NULL);
		frame_to_channel(imf->fr, im_window, "i_channel");
		imf_update_status_label(dialog, imf);
	} else {
		err_printf("No Frame selected\n");
	}
	if (P_INT(FILE_SAVE_MEM)) {
		imfl = gtk_object_get_data(GTK_OBJECT(dialog), "imfl");
		if (imfl)
			unload_clean_frames(imfl);
	}
	update_selected_status_label(dialog);
}

static void imf_selall_cb(GtkWidget *wid, gpointer dialog)
{
	GtkList *list;

	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	gtk_list_select_all(list);
}

static void imf_add_files(GList *files, gpointer dialog)
{
	GtkList *list;
	GtkWidget *item;
	GList *il = NULL;
	struct image_file *imf;
	struct image_file_list *imfl;
	char *text;

	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	imfl = gtk_object_get_data(GTK_OBJECT(dialog), "imfl");
	if (imfl == NULL) {
		imfl = image_file_list_new();
		gtk_object_set_data_full(GTK_OBJECT(dialog), "imfl",
					 imfl, (GtkDestroyNotify)(image_file_list_release));
	}

	while (files != NULL) {
		imf = image_file_new();
		text = files->data;
		files = g_list_next(files);

		imf->filename = strdup(text);
		item=gtk_list_item_new_with_label(imf->filename);
		image_file_ref(imf);
		gtk_object_set_data_full(GTK_OBJECT(item), "imf",
					 imf, (GtkDestroyNotify)(image_file_release));
		image_file_ref(imf);
		imfl->imlist = g_list_append(imfl->imlist, imf);

		il=g_list_append(il, item);
		gtk_widget_show(item);
		d3_printf("adding %s\n", text);
	}
	gtk_list_append_items(list, il);
	update_selected_status_label(dialog);
}

static void imf_add_cb(GtkWidget *wid, gpointer dialog)
{
	GtkList *list;

	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	d3_printf("imf add\n");

	file_select_list(dialog, "Select files", "*.fits", imf_add_files);
}

/* reload selected files */
static void imf_reload_cb(GtkWidget *wid, gpointer dialog)
{
	GList *sel = NULL;
	GtkList *list;
	GtkListItem *item;
	struct image_file *imf;
	struct image_file_list *imfl;

	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	imfl = gtk_object_get_data(GTK_OBJECT(dialog), "imfl");
	g_return_if_fail(imfl != NULL);

	sel = list->selection;
	while(sel != NULL) {
		item = sel->data;
		sel = g_list_next(sel);
		imf = gtk_object_get_data(GTK_OBJECT(item), "imf");
		if (imf == NULL) {
			err_printf("null imf\n");
			continue;
		}
		d3_printf("unloading %s\n", imf->filename);
		if ((imf->flags & IMG_LOADED) && (imf->fr)) {
			release_frame(imf->fr);
			imf->fr = NULL;
		}
		imf->flags &= IMG_SKIP; /* we keep the skip flag */
	}
	update_selected_status_label(dialog);
}

void switch_frame_cb(gpointer window, guint action, GtkWidget *menu_item)
{
	GtkWidget *dialog;
	dialog = gtk_object_get_data(GTK_OBJECT(window), "processing");
	if (dialog == NULL) {
		dialog = make_image_processing(window);
	}
	g_return_if_fail(dialog != NULL);

	switch(action) {
	case SWF_NEXT:
		imf_next_cb(NULL, dialog);
		break;
	case SWF_SKIP:
		imf_skip_cb(NULL, dialog);
		break;
	case SWF_PREV:
		imf_prev_cb(NULL, dialog);
		break;
	}
	update_selected_status_label(dialog);
}

static void imf_update_status_label(gpointer dialog, struct image_file *imf)
{
	char *fn;
	char msg[128];
	int i;
	GtkWidget *label;

	label = gtk_object_get_data(GTK_OBJECT(dialog), "imf_info_label");
	g_return_if_fail(label != NULL);

	if (imf == NULL) {
		gtk_label_set_text(GTK_LABEL(label), "");
		return;
	}

	fn = strdup(imf->filename);

	i = snprintf(msg, 127, "%s", basename(fn));
	free(fn);
	if (imf->flags & IMG_SKIP)
		i += snprintf(msg+i, 127-i, " SKIP");
	clamp_int(&i, 0, 127);
	if (imf->flags & IMG_LOADED)
		i += snprintf(msg+i, 127-i, " LD");
	clamp_int(&i, 0, 127);
	if (imf->flags & IMG_OP_BIAS)
		i += snprintf(msg+i, 127-i, " BIAS");
	clamp_int(&i, 0, 127);
	if (imf->flags & IMG_OP_DARK)
		i += snprintf(msg+i, 127-i, " DARK");
	clamp_int(&i, 0, 127);
	if (imf->flags & IMG_OP_FLAT)
		i += snprintf(msg+i, 127-i, " FLAT");
	clamp_int(&i, 0, 127);
	if (imf->flags & IMG_OP_MUL)
		i += snprintf(msg+i, 127-i, " MUL");
	clamp_int(&i, 0, 127);
	if (imf->flags & IMG_OP_ADD)
		i += snprintf(msg+i, 127-i, " ADD");
	clamp_int(&i, 0, 127);
	if (imf->flags & IMG_OP_ALIGN)
		i += snprintf(msg+i, 127-i, " ALIGN");
	gtk_label_set_text(GTK_LABEL(label), msg);
}

static void update_selected_status_label(gpointer dialog)
{
	GtkWidget *im_window;
	GtkList *list;
	GtkListItem *item;
	struct image_file *imf;

	d3_printf("display\n");

	im_window = gtk_object_get_data(GTK_OBJECT(dialog), "im_window");
	g_return_if_fail(im_window != NULL);
	list = gtk_object_get_data(GTK_OBJECT(dialog), "image_file_list");
	g_return_if_fail(list != NULL);

	if (list->selection != NULL) {
		item = list->selection->data;
		imf = gtk_object_get_data(GTK_OBJECT(item), "imf");
		imf_update_status_label(dialog, imf);
	}
}


static void select_cb(GtkList *list, GtkWidget *wid, gpointer dialog)
{
	char *text;
	struct image_file *imf;

	gtk_label_get(GTK_LABEL(GTK_BIN(wid)->child), &text);
	d3_printf("select: %s\n", text);

	imf = gtk_object_get_data(GTK_OBJECT(wid), "imf");
	g_return_if_fail(imf != NULL);
	imf_update_status_label(dialog, imf);
}

/*
static void list_button_cb(GtkWidget *wid, GdkEventButton *event, gpointer dialog)
{
	if (event->type == GDK_2BUTTON_PRESS) {
		while(gtk_events_pending())
		      gtk_main_iteration();
		imf_display_cb(NULL, dialog);
	}
}
*/

static void imf_red_browse_cb(GtkWidget *wid, gpointer dialog)
{
	GtkWidget *entry;
	if (wid == gtk_object_get_data(GTK_OBJECT(dialog), "bias_browse")) {
		entry = gtk_object_get_data(GTK_OBJECT(dialog), "bias_entry");
		g_return_if_fail(entry != NULL);
		file_select_to_entry(dialog, entry, "Select bias frame", "*", 1);
	}
	if (wid == gtk_object_get_data(GTK_OBJECT(dialog), "dark_browse")) {
		entry = gtk_object_get_data(GTK_OBJECT(dialog), "dark_entry");
		g_return_if_fail(entry != NULL);
		file_select_to_entry(dialog, entry, "Select dark frame", "*", 1);
	}
	if (wid == gtk_object_get_data(GTK_OBJECT(dialog), "flat_browse")) {
		entry = gtk_object_get_data(GTK_OBJECT(dialog), "flat_entry");
		g_return_if_fail(entry != NULL);
		file_select_to_entry(dialog, entry, "Select flat frame", "*", 1);
	}
	if (wid == gtk_object_get_data(GTK_OBJECT(dialog), "align_browse")) {
		entry = gtk_object_get_data(GTK_OBJECT(dialog), "align_entry");
		g_return_if_fail(entry != NULL);
		file_select_to_entry(dialog, entry, "Select alignment reference frame", "*", 1);
	}
	if (wid == gtk_object_get_data(GTK_OBJECT(dialog), "output_file_browse")) {
		entry = gtk_object_get_data(GTK_OBJECT(dialog), "output_file_entry");
		g_return_if_fail(entry != NULL);
		file_select_to_entry(dialog, entry, "Select output file/dir", "*", 1);
	}
}

static int progress_pr(char *msg, void *dialog)
{
	GtkWidget *text;
//	GdkFont *font = NULL;

//	d1_printf("*%s", msg);

	text = gtk_object_get_data(GTK_OBJECT(dialog), "processing_log_text");
	g_return_val_if_fail(text != NULL, 0);

	gtk_text_set_line_wrap(GTK_TEXT (text), 0);
	gtk_text_insert (GTK_TEXT (text), text->style->font, &text->style->black, NULL,
				 msg, -1);

	while(gtk_events_pending())
		gtk_main_iteration();

	return 0;
}

static void dialog_to_ccdr(GtkWidget *dialog, struct ccd_reduce *ccdr)
{
	char *text;

	g_return_if_fail (ccdr != NULL);
	if (get_named_checkb_val(dialog, "bias_checkb")) {
		text = named_entry_text(dialog, "bias_entry");
		if ((ccdr->ops & IMG_OP_BIAS) && ccdr->bias && 
		    strcmp(text, ccdr->bias->filename)) {
			image_file_release(ccdr->bias);
			ccdr->bias = image_file_new();
			ccdr->bias->filename = strdup(text);
		} else if (!(ccdr->ops & IMG_OP_BIAS)) {
			ccdr->bias = image_file_new();
			ccdr->bias->filename = strdup(text);
		} 
		g_free(text);
		ccdr->ops |= IMG_OP_BIAS;
	} else {
		ccdr->ops &= ~IMG_OP_BIAS;
		if ((ccdr->ops & IMG_OP_BIAS) && ccdr->bias)
			image_file_release(ccdr->bias);
		ccdr->bias = NULL;
	}

	if (get_named_checkb_val(dialog, "dark_checkb")) {
		text = named_entry_text(dialog, "dark_entry");
		if ((ccdr->ops & IMG_OP_DARK) && ccdr->dark && 
		    strcmp(text, ccdr->dark->filename)) {
			image_file_release(ccdr->dark);
			ccdr->dark = image_file_new();
			ccdr->dark->filename = strdup(text);
		} else if (!(ccdr->ops & IMG_OP_DARK)) {
			ccdr->dark = image_file_new();
			ccdr->dark->filename = strdup(text);
		} 
		g_free(text);
		ccdr->ops |= IMG_OP_DARK;
	} else {
		ccdr->ops &= ~IMG_OP_DARK;
		if ((ccdr->ops & IMG_OP_DARK) && ccdr->dark)
			image_file_release(ccdr->dark);
		ccdr->dark = NULL;
	}

	if (get_named_checkb_val(dialog, "flat_checkb")) {
		text = named_entry_text(dialog, "flat_entry");
		if ((ccdr->ops & IMG_OP_FLAT) && ccdr->flat && 
		    strcmp(text, ccdr->flat->filename)) {
			image_file_release(ccdr->flat);
			ccdr->flat = image_file_new();
			ccdr->flat->filename = strdup(text);
		} else if (!(ccdr->ops & IMG_OP_FLAT)) {
			ccdr->flat = image_file_new();
			ccdr->flat->filename = strdup(text);
		} 
		g_free(text);
		ccdr->ops |= IMG_OP_FLAT;
	} else {
		ccdr->ops &= ~IMG_OP_FLAT;
		if ((ccdr->ops & IMG_OP_FLAT) && ccdr->flat)
			image_file_release(ccdr->flat);
		ccdr->flat = NULL;
	}


	if (get_named_checkb_val(dialog, "align_checkb")) {
		text = named_entry_text(dialog, "align_entry");
		if ((ccdr->ops & IMG_OP_ALIGN) && ccdr->alignref && 
		    strcmp(text, ccdr->alignref->filename)) {
			image_file_release(ccdr->alignref);
			ccdr->alignref = image_file_new();
			ccdr->alignref->filename = strdup(text);
			free_alignment_stars(ccdr);
		} else if (!(ccdr->ops & IMG_OP_ALIGN)) {
			ccdr->alignref = image_file_new();
			ccdr->alignref->filename = strdup(text);
		} 
		g_free(text);
		ccdr->ops |= IMG_OP_ALIGN;
	} else {
		ccdr->ops &= ~IMG_OP_ALIGN;
		if ((ccdr->ops & IMG_OP_ALIGN) && ccdr->alignref)
			image_file_release(ccdr->alignref);
		ccdr->alignref = NULL;
	}

	if (get_named_checkb_val(dialog, "mul_checkb")) {
		ccdr->ops |= IMG_OP_MUL;
		ccdr->mulv = named_spin_get_value(dialog, "mul_spin");
	} else {
		ccdr->ops &= ~IMG_OP_MUL;
	}

	if (get_named_checkb_val(dialog, "add_checkb")) {
		ccdr->ops |= IMG_OP_ADD;
		ccdr->addv = named_spin_get_value(dialog, "add_spin");
	} else {
		ccdr->ops &= ~IMG_OP_ADD;
	}

	if (get_named_checkb_val(dialog, "blur_checkb")) {
		ccdr->ops |= IMG_OP_BLUR;
		ccdr->blurv = named_spin_get_value(dialog, "blur_spin");
	} else {
		ccdr->ops &= ~IMG_OP_BLUR;
	}

	if (get_named_checkb_val(dialog, "stack_checkb")) {
		ccdr->ops |= IMG_OP_STACK;
		if (get_named_checkb_val(dialog, "bg_match_add_rb")) {
			ccdr->ops |= IMG_OP_BG_ALIGN_ADD;
		} else {
			ccdr->ops &= ~IMG_OP_BG_ALIGN_ADD;
		}
		if (get_named_checkb_val(dialog, "bg_match_mul_rb")) {
			ccdr->ops |= IMG_OP_BG_ALIGN_MUL;
		} else {
			ccdr->ops &= ~IMG_OP_BG_ALIGN_MUL;
		}
		P_DBL(CCDRED_SIGMAS) = named_spin_get_value(dialog, 
							    "stack_sigmas_spin");
		par_touch(CCDRED_SIGMAS);
		P_INT(CCDRED_ITER) = named_spin_get_value(dialog, 
							  "stack_iter_spin");
		par_touch(CCDRED_ITER);
	} else {
		ccdr->ops &= ~IMG_OP_STACK;
	}
}

static void ccdred_run_cb(GtkWidget *wid, gpointer dialog)
{
	struct ccd_reduce *ccdr;	
	struct image_file_list *imfl;
	int ret, nframes;
	struct image_file *imf;
	struct ccd_frame *fr;
	GList *gl;
	int seq = 1;
	char *outf;
	GtkWidget *im_window;

	imfl = gtk_object_get_data(GTK_OBJECT(dialog), "imfl");
	g_return_if_fail (imfl != NULL);
	ccdr = gtk_object_get_data(GTK_OBJECT(dialog), "ccdred");
	if (ccdr == NULL) {
		ccdr = ccd_reduce_new();
		gtk_object_set_data_full(GTK_OBJECT(dialog), "ccdred",
					 ccdr, (GtkDestroyNotify)(ccd_reduce_release));
	}
	g_return_if_fail (ccdr != NULL);
	ccdr->ops &= ~CCDR_BG_VAL_SET;
	dialog_to_ccdr(dialog, ccdr);

	outf = named_entry_text(dialog, "output_file_entry");
	d3_printf("outf is |%s|\n", outf);

	nframes = g_list_length(imfl->imlist);
	if (!(ccdr->ops & IMG_OP_STACK)) {
//		ccdr->ops &= ~(IMG_OP_BG_ALIGN_MUL | IMG_OP_BG_ALIGN_ADD);
		gl = imfl->imlist;
		while (gl != NULL) {
			imf = gl->data;
			gl = g_list_next(gl);
			if (imf->flags & IMG_SKIP)
				continue;
			ret = reduce_frame(imf, ccdr, progress_pr, dialog);
			if (ret)
				continue;
			if (ccdr->ops & IMG_OP_INPLACE) {
				save_image_file(imf, outf, 1, NULL, progress_pr, dialog);
				imf->flags &= ~(IMG_SKIP | IMG_LOADED);
				release_frame(imf->fr);
				imf->fr = NULL;
			} else if (outf && outf[0]) {
				save_image_file(imf, outf, 0, 
						(nframes == 1 ? NULL : &seq), 
						progress_pr, dialog);
				imf->flags &= ~(IMG_SKIP | IMG_LOADED);
				release_frame(imf->fr);
				imf->fr = NULL;
			}
		}
		update_selected_status_label(dialog);
		return;
	}
	update_selected_status_label(dialog);
	if (reduce_frames(imfl, ccdr, progress_pr, dialog))
		return;
	if ((fr = stack_frames(imfl, ccdr, progress_pr, dialog)) == NULL)
		return;
	if (outf && outf[0]) {
		imf = image_file_new();
		imf->filename = outf;
		imf->fr = fr;
		imf->flags |= IMG_LOADED;
		get_frame(fr);
		snprintf(fr->name, 254, "%s", outf); 
		d3_printf("Writing %s\n", outf);
		save_image_file(imf, outf, 0, &seq, 
				progress_pr, dialog);
		image_file_release(imf);
	}
	im_window = gtk_object_get_data(GTK_OBJECT(dialog), "im_window");
	g_return_if_fail(im_window != NULL);
	frame_to_channel(fr, im_window, "i_channel");
	release_frame(fr);
}


static void show_align_cb(GtkWidget *wid, gpointer dialog)
{
	struct ccd_reduce *ccdr;	
	GtkWidget *im_window;

	ccdr = gtk_object_get_data(GTK_OBJECT(dialog), "ccdred");
	if (ccdr == NULL) {
		ccdr = ccd_reduce_new();
		gtk_object_set_data_full(GTK_OBJECT(dialog), "ccdred",
					 ccdr, (GtkDestroyNotify)(ccd_reduce_release));
	}
	g_return_if_fail (ccdr != NULL);
	dialog_to_ccdr(dialog, ccdr);

	im_window = gtk_object_get_data(GTK_OBJECT(dialog), "im_window");
	g_return_if_fail(im_window != NULL);

	d3_printf("ops: %08x, align_stars: %08p\n", ccdr->ops, ccdr->align_stars);

	if (!(ccdr->ops & CCDR_ALIGN_STARS))
		load_alignment_stars(ccdr);

	remove_stars_of_type_window(im_window, TYPE_MASK(STAR_TYPE_ALIGN), 0);
	add_gui_stars_to_window(im_window, ccdr->align_stars);
	gtk_widget_queue_draw(im_window);
}

/* set the enable check buttons when the user activates a file entry */
static void imf_red_activate_cb(GtkWidget *wid, gpointer dialog)
{

	if (wid == gtk_object_get_data(GTK_OBJECT(dialog), "dark_entry")) {
		set_named_checkb_val(dialog, "dark_checkb", 1);
	}
	if (wid == gtk_object_get_data(GTK_OBJECT(dialog), "flat_entry")) {
		set_named_checkb_val(dialog, "flat_checkb", 1);
	}
	if (wid == gtk_object_get_data(GTK_OBJECT(dialog), "bias_entry")) {
		set_named_checkb_val(dialog, "bias_checkb", 1);
	}
	if (wid == gtk_object_get_data(GTK_OBJECT(dialog), "align_entry")) {
		set_named_checkb_val(dialog, "align_checkb", 1);
	}

}
