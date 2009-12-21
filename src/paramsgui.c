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

/* gui functions related to par tree editing */
/* hopefully, all the mess is contained in this file */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gtk/gtk.h>

#include "gcx.h"
#include "params.h"
#include "catalogs.h"
#include "sourcesdraw.h"
#include "interface.h"
#include "misc.h"
#include "gui.h"

#define PAR_SIZE 150 /* size of par entry */


static void par_selected_cb(GtkTree *tree, GtkWidget *widget, gpointer parwin);
static void par_update_current(gpointer parwin);


static void make_par_tree_label(GcxPar p, char *buf)
{
	if (PAR(p)->flags & PAR_USER) {
		sprintf(buf, "%s (changes)", PAR(p)->comment);
	} else {
		sprintf(buf, "%s", PAR(p)->comment);
	}
}

/* regenerate the tree label and update */
static void update_tree_label(GcxPar pp)
{
	GtkWidget *item;
	char text[256] = "";

	if (PAR_TYPE(pp) != PAR_TREE)
		return;
	item = PAR(pp)->item;
	make_par_tree_label(pp, text);
	gtk_label_set_text(GTK_LABEL(GTK_BIN(item)->child), text);
	gtk_widget_queue_draw(item);
}

static void par_item_update_status(GcxPar p)
{
	GtkWidget *item;
	GtkWidget *statlabel;
	item = PAR(p)->item;
/*  	savecheck = gtk_object_get_data(GTK_OBJECT(item), "savecheck"); */
	statlabel = gtk_object_get_data(GTK_OBJECT(item), "statlabel");
/*  	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(savecheck),  */
/*  				      ((PAR(p)->flags & PAR_TO_SAVE) != 0));  */
  	gtk_label_set_text(GTK_LABEL(statlabel), status_string(p)); 
	gtk_widget_queue_draw(item);
}

/* mark a par and all it's ancestors as changed */
static void par_mark_changed(GcxPar p)
{
	GcxPar pp;

	PAR(p)->flags |= (PAR_TO_SAVE | PAR_USER);
//	PAR(p)->flags &= ~(PAR_FROM_RC);

	par_item_update_status(p);
	pp = PAR(p)->parent;
	while (pp != PAR_NULL) {
		PAR(pp)->flags |= (PAR_TO_SAVE | PAR_USER);
//		PAR(pp)->flags &= ~(PAR_FROM_RC);
		update_tree_label(pp);
		pp = PAR(pp)->parent;
	}
}

/* we reach here when a parameter has possibly been edited */
/* this runs in tree callback context */
static void param_edited(GtkEditable *editable, GcxPar p)
{
	char *text;
	char buf[256];

	text = gtk_editable_get_chars(editable, 0, -1);
	d4_printf("possibly edited %d text is %s\n", p, text);
	make_value_string(p, buf, 255);
	if (!strcmp(buf, text)) {
		g_free(text);
		return;
	} 
	if (!try_update_par_value(p, text)) { // we have a successful update
		par_mark_changed(p);
	} else {
		error_beep();
		d3_printf("cannot parse text\n");
	}
	g_free(text);
}

/* update the item's value in the entry/optionmenu/checkbox
 */
static void par_item_update_value(GcxPar p)
{
}

static gboolean par_clicked_cb (GtkWidget *widget, GdkEventButton *event, gpointer root)
{
	if (event->button == 1) {
		if (event->state & GDK_SHIFT_MASK) { 
			gtk_tree_set_selection_mode (GTK_TREE(root),
						     GTK_SELECTION_MULTIPLE);
		} else {
			gtk_tree_set_selection_mode (GTK_TREE(root),
						     GTK_SELECTION_SINGLE);
		}
	}
	return TRUE;
}

