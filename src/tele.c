
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

// tele.c: telescope control functions
// $Revision: 1.15 $
// $Date: 2004/12/04 00:10:43 $

/* a very poorly written lx200 interface; should be treated in the same way as 
 * the cameras, and piped through tcp, but it's a start */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <math.h>
#include <errno.h>

#include "misc.h"
#include "gcx.h"
#include "params.h"

#define JD2000 2451545.0
//#define	degrad(x)	((x)*PI/180.)
//#define	raddeg(x)	((x)*180./PI)
#define	hrdeg(x)	((x)*15.)
#define	deghr(x)	((x)/15.)
#define	hrrad(x)	degrad(hrdeg(x))
#define	radhr(x)	deghr(raddeg(x))

#define PREC_HI 1 /* high prec mode */
#define PREC_LO 0 /* low prec mode */

#define LOSMANDY

#ifdef LOSMANDY
#define LX_PREC_MODE PREC_HI /* the mode we want to use */
#define SLEW_FINISH_TIME 1000 /* we wait 2 seconds from the time the scope coordinates
			       * have reached their target before we declare the slew over
			       */
#define SLEW_END_TOL (0.2) /* how close we need to be from the target point to 
			      decide the slew ended (and begin end-of-slew delay) */
#define STABILISATION_DELAY 3000 /* time we wait at the end of the slew for the 
				    mount to stabilise */
#else
#define LX_PREC_MODE PREC_LO /* the mode we want to use */
#define SLEW_FINISH_TIME 5000 /* we wait 5 seconds from the time the scope coordinates
			       * have reached their target before we declare the slew over
			       */
#define SLEW_END_TOL (1) /* how close we need to be from the target point to 
			    decide the slew ended (and begin end-of-slew delay) */
#define STABILISATION_DELAY 0 /* time we wait at the end of the slew for the 
				    mount to stabilise */
#endif

static double lx_epoch = 2000.0;
static double last_ra = 0.0;
static double last_dec = 0.0;
static double target_ra = 0.0;
static double target_dec = 0.0;
static double last_epoch = 2000.0;

static int slew_aborted = 0;

static double ra_err = -1.0;
static double dec_err = -1.0;

static int err_cnt = 0;

#define SLEW_TIMEOUT 120000 /* max duration of a slew (ms): 2 minutes */
#define LX_SERIAL_TIMEOUT 1000000 /* this one is in us */

static struct timeval slew_start_time;
static struct timeval slew_end_time;

static int lx_response(int fd, char *buf, int size, char **patterns);



// return the ms difference between t1 and t2 (positive if t1 is earlier than t2)
/*
static int time_diff(struct timeval *t1, struct timeval *t2)
{
	int deltat;
	deltat = t2->tv_sec - t1->tv_sec;
	deltat *= 1000;
	deltat += (t2->tv_usec - t2->tv_usec) / 1000;
	return deltat;
}
*/

void lx200_set_epoch(double epo)
{
	lx_epoch = epo;
}

struct termios tio;
static int ser;

#define MAX_RETRY 10000

int sync_time(void)
{
	int fd;
	char rbuf[256];
	int rp;
	int i, ret;
	int time;

	fd = open(P_STR(FILE_GPS_SERIAL), O_RDWR, O_NONBLOCK);
	tcgetattr(fd, &tio);
	cfsetospeed(&tio, B4800);
	cfsetispeed(&tio, B4800);
	cfmakeraw(&tio);
	tcsetattr(fd, TCSANOW, &tio);

	rp = 0;
	rbuf[255] = 0;
	for (i=0; i<MAX_RETRY; i++) {
		if (read(fd, rbuf, 1) == 1) {
//			d3_printf("Got: %c\n", *(rbuf));
			if (*(rbuf) == '\n') {//end of line
				rp = 0;
				break;
			}
			rp ++;
			if (rp > 250) {
//				err_printf("sync_time: line too long\n");
				close(fd);
				return -1;
			}
		}
	}
	rp = 0;
	for (i=0; i<MAX_RETRY; i++) {
		if (read(fd, rbuf+rp, 1) == 1) {
//			d3_printf("2l:Got: %c (%d)\n", *(rbuf+rp), *(rbuf+rp));
			if (*(rbuf+rp) == '\n') {//end of line
				rp = 0;
//				err_printf("bad line :%s\n");
				close(fd);
				return -1;
			}
			rp ++;
			if (rp > 250) {
				err_printf("sync_time: line too long\n");
				close(fd);
				return -1;
			}
			if (rp > 14 && (ret = sscanf(rbuf, "$GPRMC,%d,", &time)) == 1) {
				rbuf[15] = 0;
				d3_printf("timeline: %s\n", rbuf);
				close(fd);
				return (time);
			}
		}
	}	
	err_printf("Timeout in receive\n");
	close(fd);
	return -1;
}

