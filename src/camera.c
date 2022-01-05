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

// camera image readout interface control functions
// $Revision: 1.8 $
// $Date: 2004/12/04 00:10:43 $

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <netdb.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <jpeglib.h>
#include <usb.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include <dirent.h>
#include <string.h>
#include <time.h>

#include "gcx.h"
#include "camera.h"
#include "params.h"
#include "obsdata.h"
#include "misc.h"

#define CAM_OPEN 0x100

#define MAX_CONNECTIONS 2
#define MAX_OPEN_CCD 4
#define MAX_OPENS 1 /* how many times we can open the same ccd */
#define MAX_CONNECTS 3 /* now many open ccds can use the same connection */

#define CX_PORT 5555

#define CX_DEFAULT_TIMEOUT 1000000

#define FIELD_OFFSET(type, field) ((unsigned long)&(((type *)0)->field))

#define	MT_NULL 0
#define	MT_INT 1
#define	MT_FLOAT 2
#define	MT_DOUBLE 3 
#define	MT_STRING 4

#define MT_WRITABLE 0x10
#define MT_TYPE_MASK 0x0f

struct member {
	char *name;
	int type;
	int level;
	unsigned int offset;
};

#define CAM_INFO_MEMBERS 13
struct member cam_info_members[] = {
	{"active_pix", MT_INT, 1, FIELD_OFFSET(struct cam_info, active_pixels)},
	{"active_lines", MT_INT, 1, FIELD_OFFSET(struct cam_info, active_lines)},
	{"dummy_pix_start", MT_INT, 1, FIELD_OFFSET(struct cam_info, dummy_pix_start)},
	{"dummy_pix_end", MT_INT, 1, FIELD_OFFSET(struct cam_info, dummy_pix_end)},
	{"dummy_lines_start", MT_INT, 1, FIELD_OFFSET(struct cam_info, dummy_lines_start)},
	{"dummy_lines_end", MT_INT, 1, FIELD_OFFSET(struct cam_info, dummy_lines_end)},
	{"pix_x_size", MT_DOUBLE, 1, FIELD_OFFSET(struct cam_info, pix_x_size)},
	{"pix_y_size", MT_DOUBLE, 1, FIELD_OFFSET(struct cam_info, pix_y_size)},
	{"min_exp", MT_DOUBLE, 1, FIELD_OFFSET(struct cam_info, min_exp)},
	{"max_exp", MT_DOUBLE, 1, FIELD_OFFSET(struct cam_info, max_exp)},
	{"scale", MT_DOUBLE, 1, FIELD_OFFSET(struct cam_info, scale)},
	{"rdnoise", MT_DOUBLE, 1, FIELD_OFFSET(struct cam_info, rdnoise)},
	{"name", MT_STRING, 1, FIELD_OFFSET(struct cam_info, name)},
	{NULL, MT_NULL, 0, 0}
};

#define CAM_EXP_MEMBERS 8
struct member cam_exp_members[] = {
	{"width", MT_INT | MT_WRITABLE, 1, FIELD_OFFSET(struct exposure, w)},
	{"height", MT_INT | MT_WRITABLE, 1, FIELD_OFFSET(struct exposure, h)},
	{"skip_x", MT_INT | MT_WRITABLE, 1, FIELD_OFFSET(struct exposure, x_skip)},
	{"skip_y", MT_INT | MT_WRITABLE, 1, FIELD_OFFSET(struct exposure, y_skip)},
	{"bin_x", MT_INT | MT_WRITABLE, 1, FIELD_OFFSET(struct exposure, bin_x)},
	{"bin_y", MT_INT | MT_WRITABLE, 1, FIELD_OFFSET(struct exposure, bin_y)},
	{"exptime", MT_DOUBLE | MT_WRITABLE, 1, FIELD_OFFSET(struct exposure, exp_time)},
	{"jdate", MT_DOUBLE, 1, FIELD_OFFSET(struct exposure, jdate)},
	{NULL, MT_NULL, 0, 0}
};

