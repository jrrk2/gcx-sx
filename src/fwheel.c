
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

/* filterc: telescope control functions
 */

/* filter wheel control functions - useful for a directly-attached 
   serial filter wheel */

#define _GNU_SOURCE

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
#include "fwheel.h"

#define FWHELL_COM_TIMEOUT 20 /* communications timeout for fwheel (ms) */
#define FWHEEL_OP_TIMEOUT 5000 /* filter wheel operations timeout (ms) */

#define MAX_FILTERS 4		/* max nr of filters we support */

static int ser; 		/* file handle for the serial port */
struct fwheel deffw;
static char *wheels[2]={NULL, NULL};
/* helper functions */

/* open and configure filter wheel serial port; return 0 if successfull */
static int open_wheel_serial(struct fwheel *fw)
{
	struct termios tio;
	if (fw->ser > 0) {
		return 0;
	}
	ser = open(fw->name, O_RDWR);
	if (ser <= 0) {
		err_printf("%s open error: %s\n", fw->name, 
			   strerror(errno));
		return -1;
	}
	tcgetattr(ser, &tio);
	cfsetospeed(&tio, B9600);
	cfsetispeed(&tio, B9600);
	cfmakeraw(&tio);
	tcsetattr(ser, TCSANOW, &tio);
	fw->ser = ser;
	return 0;
}

static void close_wheel_serial(struct fwheel *fw)
{
	if (fw->ser > 0) {
		close(fw->ser);
		fw->ser = 0;
	}
}

/* do one select and one read from the given file */
static int timeout_read(int fd, char * resp, int resp_size, int timeout)
{
	struct timeval tmo;
	int ret;
	fd_set fds;

	tmo.tv_sec = 0;
	tmo.tv_usec = timeout;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	ret = select(FD_SETSIZE, &fds, NULL, NULL, &tmo);
	if (ret == 0) {
		d3_printf("fw read timeout\n");
		return -1;
	}
	if (ret == -1 && errno) {
		err_printf("fw timeout_read: select error (%s)\n", strerror(errno));
		return -1;
	}
	ret = read(fd, resp, resp_size);
//	d3_printf("read returns %d, val=%d\n", ret, *resp);
	if (ret <= 0 && errno != 0) {
		err_printf("fw timeout_read: error in receive [%s]\n", strerror(errno));
		return -1;
	}
	return ret;
}

/* return 1 if we have data to receive, 0 if not, -1 for an error */
static int have_rx_data(int fd)
{
	struct timeval tmo;
	int ret;
	fd_set fds;

	tmo.tv_sec = 0;
	tmo.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	ret = select(FD_SETSIZE, &fds, NULL, NULL, &tmo);
	if (ret == -1 && errno) {
		err_printf("timeout_read: select error (%s)\n", strerror(errno));
		return -1;
	}
	return ret;
}

/* read a full response line from the wheel */
static int wheel_response(int fd, char *resp, int size, int timeout)
{
	int sz = 0;
	int ret;
	while (sz < size) {
		ret = timeout_read(fd, resp+sz, 1, timeout);
		if (ret <= 0)
			return ret;
		d4_printf("got %d\n", resp[sz]);
		if (resp[sz] == '\n') {
			resp[sz] = 0;
			return sz;
		}
		if (resp[sz] < ' ') 
			continue;
		sz ++;
	}
	resp[size-1] = 0;
	return sz;
}

/* write to the given file observing the timeout. If all the data cannot be sent
 * due to the timeout, the number of bytes sent is returned. if an error occurs, 
 * -1 is returned */
static int timeout_write(int fd, void *data, int size, int timeout)
{
	struct timeval tmo;
	int sent = 0;
	int ret;
	fd_set fds;

	while (sent < size) {
		tmo.tv_sec = 0;
		tmo.tv_usec = timeout;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		ret = select(FD_SETSIZE, NULL, &fds, NULL, &tmo);
		if (ret == 0) {
			return sent;
		}
		if (ret == -1 && errno) {
			err_printf("timeout_write: select error (%s)\n", strerror(errno));
			return -1;
		}
		ret = write(fd, data + sent, size - sent);
		if (ret == -1 && errno != 0) {
			err_printf("timeout_write: error in write [%s]\n", strerror(errno));
			return -1;
		}
		sent += ret;
	}
	return sent;
}