void open_serial(void)
{
	ser = open(P_STR(FILE_SCOPE_SERIAL), O_RDWR);
	if (ser <= 0) {
		err_printf("serial open error\n");
	}
//	d3_printf("ser opened: %d\n", ser);
	tcgetattr(ser, &tio);
	cfsetospeed(&tio, B9600);
	cfsetispeed(&tio, B9600);
	cfmakeraw(&tio);
	tcsetattr(ser, TCSANOW, &tio);
}

void close_serial(void)
{
//	d3_printf("ser closing (%d)\n", ser);
	close(ser);
}



void sersend(char *buf)
{
//	open_serial();
	d4_printf(">>> %s \n", buf);
	write(ser, buf, strlen(buf));
//	close_serial();
}




//dx is the move in E direction (neg for W), dy the move in N (neg for S)
//value is in ms

#define TIMED_MOVE_GUIDE 0
#define TIMED_MOVE_CENTERING 1

void timed_move(int dx, int dy, int type)
{
	struct timeval tv;
	int td;

	d3_printf("Timed move: %d %d [%d]\n", dx, dy, type);
	usleep(10000);
	open_serial();
	if (type == TIMED_MOVE_CENTERING) {
		sersend(":RC#");
	} else {
		sersend(":RG#");
	}
	update_timer(&tv);

	if (dx) {
		if (dx > 0) {	
			sersend(":Me#");
		} else {
			sersend(":Mw#");
		}
	}
	if (dy) {
		if (dy > 0) {	
			sersend(":Mn#");
		} else {
			sersend(":Ms#");
		}
	}
	while(1) {
		td = get_timer_delta(&tv);
		if ((((fabs(dx) - td) > 20) || dx == 0) 
		    && ((((fabs(dy) - td) > 20)) || dy == 0))
			usleep(10000);
		if ((dx != 0) && (td > fabs (dx))) { // stop ra movement 
			if (dx > 0) {	
				sersend(":Qe#");
			} else {
				sersend(":Qw#");
			}
			dx = 0;
			if (dy == 0) 
				break;
		}
		if ((dy != 0) && (td > fabs (dy))) { // stop ra movement 
			if (dy > 0) {	
				sersend(":Qn#");
			} else {
				sersend(":Qs#");
			}
			dy = 0;
			if (dx == 0) 
				break;
		}
	}
	close_serial();
}

void lx200_guide_move(double dx, double dy)
{
	timed_move(dx, dy, TIMED_MOVE_GUIDE);
}

/* change position by dra, ddec (degrees) using
 * a centering move */
void lx200_centering_move(double dra, double ddec)
{
	int dx, dy;
#ifdef LOSMANDY // reverses dec moves depending on hemisphere
	double ra, dec, dec1;
	static int decsign = 1.0;
#endif

	dx = dra * 3600.0 * 1000.0 / 15.0 / P_DBL(TELE_CENTERING_SPEED);
	dy = ddec * 3600.0 * 1000.0 / 15.0 / P_DBL(TELE_CENTERING_SPEED);

	if (clamp_int(&dx, -5000, 5000))
		err_printf("long move, clipped at 5sec\n");
	if (clamp_int(&dy, -5000, 5000))
		err_printf("long move, clipped at 5sec\n");

#ifdef LOSMANDY
	lx200_get_position(&ra, &dec, NULL);
	timed_move(dx, decsign * dy, TIMED_MOVE_CENTERING);
	lx200_get_position(&ra, &dec1, NULL);
	if (dy * (dec1 - dec) < 0) {
		d1_printf("dec reversal\n");
		decsign *= -1;
		timed_move(0, 2 * decsign * dy, TIMED_MOVE_CENTERING);
	}
#else
	timed_move(dx, dy, TIMED_MOVE_CENTERING);
#endif
}