#define COOLER_MEMBERS 3
struct member cooler_members[] = {
	{"temp_set", MT_DOUBLE | MT_WRITABLE, 1, FIELD_OFFSET(struct cooler, set_temp)},
	{"temp", MT_DOUBLE, 1, FIELD_OFFSET(struct cooler, temp)},
	{"power", MT_DOUBLE, 1, FIELD_OFFSET(struct cooler, cooling_power)},
	{NULL, MT_NULL, 0, 0}
};

#define CAM_STATUS_MEMBERS 3
struct member cam_status_members[] = {
	{"status", MT_INT, 1, FIELD_OFFSET(struct cam_status, state)},
	{"lines_read", MT_INT, 1, FIELD_OFFSET(struct cam_status, linesread)},
	{"exp_left", MT_DOUBLE, 1, FIELD_OFFSET(struct cam_status, expleft)},
	{NULL, MT_NULL, 0, 0}
};

#define SX_VENDOR_ID	0x1278
#define SX_PRODUCT_ID  0x0325
#define SXCCD_EXP_FLAGS_FIELD_ODD       1
#define SXCCD_EXP_FLAGS_FIELD_EVEN      2
#define SXCCD_EXP_FLAGS_FIELD_BOTH      (SXCCD_EXP_FLAGS_FIELD_EVEN|SXCCD_EXP_FLAGS_FIELD_ODD)

static char *cameras_list[] = {"usb:", NULL}; 
static struct connection connections[MAX_CONNECTIONS];
static struct ccd open_ccds[MAX_OPEN_CCD];

/* functions that interface to the cameras */
extern int probe (GPPort *port);
extern int Reset (GPPort *port);
extern int echo (GPPort *port);
extern int get_params (GPPort *port, struct t_sxccd_params *params);
extern int expose (GPPort *port);
extern int ExposePixels(GPPort *port, unsigned short flags, unsigned short camIndex, unsigned short xoffset, unsigned short yoffset,
			unsigned short width, unsigned short height, unsigned short xbin, unsigned short ybin, unsigned long msec);
extern int ReadPixels(GPPort *port, unsigned short *pixels, unsigned long count);

/* get a list of accessible camera names */
char ** find_cameras(void)
{
	return cameras_list;
}
#if 0
/* compare the given url to the open cameras 
 * return the index in the ccd table, or -1 of not found */
static int lookup_url(char *url)
{
	int i;
	for (i=0; i< MAX_OPEN_CCD; i++) {
		if (open_ccds[i].url != NULL && !strcasecmp(open_ccds[i].url, url))
			return i;
	}
	return -1;
}

/* compare the given url base to the open connections
 * return the index in the conn table, or -1 of not found */
static int lookup_urlb(char *urlb)
{
	int i;
	for (i=0; i< MAX_CONNECTIONS; i++) {
		if (connections[i].target != NULL && !strcasecmp(connections[i].target, urlb))
			return i;
	}
	return -1;
}
#endif
static int find_ccd_open_slot(void)
{
	int i;
	for (i=0; i< MAX_OPEN_CCD; i++) {
		if (open_ccds[i].ref_count == 0)
			return i;
	}
	return -1;
}

static int find_conn_open_slot(void)
{
	int i;
	for (i=0; i< MAX_CONNECTIONS; i++) {
		if (connections[i].ref_count == 0)
			return i;
	}
	return -1;
}
#if 0
/* extract the protocol+hostname and camera name from an URL,
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
	while (url[i] && url[i] != '/') 
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
#endif
int close_connection(struct connection * conn)
{
	if (conn == NULL)
		return -1;
	if (conn->ref_count > 1) {
		conn->ref_count --;
		return 0;
	}
	if (conn->ref_count != 1) {
		err_printf("*warning* attempt to close connection with refcount of %d\n", 
			   conn->ref_count);
	}
	gp_port_close(conn->port);
	conn->ref_count = 0;
	return 0;
}

int cx_connect(char *host, int port)
{
	struct hostent *hent;
	struct sockaddr_in sockadr;
	int sock;

	hent = gethostbyname(host);
	if (hent == NULL) {
		err_printf("cx_connect: cannot find host %s, error %d\n", host, h_errno);
		return -1;
	}
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		err_printf("cx_connect: cannot create socket\n");
		return -1;
	}
	sockadr.sin_family = AF_INET;
	sockadr.sin_addr.s_addr = *((unsigned *)(hent->h_addr));
	sockadr.sin_port = htons(port);
//	d3_printf("h_length = %d adr = %08x\n", hent->h_length, *(hent->h_addr));
	if (connect(sock, (struct sockaddr *)(&sockadr), sizeof(sockadr))) {
		err_printf("cx_connect: could not connect: %d - %s\n", errno, strerror(errno));
		close(sock);
		return -1;
	}
	return sock;
}

static struct connection *open_dev(GPPort *port, GPPortInfo *info)
{
	struct connection *conn;
	int ci, retval;
#if 0
	ci = lookup_urlb(info->name);
	if (ci >= 0) { /* it has already been open */
		if (connections[ci].ref_count >= MAX_OPENS) {
			err_printf("ccd open too many times\n");
			return NULL;
		}
		connections[ci].ref_count ++;
		return &(connections[ci]);
	}
