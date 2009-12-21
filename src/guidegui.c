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

/* Guide window creation and menus */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
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
#include "guide.h"

#define GUIDE_BOX_SIZE 96 	/* size of guide box */


static GtkItemFactoryEntry guide_image_menu_items[] = {
	{ "/_File",		NULL,         	NULL,  		0, "<Branch>" },
/*  	{ "/File/tear",  	NULL,         	NULL,  		0, "<Tearoff>" }, */
//	{ "/File/_New Frame",	"<control>n", 	new_frame_cb, 	0, "<Item>" },
	{ "/File/_Open Fits",	"<control>o", 	file_popup_cb, 	FILE_OPEN, "<Item>" },
//	{ "/File/_Close",	"<control>c", 	file_popup_cb, 	FILE_CLOSE, "<Item>" },

	{ "/File/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/File/_Save Fits As...", "<control>s", file_popup_cb, FILE_SAVE_AS, "<Item>" },
	{ "/File/_Export Image",	NULL, 	NULL, 	0, "<Branch>" },
	{ "/File/_Export Image/_8-bit pnm", NULL, file_popup_cb, FILE_EXPORT_PNM8, "<Item>" },
	{ "/File/sep",		NULL,         	NULL,  		0, "<Separator>" },
//	{ "/File/_Quit",	"<control>Q", 	user_quit_action, 0, "<Item>" },


	{ "/_Image",      	NULL,   	NULL, 		0, "<Branch>" },
	{ "/Image/tear",  	NULL,   	NULL,  		0, "<Tearoff>" },
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
	{ "/Stars/tear",  	NULL,   	NULL,  		0, "<Tearoff>" },
	{ "/Stars/_Detect Sources",  "s", find_stars_cb, ADD_STARS_DETECT, "<Item>" },
//	{ "/Stars/sep",		NULL,         	NULL,  		0, "<Separator>" },
//	{ "/Stars/_Mark Stars", NULL, selection_mode_cb, SEL_ACTION_MARK_STARS, "<Item>" },
//	{ "/Stars/Reset Sel Mode", "Escape", selection_mode_cb, SEL_ACTION_NORMAL, "<Item>" },
//	{ "/Stars/Show _Sources",  NULL,	NULL,  		0, "<CheckItem>" },
	{ "/Stars/sep",		NULL,         	NULL,  		0, "<Separator>" },
	{ "/Stars/Remove Selecte_d",  "<control>d", star_rm_cb, STAR_RM_SEL, "<Item>" },
	{ "/Stars/Remove _Detected Stars", "<shift>s", star_rm_cb, STAR_RM_FR, "<Item>" },
	{ "/Stars/Remove _User Stars",  "<shift>u", star_rm_cb, STAR_RM_USER, "<Item>" },
	{ "/Stars/Remove _All", "<shift>a",	star_rm_cb, STAR_RM_ALL, "<Item>" },
};

gboolean guide_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gtk_object_set_data(GTK_OBJECT(data), "guide_window", NULL);
	return TRUE;
}

/* create the menu bar */
static GtkWidget *get_main_menu_bar(GtkWidget *window)
{
	GtkWidget *ret;
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;
	gint nmenu_items = sizeof (guide_image_menu_items) / 
		sizeof (guide_image_menu_items[0]);
	accel_group = gtk_accel_group_new ();

	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, 
					     "<main_menu>", accel_group);
	gtk_object_set_data_full(GTK_OBJECT(window), "main_menu_if", item_factory,
				 (GtkDestroyNotify) gtk_object_unref);
	gtk_item_factory_create_items (item_factory, nmenu_items, 
				       guide_image_menu_items, window);

  /* Attach the new accelerator group to the window. */
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

    /* Finally, return the actual menu bar created by the item factory. */ 
	ret = gtk_item_factory_get_widget (item_factory, "<main_menu>");
	return ret;
}