// transform degrees into lx200 declination format
static void dec_to_dms(char *lb, double deg)
{
	double d1, d2;
	if (deg >= 0.0) {
		deg += 1.0 / (120.0); //add half a minute for correct rounding
		d1 = floor (deg);
		deg = (deg - d1) * 60.0;
		d2 = floor (deg);
		deg = ((deg - d2) * 60.0);
		sprintf(lb, "+%02d:%02d", (int)d1,  
			(int)d2);
//		sprintf(lb, "-%02d%c%02d'%02d", (int)d1, 223,
//			(int)d2, (int)floor(deg));
	} else {
		deg = -deg;
		deg += 1.0 / (120.0); //add half a minute for correct rounding
		d1 = floor (deg);
		deg = (deg - d1) * 60.0;
		d2 = floor (deg);
		deg = ((deg - d2) * 60.0);
		sprintf(lb, "-%02d:%02d", (int)d1,  
			(int)d2);
//		sprintf(lb, "-%02d%c%02d'%02d", (int)d1, 223,
//			(int)d2, (int)floor(deg));
	}
}

// transform degrees into lx200 ra format
static void ra_to_hms(char *lb, double deg)
{
	double d1, d2;
	deg /= 15.0;
	deg += 1.0 / (60.0 * 20.0); //add half a tenth of a minute for correct rounding
	if (deg >= 0.0) {
		d1 = floor (deg);
		deg = (deg - d1) * 60.0;
		d2 = floor (deg);
		deg = ((deg - d2) * 10.0);
		sprintf(lb, "%02d:%02d.%1d", (int)d1, 
			(int)d2, (int)floor(deg));
	} else {
		err_printf("bad RA (< 0), assuming positive\n");
		deg = -deg;
		d1 = floor (deg);
		deg = (deg - d1) * 60.0;
		d2 = floor (deg);
		deg = ((deg - d2) * 10.0);
		sprintf(lb, "%02d:%02d.%1d", (int)d1, 
			(int)d2, (int)floor(deg));
	}
}

/* get the current scope position (just return the object set for now)
 * return 0 if successfull */
int lx200_get_target(double *ra, double *dec, double *epoch)
{
	*ra = last_ra;
	*dec = last_dec;
	*epoch = last_epoch;
	return 0;
}

static int lx200_set_prec(int mode);
static char *resp_zero_one[] = {"0", "1", NULL};

/* low level setting function - just send the ra and dec, no precession
   or ofsets */
static void lx200_object(double ra, double dec, char *name)
{
	char lb[64];
	char out[64];
	char buf[64];


#ifdef LOSMANDY
	lx200_set_prec(LX_PREC_MODE);
#endif
	open_serial();

	if (LX_PREC_MODE == PREC_LO)
		ra_to_hms(lb, ra);
	else
		degrees_to_dms_pr(lb, ra / 15.0, 0);

	sprintf(out, ":Sr%s#", lb);
	sersend(out);
	lx_response(ser, buf, 64, resp_zero_one);


#ifdef LOSMANDY
	if (name != NULL) {
		snprintf(out, 63, ":ON%s#", name);
		sersend(out);
	} else {
		sersend(":ONobj#");
	}
#endif


	if (LX_PREC_MODE == PREC_LO)
		dec_to_dms(lb, dec);
	else
		degrees_to_dms_pr(lb, dec, 0);

	sprintf(out, ":Sd%s#", lb);
	sersend(out);
	lx_response(ser, buf, 64, resp_zero_one);


	close_serial();
	target_ra = ra;
	target_dec = dec;
	update_timer(&slew_start_time);
	update_timer(&slew_end_time);

}

// send the 'obs' object coordinates to the telescope
int lx200_set_object(double ra, double dec, double epoch, char *name)
{
	char lb[64];
	int i, j, k, jdi;

	struct timeval tv;
	struct tm *t;

	gettimeofday(&tv, NULL);
	t = gmtime(&(tv.tv_sec));

	last_ra = ra;
	last_dec = dec;
	last_epoch = epoch;

// do the julian date (per hsaa p107)
	i = 1900 + t->tm_year;
	j = t->tm_mon + 1;
	k = t->tm_mday;

	jdi = k - 32075 + 1461 * (i + 4800 + (j - 14) / 12) / 4
		+ 367 * (j - 2 - (j - 14) / 12 * 12) / 12
		- 3 * (( i + 4900 + (j - 14) / 12) / 100) / 4;

	if (P_INT(TELE_PRECESS_TO_EOD))
		precess_hiprec(epoch, (jdi - JD2000) / 365.25 + 2000.0, &ra, &dec);

	if (LX_PREC_MODE == PREC_LO)
		ra_to_hms(lb, ra);
	else
		degrees_to_dms_pr(lb, ra / 15.0, 0);
	d3_printf("RA'%.2f %s", 2000 + (jdi - JD2000) / 365.25, lb);

	if (LX_PREC_MODE == PREC_LO)
		dec_to_dms(lb, dec);
	else
		degrees_to_dms_pr(lb, dec, 0);

	d3_printf(" DEC'%.2f %s -> scope\n", 2000 + (jdi - JD2000) / 365.25, lb);


	lx200_object(ra, dec, name);
	return 0;
}

