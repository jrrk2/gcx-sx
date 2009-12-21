#ifndef _CAMERAGUI_C_
#define _CAMERAGUI_C_

void goto_dialog_obs(GtkWidget *dialog);
int set_obs_object(GtkWidget *dialog, char *objname);
void center_matched_field(GtkWidget *dialog);
void set_exp_from_img_dialog(struct ccd *ccd, GtkWidget *dialog, void *widget);
void save_frame_auto_name(struct ccd_frame *fr, GtkWidget *dialog);

void test_camera_open(void);
void camera_cb(gpointer data, guint action, GtkWidget *menu_item);

#endif