/* extract the protocol+hostname and camera name from an URL
 * in newly alloced strings
 * return 0 if successfull, -1 in case of error
 */
static int parse_url(char *url, char *host[], char *name[])
{
	int i, ni, hl;

	i=0;
	while (url[i] && url[i++] != ':') ;
	if (url[i] == 0)
		return -1;
	if (url[i++] != '/')
		return -1;
	if (url[i++] != '/')
		return -1;
	while (url[i] && url[i] != '/') 
		i++;
	if (url[i] == 0)
		return -1;
	hl = i;
	ni = i++;
	while (url[i]) 
		i++;
	if (i == ni)
		return -1;
	*host = lstrndup(url, hl);
	if (*host == NULL) 
		return -1;
	*name = lstrndup(url+ni+1, i-ni);
	if (*name == NULL) {
		free(*host);
		*host = NULL;
		return -1;
	}
	return 0;
}

static char **get_filter_names(void)
{
	char **fnames;
	int i;
	char *p;
	char *text, *start, *end;
	int token;

	fnames = calloc(1,1024);
	i = 0;
	p = (void *)fnames + sizeof(char *) * (1 + MAX_FILTERS);
	text = P_STR(OBS_FILTER_LIST);
	while (*text != 0 && i < MAX_FILTERS) {
		token = next_token(&text, &start, &end);
		if (token == TOK_WORD || token == TOK_STRING || token == TOK_NUMBER) {
			if (end > start) {
				fnames[i] = p;
				strncpy(p, start, end-start);
				p += end - start + 1;
			}
		}
		i++;
	}
	fnames[i] = NULL;
	return fnames;
}



/* the filter wheel functions */

/* open a filter wheel; we just ignore the url and open the default one
 * for the time being  */

/* return a list of filter wheel urls we have access to */
char ** find_fwheels(void)
{
	char *url;
	asprintf(&url, "fwheel://localhost%s", 
		 P_STR(FILE_FWHEEL_SERIAL));
	wheels[0] = url;
	wheels[1] = 0;
	return wheels;
}

/* talk to the wheel (and return an error if it can't) */
static int fwheel_talk(struct fwheel *fw)
{
	int ret;
	char resp[16];
	if (fw == NULL)
		return -1;
	if ((ret = open_wheel_serial(fw))) {
		return ret;
	}
	ret = timeout_write(fw->ser, "filter\r\n", 8, 1000 * FWHELL_COM_TIMEOUT);
	if (ret != 8) {
		close_wheel_serial(fw);
		return -1;
	}
	ret = wheel_response(fw->ser, resp, 16, 1000 * FWHELL_COM_TIMEOUT);
	if (ret < 0) 
		return -1;
	return 0;
}

struct fwheel * fwheel_open(char *url)
{
	char *host, *name;
	if (deffw.ref_count > 0) {
		err_printf("filter wheel %s already open\n");
		return NULL;
	}
	if (parse_url(url, &host, &name)) {
		err_printf("filter wheel url parse error\n");
		return NULL;
	}
	d3_printf("fwheel host is %s\n", host);
	d3_printf("fwheel port is %s\n", name);
	deffw.host = host;
	asprintf(&deffw.name,"/%s", name);
	free(name);
	deffw.state = FWHEEL_READY;
	deffw.ser = 0;
	deffw.ref_count = 1;
	if (fwheel_talk(&deffw) < 0) {
		fwheel_close(&deffw);
		return NULL;
	}
	deffw.filter = fwheel_get_filter(&deffw);
	if (deffw.filter < 0) {
		fwheel_close(&deffw);
		return NULL;
	}
	deffw.fnames = get_filter_names();
	return &deffw;
}

