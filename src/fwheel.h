/* filter wheel controller
 */

/* structure describing an open filter wheel */
struct fwheel { 
	int ref_count; /* number of times this has been open */
	int ser;		/* file handle for the control connection */
	int state; 		/* state of wheel controller */
	struct timeval cmdt;	/* time when we started a command */
	int filter; 		/* the current filter (-1 if unknown) */
	char *host; 		/* host part of url */
	char *name; 		/* port name */
	char **fnames;		/* list of filters */
};

#define FWHEEL_READY 0
#define FWHEEL_CMD_ACTIVE 1
#define FWHEEL_ERROR -1

/* function prototypes */
char ** find_fwheels(void);
struct fwheel * fwheel_open(char *url);
int fwheel_close(struct fwheel *fw);
int fwheel_reset(struct fwheel *fw);
int fwheel_get_filter(struct fwheel *fw);
int fwheel_goto_filter(struct fwheel *fw, int filter);
int fwheel_poll_status(struct fwheel *fw);
char ** fwheel_get_filter_names(struct fwheel *fw);