#endif
	ci = find_conn_open_slot();
	if (ci < 0) {
		err_printf("ccd open: table full\n");
		return NULL;
	}
	conn = &connections[ci];
	retval = gp_port_open(port);
	if (retval < 0) {
	        printf("Error: %s\n", gp_port_get_error(port));
		return NULL;
	}

	conn->port = port;
	conn->info = info;
	conn->ref_count = 1;
	conn->timeout = CX_DEFAULT_TIMEOUT;
	return conn;
}

/* open a camera; returns a handle to the camera, or NULL
 * for error. 
 */
struct ccd * ccd_open(char *url)
{
	struct ccd *ccd;
	struct connection *conn;
	int ci;

	int retval2;
	GPPortInfo info;
	GPPort *port;
	retval2 = gp_port_new(&port);
	memset(&info, 0, sizeof(info));
	strcpy(info.path, url);
	info.type = GP_PORT_USB;
	gp_port_set_info(port, info);
	retval2 = gp_port_usb_find_device(port, SX_VENDOR_ID, SX_PRODUCT_ID);
 
	if (retval2 < 0) {
	  err_printf("ccd_open: %s\n", strerror(errno));
		return NULL;
	}
#if 0
	ci = lookup_url(url);
	if (ci >= 0) { /* it has already been open */
		d3_printf("%s found in open list\n", url);
		if (open_ccds[ci].ref_count >= MAX_OPENS) {
			err_printf("ccd open too many times\n");
			return NULL;
		}
		open_ccds[ci].ref_count ++;
		return &(open_ccds[ci]);
	}
#endif
	ci = find_ccd_open_slot();
	if (ci < 0) {
		err_printf("ccd open: table full\n");
		return NULL;
	}
//	d3_printf("found free slot at %d\n", ci);
	ccd = &open_ccds[ci];
	conn = open_dev(port, &info);
	if (conn == NULL) 
		goto err_exit;
//	ccd->url = strdup(url);
	ccd->conn = conn;
	ccd->ref_count ++;
	ccd_reset(ccd);
	return ccd;

err_exit:
	return NULL;
}

/* close a camera and make it available for others
 */
int ccd_close(struct ccd * ccd)
{
	if (ccd == NULL)
		return -1;
	if (ccd->ref_count > 1) {
		ccd->ref_count --;
		return 0;
	}
	if (ccd->ref_count != 1) {
		err_printf("*warning* attempt to close camera with refcount of %d\n", 
			   ccd->ref_count);
	}
	close_connection(ccd->conn);
	ccd->ref_count = 0;
	return 0;
}

/* read a line from the socket, with timeout. If the line does not fit in the 
 * buffer provided, it is reallocated. returns lenth for success, or a negative error code
 */