/*
 * mouse button event callback. It is normally called after the callback in
 * sourcesdraw has decided not to act on the click
 */
static gboolean image_clicked_cb(GtkWidget *w, GdkEventButton *event, gpointer data)
{
	GSList *found;

//	printf("button press : %f %f state %08x button %08x \n",
//	       event->x, event->y, event->state, event->button);
	if (event->button == 3) {
		show_region_stats(data, event->x, event->y);
/*		found = stars_under_click(GTK_WIDGET(data), event);
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
*/
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

	if (event->is_hint) {
		gdk_window_get_pointer (event->window, &x, &y, &state);
	} else {
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

/* find a guide star from the image */
/* if we have user stars, we use the first of them; otherwise we search for stars */
static void find_guide_star_cb( GtkWidget *widget, gpointer window)
{
	struct image_channel *i_ch;
	double x, y;
	GSList *sl = NULL;
	struct gui_star *gs;
	struct guider *guider;
	struct gui_star_list *gsl;
	int found = 0;

	gsl = gtk_object_get_data(GTK_OBJECT(window), "gui_star_list");
	if (gsl != NULL) 
		remove_stars_of_type(gsl, TYPE_MASK(STAR_TYPE_ALIGN), 0);
	i_ch = gtk_object_get_data(GTK_OBJECT(window), "i_channel");
	if (i_ch == NULL || i_ch->fr == NULL) {
		err_printf("no image\n");
		return;
	}
	if (gsl != NULL) {
		sl = filter_selection(gsl->sl, TYPE_MASK(STAR_TYPE_USEL), 0, 0);
	}
	if (sl != NULL) {
		GUI_STAR(sl->data)->flags = STAR_TYPE_ALIGN;
		gsl->display_mask |= TYPE_MASK(STAR_TYPE_ALIGN);
		gsl->select_mask |= TYPE_MASK(STAR_TYPE_ALIGN);
		x = GUI_STAR(sl->data)->x;
		y = GUI_STAR(sl->data)->y;
		g_slist_free(sl);
		found = 1;
	} else {
		if (!detect_guide_star(i_ch->fr, &x, &y)) {
			info_printf("guide star at %.1f %.1f\n", x, y);
			gs = gui_star_new();
			gs->x = x;
			gs->y = y;
			gs->size = 1.0 * P_INT(DO_DEFAULT_STAR_SZ);
			gs->flags = STAR_TYPE_ALIGN;
			sl = g_slist_prepend(sl, gs);
			add_gui_stars_to_window(window, sl);
			gui_star_release(gs);
			found = 1;
			g_slist_free(sl);
		}
	}
	if (found) {
		guider = gtk_object_get_data(GTK_OBJECT(window), "guider");
		if (guider == NULL) {
			guider = guider_new();
			gtk_object_set_data_full(GTK_OBJECT(window), "guider", 
						 guider, (GtkDestroyNotify)guider_release);
		}
		guider_set_target(guider, i_ch->fr, x, y);
		gtk_widget_queue_draw(window);
	}
}

/*
 * an expose event to the zoomed guide box
 * we only handle the i channel for now
 */
gboolean gbox_expose_cb(GtkWidget *widget, GdkEventExpose *event, gpointer window)
{
	struct map_cache *cache = NULL;
	struct image_channel *i_channel;
	struct guider *guider;
	int s, x, y;
	void *ret;
	
	cache = gtk_object_get_data(GTK_OBJECT(window), "gbox_cache");
	if (cache == NULL) {
		cache = new_map_cache(GUIDE_BOX_SIZE * GUIDE_BOX_SIZE, MAP_CACHE_GRAY);
		gtk_object_set_data_full(GTK_OBJECT(window), "gbox_cache", 
					 cache, (GtkDestroyNotify)release_map_cache);
	}
	ret = gtk_object_get_data(GTK_OBJECT(window), "i_channel");
	if (ret == NULL) /* no channel */
		return TRUE;
	i_channel = ret;
	if (i_channel->fr == NULL) /* no frame */
		return TRUE;
	guider = gtk_object_get_data(GTK_OBJECT(window), "guider");

	if (P_INT(GUIDE_BOX_ZOOM) < 1)
		P_INT(GUIDE_BOX_ZOOM) = 1;
	if (P_INT(GUIDE_BOX_ZOOM) > 16)
		P_INT(GUIDE_BOX_ZOOM) = 16;
	s = GUIDE_BOX_SIZE / P_INT(GUIDE_BOX_ZOOM);
	
	if (guider == NULL) {
		x = i_channel->fr->w / 2;
		y = i_channel->fr->h / 2;
	} else {
		x = (guider->xtgt);
		y = (guider->ytgt);
	}
	cache->cache_valid = 0; /* until we fix all update failures */
	if (!cache->cache_valid) {
		image_box_to_cache(cache, i_channel, P_INT(GUIDE_BOX_ZOOM), 
				   x - s / 2 - 1,
				   y - s / 2 - 1,
				   s+1, s+1);
		cache->x = - ((s / 2 + 2) * P_INT(GUIDE_BOX_ZOOM) - GUIDE_BOX_SIZE / 2);
		cache->y = cache->x;
	}
	paint_from_gray_cache(widget, cache, &(event->area));
	return TRUE;
}


/* create / open the guiding dialog */
void open_guide_cb(gpointer data, guint action, GtkWidget *menu_item)
{
	GtkWidget *window = data;
	GtkWidget *gwindow, *vb, *menubar, *im, *scw;

	gwindow = gtk_object_get_data(GTK_OBJECT(window), "guide_window");
	if (gwindow == NULL) {
		gwindow = create_guide_window();
		gtk_object_set_data_full(GTK_OBJECT(window), "guide_window",
					 gwindow, (GtkDestroyNotify)(gtk_widget_destroy));
		vb = gtk_object_get_data(GTK_OBJECT(gwindow), "guide_vbox");
		menubar = get_main_menu_bar(gwindow);
		gtk_box_pack_start(GTK_BOX(vb), menubar, FALSE, TRUE, 0);
		gtk_box_reorder_child(GTK_BOX(vb), menubar, 0);
		gtk_widget_show(menubar);
		scw = gtk_object_get_data(GTK_OBJECT(gwindow), "scrolled_window");
		im = gtk_object_get_data(GTK_OBJECT(gwindow), "image");
		set_named_callback(gwindow, "image", "expose_event", image_expose_cb);
		set_named_callback(gwindow, "guide_box_darea", "expose_event", gbox_expose_cb);
		gtk_signal_connect(GTK_OBJECT(gwindow), "delete_event", 
				   GTK_SIGNAL_FUNC(guide_window_delete), window);
		gtk_signal_connect(GTK_OBJECT(im), "motion-notify-event", 
				   GTK_SIGNAL_FUNC(motion_event_cb), gwindow);
		gtk_signal_connect(GTK_OBJECT(scw), "button_press_event", 
				   GTK_SIGNAL_FUNC(sources_clicked_cb), gwindow);
		gtk_signal_connect(GTK_OBJECT(scw), "button_press_event", 
				   GTK_SIGNAL_FUNC(image_clicked_cb), gwindow);
		gtk_widget_add_events(im, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK
				      | GDK_POINTER_MOTION_HINT_MASK);
		set_named_callback(gwindow, "guide_find_star", "clicked", find_guide_star_cb);
		im = gtk_object_get_data(GTK_OBJECT(gwindow), "guide_box_darea");
		gtk_drawing_area_size(GTK_DRAWING_AREA(im), GUIDE_BOX_SIZE, GUIDE_BOX_SIZE);

		gtk_widget_show(gwindow);

	} else {
		gdk_window_raise(gwindow->window);
	}

}