/* make an tree item containing a parameter */
GtkWidget * make_ptree_leaf(GcxPar p, GtkWidget *root)
{
	GtkWidget *treeitem;
	GtkWidget *hbox1;
	GtkWidget *description;
	GtkWidget *statlabel;

//	d3_printf("making leaf item %d of type %d comment: %s\n", 
//		  p, PAR_TYPE(p), PAR(p)->comment);

	treeitem = gtk_tree_item_new ();
	gtk_signal_connect(GTK_OBJECT(treeitem), "button_press_event", 
			   GTK_SIGNAL_FUNC(par_clicked_cb), root);
	gtk_widget_show (treeitem);

	hbox1 = gtk_hbox_new (FALSE, 0);
	gtk_widget_ref (hbox1);
	gtk_object_set_data_full (GTK_OBJECT (treeitem), "hbox1", hbox1,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (hbox1);
	gtk_container_add (GTK_CONTAINER (treeitem), hbox1);

	description = gtk_label_new (PAR(p)->comment);
	gtk_widget_ref (description);
	gtk_object_set_data_full (GTK_OBJECT (treeitem), "description", description,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (description);
	gtk_box_pack_end (GTK_BOX (hbox1), description, TRUE, TRUE, 0);
	gtk_misc_set_alignment (GTK_MISC (description), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (description), 5, 0);

	statlabel = gtk_label_new (status_string(p));
	gtk_widget_ref (statlabel);
	gtk_object_set_data_full (GTK_OBJECT (treeitem), "statlabel", statlabel,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (statlabel);

	gtk_misc_set_padding (GTK_MISC (statlabel), 5, 0);
	gtk_object_set_data(GTK_OBJECT (treeitem), "parameter", GINT_TO_POINTER(p));
	PAR(p)->item = treeitem;
	return treeitem;
}

GtkWidget *make_stree_item(GcxPar p, GtkWidget *root)
{
	GtkWidget *item;
	char text[256] = "";

//	d3_printf("making tree item %d of type %d comment: %s\n", 
//		  p, PAR_TYPE(p), PAR(p)->comment);
	make_par_tree_label(p, text);
	item = gtk_tree_item_new_with_label (text);
	gtk_signal_connect(GTK_OBJECT(item), "button_press_event", 
			   GTK_SIGNAL_FUNC(par_clicked_cb), root);
	gtk_object_set_data(GTK_OBJECT (item), "parameter", GINT_TO_POINTER(p));
	gtk_widget_show (item);
 	PAR(p)->item = item;
	return item;
}

GtkWidget *make_subtree(GcxPar p, GtkWidget *item, GtkWidget *root, GtkWidget *parwin)
{
	GtkWidget *subtree;
	GtkWidget *child;
	GtkWidget *sstree;
	GcxPar pp;

	if (PAR_TYPE(p) != PAR_TREE) {
		err_printf("not a tree param\n");
		return NULL;
	}
//	d3_printf("item is [%08x]\n", (unsigned)item);
	/* Create this item's subtree */
	subtree = gtk_tree_new();
	gtk_widget_ref (subtree);
	gtk_widget_show(subtree);
	gtk_signal_connect (GTK_OBJECT (subtree), "select-child",
			    GTK_SIGNAL_FUNC (par_selected_cb), parwin);
	gtk_tree_item_set_subtree (GTK_TREE_ITEM(item), subtree);
	/* and now walk the children and add them in the subtree */
	/* nothing like recursion for tree building */
	gtk_object_set_data_full (GTK_OBJECT (item), "subtree", 
				  subtree, (GtkDestroyNotify) gtk_widget_unref);
//	d3_printf("subtree is [%08x]\n", (unsigned)subtree);
	pp = PAR(p)->child;
	while (pp != PAR_NULL) {
		if (PAR_TYPE(pp) == PAR_TREE) {
			child = make_stree_item(pp, root);
			gtk_tree_append (GTK_TREE(subtree), child);
			sstree = make_subtree(pp, child, root, parwin);
		} else {
			child = make_ptree_leaf(pp, root);
			gtk_tree_append (GTK_TREE(subtree), child);
		}
		pp = PAR(pp)->next;
	} 
	return item;
}

GtkWidget* make_par_tree(GtkWidget *parwin) 
{
	GtkWidget *tree1;
	GcxPar p;
	GtkWidget *item;

	tree1 = gtk_tree_new ();
	p = PAR_FIRST;
	while (p != PAR_NULL) {
		item = make_stree_item(p, tree1);
// 		d3_printf("made first-level tree item %d [%08x]\n", p, (unsigned)item);
		gtk_tree_append (GTK_TREE(tree1), item);
		make_subtree(p, item, tree1, parwin);
		p = PAR(p)->next;
	}
	gtk_widget_show (tree1);
	return tree1;
}


static void par_item_restore_default_gui(GcxPar p)
{
	if (PAR(p)->flags & (PAR_TREE)) { // recurse tree
		GcxPar pp;
		PAR(p)->flags &= ~PAR_USER;
		update_tree_label(p);
		pp = PAR(p)->child;
		while(pp != PAR_NULL) {
			par_item_restore_default_gui(pp);
			pp = PAR(pp)->next;
		}
	} else if (PAR(p)->flags & (PAR_USER)) {
			if (PAR_TYPE(p) == PAR_STRING) {
				change_par_string(p, PAR(p)->defval.s);
			} else {
				memcpy(&(PAR(p)->val), &(PAR(p)->defval), sizeof(union pval));
			}
//			par_item_update_value(p);
//			d3_printf("defaulting par %d\n", p);
			PAR(p)->flags &= ~PAR_USER; 
			PAR(p)->flags &= ~PAR_FROM_RC; 
			PAR(p)->flags &= ~PAR_TO_SAVE;
			par_item_update_status(p);
	}
}

static void update_ancestors_state_gui(GcxPar p)
{
	p = PAR(p)->parent;
	while (p != PAR_NULL && PAR_TYPE(p) == PAR_TREE) {
		int type = 0;
		PAR(p)->flags &= ~(PAR_USER | PAR_FROM_RC);
		type = or_child_type(p, 0);
		PAR(p)->flags |= (type & (PAR_USER | PAR_FROM_RC));
		update_tree_label(p);
		p = PAR(p)->parent;
	}
}

static void restore_defaults_cb( GtkWidget *widget, gpointer parwin )
{
	GList *i;
	GtkTree *tree;

	tree = GTK_TREE(gtk_object_get_data(GTK_OBJECT(parwin), "tree1"));
	g_return_if_fail(tree != NULL);
  
	i = GTK_TREE_ROOT_TREE(tree)->selection;

	while (i) {
		GtkWidget *item;
		gpointer p;
		item = GTK_WIDGET (i->data);
		p = gtk_object_get_data(GTK_OBJECT(item), "parameter");
		par_item_restore_default_gui(GPOINTER_TO_INT(p));
		update_ancestors_state_gui(GPOINTER_TO_INT(p));
		i = i->next;
	}
	par_update_current(parwin);
}



static void par_edit_changed(GtkEditable *editable, gpointer parwin);

static void setup_par_combo(gpointer parwin, GtkWidget *combo, GcxPar p)
{
	GtkWidget *entry;
	char val[256];
	char ** c;
	GList *cl = NULL;

	entry = GTK_COMBO(combo)->entry;

	gtk_signal_handler_block_by_func(GTK_OBJECT(entry), par_edit_changed, parwin);

	if (PAR_FORMAT(p) == FMT_OPTION
	    && PAR(p)->choices != NULL) {
		c = PAR(p)->choices;
		while(*c != NULL) {
			cl = g_list_append(cl, *c);
			c++;
		}
	} else if (PAR_FORMAT(p) == FMT_BOOL) {
		cl = g_list_append(cl, "yes");
		cl = g_list_append(cl, "no");
	}
	if (!cl) {
		gtk_entry_set_editable(GTK_ENTRY(entry), 1);
		make_defval_string(p, val, 255);
		cl = g_list_append(cl, val);
		gtk_combo_disable_activate(GTK_COMBO(combo));
		gtk_combo_set_value_in_list(GTK_COMBO(combo), 0, 0);
	} else {
		gtk_entry_set_editable(GTK_ENTRY(entry), 0);
		gtk_combo_set_value_in_list(GTK_COMBO(combo), 1, 0);
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), cl);

	make_value_string(p, val, 255);
	gtk_entry_set_text (GTK_ENTRY (entry), (val));
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(entry), par_edit_changed, parwin);
	d4_printf("setting val to %s\n", val);
}

/* refresh the labels of a par in the editing dialog */
static void update_par_labels(GcxPar p, gpointer parwin)
{
	GtkWidget *label;
	char buf[256] = "Config file: ";

	label = gtk_object_get_data(GTK_OBJECT(parwin), "par_title_label");
	g_return_if_fail(label != NULL);
	gtk_label_set_text(GTK_LABEL(label), PAR(p)->comment);

	label = gtk_object_get_data(GTK_OBJECT(parwin), "par_fname_label");
	g_return_if_fail(label != NULL);
	par_pathname(p, buf+13, 255-13);
	gtk_label_set_text(GTK_LABEL(label), buf);

	label = gtk_object_get_data(GTK_OBJECT(parwin), "par_descr_label");
	g_return_if_fail(label != NULL);
	if (PAR(p)->description)
		gtk_label_set_text(GTK_LABEL(label), PAR(p)->description);
	else
		gtk_label_set_text(GTK_LABEL(label), "");

	label = gtk_object_get_data(GTK_OBJECT(parwin), "par_type_label");
	g_return_if_fail(label != NULL);
	switch(PAR_TYPE(p)) {
	case PAR_INTEGER:
		if (PAR_FORMAT(p) == FMT_BOOL) {
			gtk_label_set_text(GTK_LABEL(label), "Type: Truth value");
		} else if (PAR_FORMAT(p) == FMT_OPTION) {
			gtk_label_set_text(GTK_LABEL(label), "Type: Multiple choices");
		} else {
			gtk_label_set_text(GTK_LABEL(label), "Type: Integer");
		}
		break;
	case PAR_STRING:
		gtk_label_set_text(GTK_LABEL(label), "Type: String");
		break;
	case PAR_DOUBLE:
		gtk_label_set_text(GTK_LABEL(label), "Type: Real Number");
		break;
	case PAR_TREE:
		gtk_label_set_text(GTK_LABEL(label), "Type: Subtree");
		break;
	}

	label = gtk_object_get_data(GTK_OBJECT(parwin), "par_status_label");
	g_return_if_fail(label != NULL);
	if ((PAR_TYPE(p) == PAR_TREE) || (p == PAR_NULL)) {
		gtk_label_set_text(GTK_LABEL(label), "");
	} else {
		gtk_label_set_text(GTK_LABEL(label), status_string(p));
	}
}

/* new par editing callbacks */

static void par_edit_activate(GtkEditable *editable, gpointer parwin)
{
	GcxPar p;
	p = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(parwin), "selpar"));
	param_edited(editable, p);
	update_par_labels(p, parwin);
}