int timeout_recv(int sock, char ** resp, int resp_size, int timeout)
{
	struct timeval tmo;
	char *nb;
	int ret, count = 0;
	fd_set fds;


	do {
		tmo.tv_sec = 0;
		tmo.tv_usec = timeout;
//		d3_printf("timeout is %d\n", timeout);		
		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		ret = select(FD_SETSIZE, &fds, NULL, NULL, &tmo);
		if (ret == 0) {
			err_printf("camera: timeout in receive\n");
			return -1;
		}
		if (ret == 1 && errno) {
			err_printf("camera: select error on %d %s\n", sock, strerror(errno));
			return -1;
		}
		ret = recv(sock, *resp + count, resp_size, 0);
		if (ret == 1 && errno != 0) {
			err_printf("error in receive\n");
			return -1;
		}
//		d3_printf("received %d\n", ret);
		if (*(*resp + ret - 1) == '\n') {
			*(*resp + ret - 1) = 0;
			return ret;
		}
		if (ret == resp_size) { /* we need to enlarge the buffer */
			nb = (char *)realloc(*resp, 2 * resp_size + 1);
			if (nb == NULL) {
				err_printf("error in realloc\n");
				return -1;
			}
			*resp = nb;
			resp_size = 2 * resp_size;
		}
		count += ret;
	} while (*resp);
	return -1;
}

/* like timeout_receive, but only do a single receive for the length
 * of the buffer */
int timeout_read_buf(int sock, void *buf, int size, int timeout)
{
	struct timeval tmo;
	int ret;
	fd_set fds;


	tmo.tv_sec = 0;
	tmo.tv_usec = timeout;
//		d3_printf("timeout is %d\n", timeout);		
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	ret = select(FD_SETSIZE, &fds, NULL, NULL, &tmo);
	if (timeout == 0 && ret == 0)
		return 0;
	if (ret == 0) {
		err_printf("timeout_read_buf: timeout in receive\n");
		return -1;
	}
	if (ret == 1 && errno) {
		err_printf("timeout_read_buf: select error\n");
		return -1;
	}
	ret = recv(sock, buf, size, 0);
	if (ret == 1 && errno != 0) {
		err_printf("timeout_read_buf: error in receive\n");
		return -1;
	}
	return ret;
}

#if 0
/* send a command and get the response; return 0 if the response was 'ok',
 * 1 if a different response was received, or a negative error code. 
 * if 1 is returned and msg is non-NULL, the (newly allocated) response 
 * string is updated in msg
 */
#define INITIAL_RESP_BUF 2000
int proto_command(struct ccd * ccd, char * cmd, char ** msg)
{
	int sock, ret, cnt;
	char *resp;

	if (ccd->conn == NULL) {
		err_printf("proto_command: no connection\n");
		return -1;
	}
	sock = ccd->conn->fd;
	cnt = strlen(cmd);
	ret = write(sock, cmd, cnt);
	if (ret != cnt) {
		err_printf("error sending command: %s\n", cmd);
		return -1;
	}
	resp = malloc(INITIAL_RESP_BUF+1);
	ret = timeout_recv(sock, &resp, INITIAL_RESP_BUF, ccd->conn->timeout);
	if (ret < 0) {
		free(resp);
		return ret;
	}
	if (!strncasecmp(resp, "ok", 2)) {
		free(resp);
		return 0;
	}
	if (msg != NULL) {
		*msg = resp;
	}
	return 1;
}

/* find a structure member matching the given name
 */
static struct member * member_lookup(struct member *memb, int level, char *name, int len)
{
//	d3_printf("looking up %15s\n", name);
	while(memb->name != NULL && (memb->type & MT_TYPE_MASK) != MT_NULL) {
		if (name_matches(memb->name, name, len) && level == memb->level) 
			return memb;
		memb ++;
	}
	return NULL;
}

/* try to set the value of a structure member, return 0 if successfull
 */
static int assign_member(struct member *cmem, void *structbase, char *val, int len)
{
	int v;
	double vd;
	char *nc, **op;

	switch(cmem->type & MT_TYPE_MASK) {
	case MT_INT:
		v = strtol(val, &nc, 0);
		if (val == nc) {
			return -1;
		}
		*((int *)(structbase + cmem->offset)) = v;
		return 0;
	case MT_FLOAT:
		vd = strtod(val, &nc);
		if (val == nc) {
			return -1;
		}
		*((float *)(structbase + cmem->offset)) = vd;
		return 0;
	case MT_DOUBLE:
		vd = strtod(val, &nc);
		if (val == nc) {
			return -1;
		}
		*((double *)(structbase + cmem->offset)) = vd;
		return 0;
	case MT_STRING:
		op = ((char **)(structbase + cmem->offset));
		free (*op);
		*op = lstrndup(val, len);
		return 0;
	}
	return -1;
}

