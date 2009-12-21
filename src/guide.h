#ifndef _GUIDE_H_
#define _GUIDE_H_

/* a timed value (guide history piece) */
struct timed_double {
	double v;		/* that actual value */
	struct timeval tv;	/* time when we took that value */
};
struct timed_pair {
	double x;		/* that actual value */
	double y;		/* that actual value */
	struct timeval tv;	/* time when we took that value */
};

/* amount of time we hold the guiding history for */
#define GUIDE_HIST_LENGTH 256

/* the guider state */
/* we rotate the history bits in the arrays, so that the first element is always the 
 * most recent */
struct guider {
	int ref_count;
	int state;		/* guider status */
	int perrpoints;		/* the number of error (position) points */
	int movepoints; 	/* the number of guide move points */
	struct timed_pair perr[GUIDE_HIST_LENGTH]; /* position error history */
	struct timed_double perr_err[GUIDE_HIST_LENGTH]; /* position error uncertainty history */
	struct timed_pair move[GUIDE_HIST_LENGTH]; /* move history */
	double xtgt;		/* guide target position */
	double ytgt;		
	double xbias;		/* bias (initial error) values; either centroid bias */
	double ybias;		/* or ratio target values. The value the algorithm seeks 
				 * is tgt+bias */
};

#define GUIDER_TARGET_SET 0x01	/* xtgt and ytgt hold a valid target */

/* guide.c */
int detect_guide_star(struct ccd_frame *fr, double *x, double *y);
struct guider *guider_new(void);
void guider_ref(struct guider *guider);
void guider_release(struct guider *guider);
void guider_set_target(struct guider *guider, struct ccd_frame *fr, 
		       double x, double y);



#endif