static void par_edit_changed(GtkEditable *editable, gpointer parwin)
{
	GcxPar p;
	d4_printf("changed\n");
	p = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(parwin), "selpar"));
	param_edited(editable, p);
	update_par_labels(p, parwin);
}

static void par_selected_cb(GtkTree *tree, GtkWidget *widget, gpointer parwin)
{
	GcxPar p;
	GtkWidget *combo;

	p = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(widget), "parameter"));
	d4_printf("selected par: %d\n", p);

	gtk_object_set_data(GTK_OBJECT(parwin), "selpar", (gpointer) p);

	update_par_labels(p, parwin);

	combo = gtk_object_get_data(GTK_OBJECT(parwin), "par_combo");
	g_return_if_fail(combo != NULL);
	if ((PAR_TYPE(p) == PAR_TREE) || (p == PAR_NULL)) {
		gtk_widget_hide(combo);
	} else {
		setup_par_combo(parwin, combo, p);
		gtk_widget_show(combo);
	}

}

/* update the currently selected parameter */
static void par_update_current(gpointer parwin)
{
	GtkWidget *combo;
	GcxPar p;
	d4_printf("update current\n");
	p = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(parwin), "selpar"));
	combo = gtk_object_get_data(GTK_OBJECT(parwin), "par_combo");
	g_return_if_fail(combo != NULL);
	if (p != PAR_NULL) {
		setup_par_combo(parwin, combo, p);
		update_par_labels(p, parwin);
	}