#define PS_NAME 0
#define PS_VAL 1
/* parse a response a-list into the given structure, which is described by the
 * memb table return number of values successfully read or -1 for an error
 */
static int parse_structure(char *resp, void *rstruct, struct member *memb)
{
	char *text, *start, *end;
	int level = 0;
	int token, vals = 0;
	struct member *cmem = NULL;

	text = resp;
	while (*text != 0) {
		token = next_token(&text, &start, &end);
		switch(token) {
		case TOK_PUNCT:
			cmem = NULL;
			if (*start == '(') {
				level ++;
				break;
			}
			if (*start == ')') {
				if (level == 0) {
					err_printf("too many closed brackets\n");
					return -1;
				}
				level --;
				break;
			}
			return -1;
		case TOK_WORD:
			if (cmem == NULL) {/* we expect a name */
//				d3_printf("looking for %s len %d at level %d\n", 
//					  start, end-start+1,level);
				cmem = member_lookup(memb, level, start, end-start);
				if (cmem == NULL) {
					err_printf("bad varname\n");
				}
				break;
			} 
/* fallthrough */
		case TOK_STRING:
		case TOK_NUMBER:
			if (cmem == NULL) {
				err_printf("skipping non-name token\n");
				break;
			}
			if (!assign_member(cmem, rstruct, start, end-start)) {
				vals ++;
			} else {
				err_printf("bad value\n");
			}
			cmem = NULL;
			break;
		case TOK_EOL:
			return vals;
		default:
			return vals;
		}
	}
	return vals;
}

/* issue a set command for a structure member to a camera */
static int set_member(struct ccd *ccd, void *data, struct member *memb)
{
	int ret = 0;
	static int tmpi;
	static float tmpf;
	static double tmpd;
	static char *tmps;
	switch(memb->type & MT_TYPE_MASK) {
	case MT_INT:
	  tmpi = *((int *)data);
	  return ret;
	case MT_FLOAT:
	  tmpf = *((float *)data);
	  return ret;
	case MT_DOUBLE:
	  tmpd = *((double *)data);
	  return ret;
	case MT_STRING:
	  tmps = *((char **)data);
	  return ret;
	default:
	  err_printf("unknown member type: %d\n", memb->type);
	  return -1;
	}

}

/* set all (writable) members of a structure */
static int set_structure(struct ccd *ccd, void *sstruct, struct member *memb)
{
	int err = 0;
	while(memb->name != NULL && (memb->type & MT_TYPE_MASK) != MT_NULL) {
		if (memb->type & MT_WRITABLE) {
			err |= set_member(ccd, sstruct + memb->offset, memb);
		}
		memb++;
	}
	return -(err != 0);
}
#endif

/* set the params for further exposures; the camera must be idle, or an
 * error will be generated. 
 */
int ccd_set_exposure(struct ccd * ccd, struct exposure *expo)
{
  memcpy(&(ccd->exp), expo, sizeof *expo);
  return 0;
}

/* reset the camera to it's idle status and
 * abort any ongoing operations; also reset the hardware
 */
int ccd_reset(struct ccd * ccd)
{
return Reset (ccd->conn->port);
}

/* schedule an exposure start on a camera
 * return 0 if successful, or a negative error code
 * uses exposure parameters from exp; if the start is
 * successfull, updates exp for the current values
 */
int ccd_start_exposure(struct ccd * ccd, int dark)
{
        enum {camIndex = 0};
	int ret = 0;

	ccd->exp.dark = dark;
	/* begin next exposure */
	ExposePixels(ccd->conn->port, SXCCD_EXP_FLAGS_FIELD_BOTH, camIndex,
		       ccd->exp.x_skip, ccd->exp.y_skip,
		       ccd->exp.w << 1, ccd->exp.h >> 1,
		     ccd->exp.bin_x, ccd->exp.bin_y, (unsigned long)floor(ccd->exp.exp_time * 1000 + 0.5));
	ccd->exp.jdate = time(NULL)/86400.0 + (1970+4713)*365.2425;
	ccd->stat.state = CAM_READ_END;
	return ret;
}