static char *resp_precision[] = {"LOW  PRECISION", "HIGH PRECISION", NULL};

/* set the precision mode, retun 0 if successful */
static int lx200_set_prec(int mode)
{
	char buf[256];
	int ret;

	buf[0] = 0;
	buf[255] = 0;
	open_serial();
	sersend(":P#");
	ret = lx_response(ser, buf, 256, resp_precision);
	if (ret == 1) {
		d3_printf("current precision is low\n");
		if (mode == PREC_HI)
			sersend(":U#");
		return 0;
	} else if (ret == 2) {
		d3_printf("current precision is high\n");
		if (mode == PREC_LO)
			sersend(":U#");
		return 0;
	} else {
		d3_printf("can't understand response to :P# [%s]\n", buf);
	}
	return -1;
}


// start a slew to the selected object
void lx200_start_slew(void)
{
	char buf[256];

	buf[0] = 0;
	open_serial();
	sersend(":MS#");
	slew_aborted = 0;
	ra_err = -1.0;
	dec_err = -1.0;
	err_cnt = 0;
//	ret = lx200_read(ser, buf, 256);
//	d3_printf("got %s\n", buf);
	close_serial();
}

// abort current slew
void lx200_abort_slew(void)
{
	open_serial();
	sersend(":Q#");
	slew_aborted = 1;
	close_serial();
}

// sync to current coordinates
static int lx200_sync(void)
{
	char buf[64];

	d3_printf("lx Sync\n");
	open_serial();
	usleep(100000);
	sersend(":CM#");
	lx_response(ser, buf, 64, NULL);
	close_serial();
	usleep(100000);
	return 0;
}

// sync to current coordinates
static int lx200_align(void)
{
	char buf[64];

	open_serial();

#ifdef LOSMANDY
	sersend(":Cm#");
#else
	sersend(":CM#");
#endif
	lx_response(ser, buf, 64, NULL);
	close_serial();
	usleep(100000);
	return 0;
}


int lx200_sync_coords(double ra, double dec, double epoch)
{
	int ret;
	if (epoch == 0.0)
		epoch = 2000.0;

	ret = lx200_set_object(ra, dec, epoch, "sobj");
	if (ret)
		return ret;
	ret = lx200_sync();
	return ret;
}

int lx200_align_coords(double ra, double dec, double epoch)
{
	int ret;
	if (epoch == 0.0)
		epoch = 2000.0;

	ret = lx200_set_object(ra, dec, epoch, "aobj");
	if (ret)
		return ret;
	ret = lx200_align();
	return ret;
}


/* do one select and one read from the given file */
static int timeout_read(int fd, char * resp, int resp_size, int timeout)
{
	struct timeval tmo;
	int ret;
	fd_set fds;

	if (fd <= 0)
		return -1;

	tmo.tv_sec = 0;
	tmo.tv_usec = timeout;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	ret = select(FD_SETSIZE, &fds, NULL, NULL, &tmo);
	if (ret == 0) {
		err_printf("lx_read: timeout in receive\n");
		return -1;
	}
	if (ret == -1 && errno) {
		err_printf("lx_read: select error (%s)\n", strerror(errno));
		return -1;
	}
	ret = read(fd, resp, resp_size);
//	d3_printf("read returns %d, val=%d\n", ret, *resp);
	if (ret == -1 && errno != 0) {
		err_printf("lx_read: error in receive [%s]\n", strerror(errno));
		return -1;
	}
	return ret;
}


/* return the index to the matched pattern (starting at 1), 0 if a # was found, or -1 if we 
 * reached timeout without matching anything */
static int lx_response(int fd, char *buf, int size, char **patterns)
{
	int ret;
	char **p;
	char *bp = buf;
	int i;

	while (size > 2) {
		ret = timeout_read(fd, bp, size - 1, LX_SERIAL_TIMEOUT);
		if (ret < 0) {
			*bp = 0;
			d4_printf("lx response timeout: [%s]\n", buf);
			return -1;
		} else {
			bp += ret;
			*bp = 0;
			i = 1;
			p = patterns;
			if (*(bp - 1) == '#') {
				d4_printf("lx response: [%s]\n", buf);
				return 0;
			}
			while ((p != NULL) && (*p != NULL)) {
				if (!strcasecmp(buf, *p)) {
					d4_printf("lx response: [%s] (%d)\n", buf, i);
					return i;
				}
				p++;
				i++;
			}
			size -= ret;
		} 
	}
	*bp = 0;
	return -1;
}