int fwheel_close(struct fwheel *fw)
{
	if (fw == NULL)
		return -1;
	close_wheel_serial(fw);
	if (fw->host) {
		free(fw->host);
		fw->host = NULL;
	}
	if (fw->name) {
		free(fw->name);
		fw->name = NULL;
	}
	if (fw->fnames) {
		free(fw->fnames);
		fw->fnames = NULL;
	}
	fw->ref_count = 0;
	fw->filter = -1;
	return 0;
}

int fwheel_reset(struct fwheel *fw)
{
	return -1;
}

/* get the current filter number from the wheel. return -1 if an error occurs */
int fwheel_get_filter(struct fwheel *fw)
{
	int ret;
	char resp[16];
	char *endp;
	if (fw == NULL)
		return -1;
	if (fw->state == FWHEEL_CMD_ACTIVE) {
		err_printf("wheel not ready for a filter change\n");
		return -1;
	}
	if ((ret = open_wheel_serial(fw))) {
		return ret;
	}
	ret = timeout_write(fw->ser, "filter\r\n", 8, 1000 * FWHELL_COM_TIMEOUT);
	d4_printf("fwheel write returns %d\n", ret);
	if (ret != 8) {
//		close_wheel_serial(fw);
		return -1;
	}
	ret = wheel_response(fw->ser, resp, 16, 1000 * FWHELL_COM_TIMEOUT);
//	close_wheel_serial(fw);
	if (ret < 0) 
		return -1;
	ret = strtol(resp, &endp, 10);
	if (ret == 0) {
		err_printf("bad response from wheel: %s\n", resp);
	}
	if (ret <= 0)
		fw->filter = -1;
	else
		fw->filter = ret - 1;
	return ret - 1;
}




/* initiate a filter change. One should call fwheel_poll_status
 * to check when the change is complete. Returns 0 is the command was successfull, 
 * -1 for an error */
int fwheel_goto_filter(struct fwheel *fw, int filter)
{
	int ret;
	char cmd[16];
	if (fw == NULL)
		return -1;
	if (fw->state == FWHEEL_CMD_ACTIVE ||
	    fw->state == FWHEEL_ERROR) {
		err_printf("wheel not ready for a filter change\n");
		return -1;
	}
	if ((ret = open_wheel_serial(fw))) {
		return ret;
	}
	snprintf(cmd, 16, "filter %d\r\n", filter + 1);
	ret = timeout_write(fw->ser, cmd, strlen(cmd), 1000 * FWHELL_COM_TIMEOUT);
	if (ret != strlen(cmd))
		return -1;
	fw->state = FWHEEL_CMD_ACTIVE;
	fw->filter = filter;
	update_timer(&fw->cmdt);
	return 0;
}

int fwheel_poll_status(struct fwheel *fw)
{
	int ret;
	char resp[256];
	if (fw == NULL)
		return -1;
	if (fw->state != FWHEEL_CMD_ACTIVE) {
		return fw->state;
	}
	ret = have_rx_data(fw->ser);
	if (get_timer_delta(&fw->cmdt) > FWHEEL_OP_TIMEOUT) {
		err_printf("filter wheel operation timeout\n");
		fw->state = FWHEEL_ERROR;
		return fw->state;
	}
	if (ret == 0)
		return fw->state;
	if (ret < 0) {
		fw->state = FWHEEL_ERROR;
		return fw->state;
	}
	ret = wheel_response(fw->ser, resp, 250, 1000 * FWHELL_COM_TIMEOUT);
	if (ret <= 0) {
		fw->state = FWHEEL_ERROR;
		return fw->state;
	}
	if (ret > 1 && resp[0] == 'o' && resp[1] == 'k') {
		fw->state = FWHEEL_READY;
//		close_wheel_serial(fw);
		return fw->state;
	} else {
		err_printf("err response from wheel: %s\n", resp);
		fw->state = FWHEEL_ERROR;
		return fw->state;
	}
}

char ** fwheel_get_filter_names(struct fwheel *fw)
{
	return fw->fnames;
}