/* set the cooler temperature */
int ccd_set_temperature(struct ccd * ccd, double temp_set)
{
	int ret = 0;
	/* jrrk */
	return ret;
}


/* terminate the current exposure prematurely */
int ccd_end_exposure(struct ccd * ccd)
{
	return 0;
}

/* get the current status of the camera 
 */
int ccd_read_status(struct ccd * ccd, struct cam_status *stat)
{
	int ret = 0;
	stat->state = ccd->stat.state;
	stat->expleft = 0.0;
	stat->linesread = 0;
	return ret;
}

/* get the current exposure parameters from the camera 
 */
int ccd_read_exposure(struct ccd * ccd, struct exposure *expo)
{
	int ret = 0;
	memcpy(expo, &(ccd->exp), sizeof *expo);
	return ret;
}

/* read data froma frame. Return the number of bytes read, 0 for end of frame, -1
 * if we expect data to be available later or another negative error code
 */
int ccd_read_frame(struct ccd * ccd, void *buf, int size)
{
	int count = 0;
	short *VideoImage = (short *)malloc(size);
	
	assert(size == sizeof(short) * ccd->exp.w * ccd->exp.h / ccd->exp.bin_x / ccd->exp.bin_y);

	if (ccd == NULL || ccd->conn == NULL)
		return -2; /* bad ccd/connection */

	count = ReadPixels(ccd->conn->port, (unsigned short *)VideoImage,		   
		     ccd->exp.w * ccd->exp.h / ccd->exp.bin_x / ccd->exp.bin_y);

	if (ccd->exp.dark)
	  memcpy(buf, VideoImage, size);
	else
	  Arrayswap(buf, VideoImage, ccd->exp.h/ccd->exp.bin_y,
				      ccd->exp.w/ccd->exp.bin_x,
		    ((short *)buf)+ccd->exp.w * ccd->exp.h / ccd->exp.bin_x / ccd->exp.bin_y,
				      VideoImage+ccd->exp.w * ccd->exp.h / ccd->exp.bin_x / ccd->exp.bin_y);
 
	free(VideoImage);

	ccd->stat.state = CAM_IDLE;	

	return count;
}

/* get the capabilities of the camera 
 */
int ccd_read_info(struct ccd * ccd, struct cam_info *info)
{
	int ret = 0;
  struct t_sxccd_params cam_param;
	ret = get_params(ccd->conn->port, &cam_param);
	info->dummy_pix_start = cam_param.hfront_porch;
	info->dummy_pix_end = cam_param.hback_porch;
	info->active_pixels = cam_param.width;
	info->dummy_lines_start = cam_param.vfront_porch;
	info->dummy_lines_end = cam_param.vback_porch;
	info->active_lines = cam_param.height;
	info->pix_x_size = cam_param.pix_width;
	info->pix_y_size = cam_param.pix_height;
	info->datavalid = 1;
	info->min_exp = 0.001;
	info->max_exp = 86400.0;
	info->scale = 1;
	info->name = "SXV-M25c";
#if 0
	cam_param.color_matrix;
	cam_param.bits_per_pixel;
	cam_param.num_serial_ports;
	cam_param.extra_caps;
#endif
	ccd->exp.w = ccd->info.active_pixels;
	ccd->exp.h = ccd->info.active_lines;
	ccd->exp.x_skip = 0;
	ccd->exp.y_skip = 0;
	ccd->exp.bin_x = 1;
	ccd->exp.bin_y = 1;
	ccd->exp.exp_time = 1.0;
	return ret;
}

/* get the status of the cooler
 */
int ccd_read_cooler(struct ccd * ccd, struct cooler *cooler)
{
	int ret = 0;
	cooler->temp = -20.0;
	cooler->set_temp = -20.0;
	cooler->cooling_power = 1;
	return ret;
}

/* return a string describing the camera's status 
 */