//	d3_printf("current par has flags: %x\n", PAR(p)->flags);
}

static void par_save_cb( GtkWidget *widget, gpointer parwin )
{
	save_params_rc();
	par_update_current(parwin);
}

/* update item values for all siblings of p and their descendants
 */
static void par_update_items(GcxPar p)
{
	while (p != PAR_NULL) {
		if (PAR_TYPE(p) == PAR_TREE && PAR(p)->child != PAR_NULL) {
			par_update_items(PAR(p)->child);
		} else {
			par_item_update_value(p);
			par_item_update_status(p);
		}
		p = PAR(p)->next;
	}

}

static void par_load_cb( GtkWidget *widget, gpointer parwin )
{
	if (!load_params_rc())
		par_update_items(PAR_FIRST);
	par_update_current(parwin);
}

static void close_par_dialog(GtkWidget *widget, gpointer parwin)
{
	gpointer window;
	window = gtk_object_get_data(GTK_OBJECT(parwin), "im_window");
	gtk_object_set_data(GTK_OBJECT(window), "params_window", NULL);
}

static void close_parwin(GtkWidget *widget, GdkEvent *event, gpointer window )
{
	gtk_object_set_data(GTK_OBJECT(window), "params_window", NULL);
}


void edit_options_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *window = data;
	GtkWidget *parwin;
	GtkWidget *tree1;
	GtkWidget *viewport;

	parwin = gtk_object_get_data(GTK_OBJECT(window), "params_window");
	if (parwin == NULL) {
		parwin = create_par_edit();
		viewport = gtk_object_get_data(GTK_OBJECT(parwin), "viewport4");
		tree1 = make_par_tree(parwin);
		gtk_widget_ref (tree1);
		gtk_object_set_data_full (GTK_OBJECT (parwin), "tree1", tree1,
					  (GtkDestroyNotify) gtk_widget_unref);
		gtk_tree_set_selection_mode (GTK_TREE(tree1),
					     GTK_SELECTION_MULTIPLE);
		gtk_widget_show (tree1);
		gtk_container_add (GTK_CONTAINER (viewport), tree1);

		gtk_object_set_data(GTK_OBJECT(parwin), "im_window", window);
		gtk_object_set_data_full(GTK_OBJECT(window), "params_window",
					 parwin, (GtkDestroyNotify)(gtk_widget_destroy));

		gtk_signal_connect (GTK_OBJECT (parwin), "delete-event",
				    GTK_SIGNAL_FUNC (close_parwin), window);
		gtk_signal_connect (GTK_OBJECT (tree1), "select-child",
				    GTK_SIGNAL_FUNC (par_selected_cb), parwin);
		set_named_callback (parwin, "par_close", "clicked",
				    GTK_SIGNAL_FUNC (close_par_dialog));
		set_named_callback (parwin, "par_combo_entry", "activate",
				    GTK_SIGNAL_FUNC (par_edit_activate));
		set_named_callback (parwin, "par_combo_entry", "changed",
				    GTK_SIGNAL_FUNC (par_edit_changed));
		set_named_callback (parwin, "par_save", "clicked",
				    GTK_SIGNAL_FUNC (par_save_cb));
		set_named_callback (parwin, "par_default", "clicked",
				    GTK_SIGNAL_FUNC (restore_defaults_cb));
		set_named_callback (parwin, "par_load", "clicked",
				    GTK_SIGNAL_FUNC (par_load_cb));
		gtk_widget_show(parwin);
	} else {
		par_update_current(parwin);
		gdk_window_raise(parwin->window);
	}
}