/* read the position from scope. epoch is the epoch last set (usually 2000) 
 * we assume all coordinates are precessed to it 
 * returns 0 if the read was successfull, -1 for an error */
int lx200_get_position(double *ra, double *dec, double *epoch)
{
	char buf[256];
	int ret;
	double lra, ldec;

	open_serial();
	sersend(":GR#");
	ret = lx_response(ser, buf, 256, NULL);
	if (ret < 0) {
		err_printf("Timeout talking to scope\n");
		goto err_close;
	}
//	d4_printf("<<< %s\n", buf);
	ret = dms_to_degrees(buf, &lra);
	if (ret) {
		err_printf("Error parsing ra from scope\n");
		goto err_close;
	}
	lra *= 15.0;
#ifdef LOSMANDY
	usleep(100000); /* we need this, or the mount will overshoot it's target! */
#endif
	sersend(":GD#");
	ret = lx_response(ser, buf, 256, NULL);
	if (ret < 0) {
		err_printf("Timeout talking to scope\n");
		goto err_close;
	}
	d4_printf("<<< %s\n", buf);
	ret = dms_to_degrees(buf, &ldec);
	if (ret) {
		err_printf("Error parsing dec from scope\n");
		goto err_close;
	}
	if (ra != NULL)
		*ra = lra; 
	if (dec != NULL)
		*dec = ldec;
	if (epoch != NULL)
		*epoch = lx_epoch;
	close_serial();
	return 0;

err_close:
	close_serial();
	return -1;
}

/* 1/cos(dec), clamped at 5 */
static double dec_factor(double dec)
{
	if (cos(dec) > 0.2)
		return (1.0 / cos(dec));
	else
		return 5.0;

}

/* check if the slew has finished. Return 0 if finished, -1 if we can;t talk to the scope 
 * or 1 if slew is in progress. In the latter case, update dra and ddec with the amount 
 * of movement still needed; -2 after SLEW_TIMEOUT or abort */
int lx200_poll_slew_status(double *dra, double *ddec)
{
	double ra, dec, dr, dd;
	int ret;

	if (get_timer_delta (&slew_start_time) > SLEW_TIMEOUT) {
		err_printf("Slew timed out\n");
		return -2;
	}

	if (slew_aborted) {
		err_printf("Slew aborted\n");
		return -2;
	}

	ret = lx200_get_position(&ra, &dec, NULL);
	if (ret != 0) 
		return -1;

	d4_printf("+++ %.3f, %.3f\n", ra, dec);

	dr = fabs(angular_dist(ra, target_ra)) / dec_factor(target_dec);
	dd = fabs(angular_dist(dec, target_dec));
	if ((dr > SLEW_END_TOL) || (dd > SLEW_END_TOL)) {
		update_timer(&slew_end_time);
	}

	if (P_INT(TELE_ABORT_FLIP)) {
		if ((ra_err > 0) && (ra_err < dr) && (dr > 3.0)) {
			err_cnt ++;
		}
		if ((dec_err > 0) && (dec_err < dd) && (dd > 3.0)) {
			err_cnt ++;
		}
	}
	if (err_cnt > 8) {
		lx200_abort_slew();
		err_printf("Distance increasing (meridian flip?)\n");
		return -2;
	}

	ra_err = dr;
	dec_err = dd;

	if (dra != NULL) {
		*dra = angular_dist(ra, target_ra);
	}
	if (ddec != NULL) {
		*ddec = angular_dist(dec, target_dec);
	}

	if (get_timer_delta (&slew_end_time) > SLEW_FINISH_TIME) {
#ifdef LOSMANDY
		/* take out gear play */
		if (P_DBL(TELE_GEAR_PLAY) > 0.001) {
			lx200_centering_move(P_DBL(TELE_GEAR_PLAY), P_DBL(TELE_GEAR_PLAY));
			lx200_centering_move(-P_DBL(TELE_GEAR_PLAY), -P_DBL(TELE_GEAR_PLAY));
		}
#endif
		err_printf("Slew finished\n");
		return 0;
	}
	return 1;
}