void cam_status_string(struct ccd *ccd, char *buf, int len)
{
	if (ccd == NULL) {
		strncpy(buf, "(null)", len);
		return;
	}
	if (ccd->ref_count == 0) {
		strncpy(buf, "Not Connected", len);
		return;
	}
	switch(ccd->stat.state & CAM_STAT_MASK) {
	case CAM_INVALID:
		strncpy(buf, "Invalid", len);
		break;
	case CAM_IDLE:
		strncpy(buf, "Idle", len);
		break;
	case CAM_EXP:
		snprintf(buf, len, "Integrating, %.1f s left", ccd->stat.expleft);
		break;
	case CAM_EXPEND:
		strncpy(buf, "Finishing integration", len);
		break;
	case CAM_READ:
		snprintf(buf, len, "Reading, line %d", ccd->stat.linesread);
		break;
	case CAM_READ_END:
		strncpy(buf, "Transferring data", len);
		break;
	}
}

/* set the frame's header fields with data from the ccd */
void ccd_frame_set_exp_data(struct ccd_frame *fr, struct ccd *ccd)
{
	char lb[128];
	char date[64];
	double p;

	fr->x_skip = ccd->exp.x_skip;
	fr->y_skip = ccd->exp.y_skip;
	fr->pix_format = PIX_16LE;
	fr->exp.datavalid = 1;
	fr->exp.bin_x = ccd->exp.bin_x;
	fr->exp.bin_y = ccd->exp.bin_y;

	fr->fim.xrefpix = fr->w / 2.0;
	fr->fim.yrefpix = fr->h / 2.0;
	p = -180.0 / PI / (P_DBL(OBS_FLEN) * 10000.0);
//	d3_printf("%f\n", p);
	fr->fim.xinc = ccd->info.pix_x_size * p * fr->exp.bin_x;
	if (P_INT(OBS_FLIPPED))
		fr->fim.yinc = - ccd->info.pix_y_size * p * fr->exp.bin_y;
	else
		fr->fim.yinc = ccd->info.pix_y_size * p * fr->exp.bin_y;

	sprintf(lb, "%20d / OFFSET OF FIRST PIXEL FROM SENSOR CORNER", 
		fr->x_skip);
	fits_add_keyword(fr, P_STR(FN_SKIPX), lb);
	sprintf(lb, "%20d / OFFSET OF FIRST PIXEL FROM SENSOR CORNER", 
		fr->y_skip);
	fits_add_keyword(fr, P_STR(FN_SKIPY), lb);
	sprintf(lb, "%20d / BINNING IN X DIRECTION", 
		fr->exp.bin_x);
	fits_add_keyword(fr, P_STR(FN_BINX), lb);
 	sprintf(lb, "%20d / BINNING IN Y DIRECTION", 
		fr->exp.bin_y);
	fits_add_keyword(fr, P_STR(FN_BINY), lb);

	if (ccd->info.name != NULL) {
		snprintf(lb, 127, "'%s'", ccd->info.name);
		fits_add_keyword(fr, P_STR(FN_INSTRUME), lb);
	}
	sprintf(lb, "%20.1f / ELECTRONS / ADU", ccd->info.scale);
	fits_add_keyword(fr, P_STR(FN_ELADU), lb);
	sprintf(lb, "%20.1f / READ NOISE IN ADUs", ccd->info.rdnoise);
	fits_add_keyword(fr, P_STR(FN_RDNOISE), lb);
//	sprintf(lb, "%20.1f / MULTIPLICATIVE NOISE COEFF", 0);
//	fits_add_keyword(fr, P_STR(FN_FLNOISE), lb);
	sprintf(lb, "%20.3f / EXPOSURE TIME IN SECONDS", ccd->exp.exp_time);
	fits_add_keyword(fr, P_STR(FN_EXPTIME), lb);
	sprintf(lb, "%20.8f / JULIAN DATE OF EXPOSURE START (UTC)", ccd->exp.jdate);
	fits_add_keyword(fr, P_STR(FN_JDATE), lb);
	date_time_from_jdate(ccd->exp.jdate, date, 63);
	fits_add_keyword(fr, P_STR(FN_DATE_OBS), date);
	sprintf(lb, "%20.1f / SENSOR TEMPERATURE IN K", ccd->cooler.temp + 273.15);
	fits_add_keyword(fr, P_STR(FN_SNSTEMP), lb);
	sprintf(lb, "%20.8f / IMAGE SCALE IN SECONDS PER PIXEL", 
		3600.0 * fabs(fr->fim.xinc));
	fits_add_keyword(fr, P_STR(FN_SECPIX), lb);
}
