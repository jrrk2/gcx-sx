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

/* read recipy files; create tabular reports */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <glob.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gcx.h"
#include "catalogs.h"
#include "gui.h"
#include "obsdata.h"
#include "sourcesdraw.h"
#include "params.h"
#include "interface.h"
#include "wcs.h"
#include "camera.h"
#include "cameragui.h"
#include "filegui.h"

#include "symbols.h"
#include "recipy.h"


/* skip until we get a closing brace */

static void skip_list(GScanner *scan) 
{
	int nbr = 1;
	int tok;
	do {
		tok = g_scanner_get_next_token(scan);
		if (tok == '(')
			nbr ++;
		else if (tok == ')')
			nbr --;
	} while ((tok != G_TOKEN_EOF) && (nbr > 0));
}

/* skip one item (or a whole list if that is the case */
static void skip_one(GScanner *scan) 
{
	int tok;
	tok = g_scanner_get_next_token(scan);
	if (tok == '(') 
		skip_list(scan);
}

/* if the next token is a floating point number, read it, convert to precision
 * and remove from the input stream; return 0 if that happens */
static int try_scan_precision(GScanner *scan, int *w, int *p)
{
	double v;
	if (g_scanner_peek_next_token(scan) != G_TOKEN_FLOAT) {
		*w = -1;
		*p = -1;
		return -1;
	}
	g_scanner_get_next_token(scan);
	v = floatval(scan);
	if (v < 1.0) {
		*w = -1;
		*p = floor(v * 10 + 0.5);
	} else {
		*w = floor(v);
		v -= *w;
		*p = floor(v * 10 + 0.5);
	}
	return 0;
}

/* scan the format string (that can be multiline), and fill up the cfmt table
 * return the number of fields read, or -1 if an error occured; up to n
 * columns are read; 
 * when present, the data fields are malloced strings
 * we use '#' comments in the format file 
 */

#define TAB_OPTION_TABLEHEAD 0x01
#define TAB_OPTION_COLLIST 0x02

static int parse_tab_format(char *format, struct col_format *cfmt, int n, int *options)
{
	GScanner *scan;
	GTokenType tok;

	int sym;

	int i = 0;
	int opt = 0;

	scan = init_scanner();
	scan->config->cpair_comment_single = "#\n";
	g_scanner_input_text(scan, format, strlen(format));

	do {
		tok = g_scanner_get_next_token(scan);
		if (tok != G_TOKEN_SYMBOL && tok != G_TOKEN_EOF) {
			err_printf("unexpected token while scanning table format\n");
			skip_one(scan);
			continue;
		}
		if (tok == G_TOKEN_EOF)
			break;
		switch((sym = intval(scan))) {
		case SYM_TABLEHEAD:
			opt |= TAB_OPTION_TABLEHEAD;
			break;
		case SYM_COLLIST:
			opt |= TAB_OPTION_COLLIST;
			break;
		case SYM_SMAG:
		case SYM_SMAGS:
			try_scan_precision(scan, &cfmt[i].width, &cfmt[i].precision);
			if (g_scanner_peek_next_token(scan) != G_TOKEN_STRING) {
				err_printf("format error: %s needs a band name string\n",
					   symname[sym]);
				continue;
			} 
			tok = g_scanner_get_next_token(scan);
			cfmt[i].data = strdup(stringval(scan));
			cfmt[i].type = SYM_SMAG;
			i++;
			break;
		case SYM_IMAG:
		case SYM_IMAGS:
			try_scan_precision(scan, &cfmt[i].width, &cfmt[i].precision);
			if (g_scanner_peek_next_token(scan) != G_TOKEN_STRING) {
				err_printf("format error: %s needs a band name string\n",
					   symname[sym]);
				continue;
			} 
			tok = g_scanner_get_next_token(scan);
			cfmt[i].data = strdup(stringval(scan));
			cfmt[i].type = SYM_IMAG;
			i++;
			break;
		case SYM_SERR:
		case SYM_IERR:
			try_scan_precision(scan, &cfmt[i].width, &cfmt[i].precision);
			if (g_scanner_peek_next_token(scan) != G_TOKEN_STRING) {
				err_printf("format error: %s needs a band name string\n",
					   symname[sym]);
				continue;
			} 
			tok = g_scanner_get_next_token(scan);
			cfmt[i].data = strdup(stringval(scan));
			cfmt[i].type = sym;
			i++;
			break;
		case SYM_OBSERVATION:
		case SYM_AIRMASS:
		case SYM_JDATE:
		case SYM_MJD:
		case SYM_FILTER:
		case SYM_RA:
		case SYM_DEC:
		case SYM_DRA:
		case SYM_DDEC:
		case SYM_FLAGS:
		case SYM_NAME:
		case SYM_RESIDUAL:
		case SYM_STDERR:
			try_scan_precision(scan, &cfmt[i].width, &cfmt[i].precision);
			if (g_scanner_peek_next_token(scan) == G_TOKEN_STRING) {
				tok = g_scanner_get_next_token(scan);
				cfmt[i].data = strdup(stringval(scan));
			} else {
				cfmt[i].data = NULL;
			}
			cfmt[i].type = sym;
			i++;
			break;
		default:
			err_printf("unexpected symbol \'%s\' while scanning table format\n",
				   symname[sym]);
			break;
		}
	} while (tok != G_TOKEN_EOF && i < n);
	g_scanner_destroy(scan);
	if (options)
		*options = opt;
	return i;
}


/* convert the lispish report format to a table
 * format is a string that describes what should be output;
 * it's a list of tokens describing the output columns; some
 * tokens take an argument; each token is followed by an optional column
 * width.precision; a single space separates the columns
 * name 16 ra 12 dec 12 smag "v(tycho)" 8 smagerr "v" 8 obsname 16 airmass 5 mjd 12
 * if format is null, it is taken from par
 */

/* obs tokens:
 * observation (a random identifier plus the mjd of the observation)
 * airmass - calculated from obs fields; 1.0 if unknown
 * jd/mjd - the julian date / mjd of the observation
 * filter - the filter name
 */

/* star tokens 
 * name - the star designation
 * ra, dec - in dms format
 * dra, ddec - in decimal degrees format
 * smag <band name> - star standard magnitude in the given band
 * serr <band name> - standard magnitude error
 * imag, ierr - same for instrumental magnitudes
 * flags - extraction/reduction flags (binary)
 */

/* control tokens 
 * tablehead - print a table header
 * collist - print a list of columns
 */

#define TABLE_MAX_FIELDS 128
#define MAX_TBL_LINE 16384

static int tab_snprint_star(char *line, int len, struct cat_star *cats, 
			    struct stf *stf, int frno, 
			    struct col_format *cfmt, int ncol);
static int tab_snprint_head(char *line, int len, 
			    struct col_format *cfmt, int ncol);
static int tab_snprint_coldesc(char *line, int len, 
			       struct col_format *cfmt, int start);
void stf_to_table(struct stf *stf, FILE *outf, struct col_format *cfmt, int ncol);

void report_to_table(FILE *inf, FILE *outf, char *format)
{
	struct col_format cfmt[TABLE_MAX_FIELDS];
	char line[MAX_TBL_LINE];
	int i, n = 0, f = 0, ncol, opt, c;
	struct stf *stf;
	GList *stars, *sl;
	struct cat_star *cats;

	srandom(time(NULL));

	if (format == NULL)
		format = P_STR(FILE_TAB_FORMAT);
	ncol = parse_tab_format(format, cfmt, TABLE_MAX_FIELDS, &opt);

//	d3_printf("parse format returns %d\n", ncol);
//	for (i=0; i<ncol; i++) {
//		d3_printf("%s %d %d \"%s\"\n", symname[cfmt[i].type], cfmt[i].width, 
//			  cfmt[i].precision, (char *)(cfmt[i].data));
//	}

	/* the header */
	tab_snprint_star(line, MAX_TBL_LINE, NULL, NULL, 0, cfmt, ncol);
	if (opt & TAB_OPTION_COLLIST) {
		c = 0;
		for (i = 0; i < ncol; i++) {
			c += tab_snprint_coldesc(line, MAX_TBL_LINE, cfmt+i, c);
			fprintf(outf, "# %s\n", line);
		}
		fprintf(outf, "\n");
	}
	if (opt & TAB_OPTION_TABLEHEAD) {
		tab_snprint_head(line, MAX_TBL_LINE, cfmt, ncol);
		fprintf(outf, "%s\n\n", line);
	}

	do {
		stf = stf_read_frame(inf);
		if (stf == NULL)
			break;
		stars = stf_find_glist(stf, 0, SYM_STARS);
		if (stars == NULL) {
			stf_free_all(stf);
			continue;
		}
		f++;
		for (sl = stars; sl != NULL; sl = sl->next) {
			cats = CAT_STAR(sl->data);
			tab_snprint_star(line, MAX_TBL_LINE, cats, stf, f, cfmt, ncol);
			fprintf(outf, "%s\n", line);
			n++;
		}
		stf_free_all(stf);
	} while (stf != NULL);
}

/* fill a width-wide field at buf and place a 0 terminator, without exceeding
 * len characters (including the terminator). Return the numbers of chars added 
 * to the buffer (excluding the term) */
static inline int blank_field(char *buf, int len, int width)
{
	int i;
	for (i = 0; (i < width) && (i < (len - 1)); i++)
		*buf++ = ' ';
	*buf = 0;
	return i;
}

/* a bit like snprintf with a %-width s argument, but it truncates the
 * string to fit the field length, and it places a trailing blank */
static inline int string_field(char *buf, int len, int width, char *text)
{
	int i, k;
	k=0;
	for (i = 0; (i < width) && (i < (len - 1)); i++) {
		if (*text == 0 || i == width -1)
			*buf++ = ' ';
		else
			*buf++ = *text++;
	}
	*buf = 0;
	return i;
}


/* generate a field of exactly width characetrs (but no more than len) containing
 * the bits in flags that have non-zero-length names in names 
 * return the number of chars generated, less the terminating 0; the last
 * char is guaranteed to be ' ' */
static inline int flags_field(char *buf, int len, int width, int flags, char **names)
{
	int i, k;
	k=0;
	for (i = 0; (i < width) && (i < (len - 1)); i++) {
		while (names[k] && (*names[k] == 0)) {
			k++;
			flags >>= 1;
		}
		if (i == width - 1) {
			*buf++ = ' ';
		} else if (names[k] == NULL) {
			*buf++ = ' ';
		} else {
			*buf++ = (flags & 0x01) ? '1' : '0';
		}
		if (names[k]) {
			k++;
			flags >>= 1;
		}
	}
	*buf = 0;
	return i;
}


/* print the description for a column, assuming it starts at 
 * column start; return the full width of the column */
static int tab_snprint_coldesc(char *line, int len, 
			    struct col_format *cfmt, int start)
{
	char buf[64];
	int p=0, k=0;

	snprintf(buf, 64, "%d-%d", start+1, start + cfmt->width);
	if (cfmt->type == SYM_FLAGS) {
		p += snprintf(line, len, "%-8s %s: type ", buf, symname[cfmt->type]);
		if (p >= len)
			return cfmt->width + 1;
		while(cat_flag_names[k] != NULL) {
			if (*cat_flag_names[k] == 0) {
				k++;
				continue;
			}
			p += snprintf(line+p, len-p, "%s ", cat_flag_names[k]);
			if (p >= len)
				return cfmt->width + 1;
			k++;
		}
	} else {
		snprintf(line, len, "%-8s %s %s", buf, symname[cfmt->type], 
			 cfmt->data ? (char *)cfmt->data : "");
	}
	return cfmt->width + 1;
}


/* print a table header */
static int tab_snprint_head(char *line, int len, 
			    struct col_format *cfmt, int ncol)
{
	int i;
	int p = 0, ret = 0;
	char buf[64];
	int w;

	for (i=0; i<ncol; i++) {
		w = cfmt[i].width;
		if (i==0) {
			w--;
			p += string_field(line + p, len - 1, 2, "#");
			if (p >= len)
				break;
		}
		switch(cfmt[i].type) {
		case SYM_SMAG:
			snprintf(buf, 63, "s(%s)", (char *)cfmt[i].data);
			ret = string_field(line + p, len - p, w + 1, buf);
			break;
		case SYM_IMAG:
			snprintf(buf, 63, "i(%s)", (char *)cfmt[i].data);
			ret = string_field(line + p, len - p, w + 1, buf);
			break;
		case SYM_SERR:
			snprintf(buf, 63, "se(%s)", (char *)cfmt[i].data);
			ret = string_field(line + p, len - p, w + 1, buf);
			break;
		case SYM_IERR:
			snprintf(buf, 63, "ie(%s)", (char *)cfmt[i].data);
			ret = string_field(line + p, len - p, w + 1, buf);
			break;
		default:
			if (i >= 1)
				ret = string_field(line + p, len - p, w + 1, 
						   symname[cfmt[i].type]);
			else 
				ret = string_field(line + p, len - p, w, 
						   symname[cfmt[i].type]);
			break;
		}
		if (p > 4)
			line[p-1]='|';
		p += ret;
		if (p >= len)
			break;
	}
	return p;
}


/* print the one-line report for the star 
 * return the number of char written 
 * cfmt width/precision fields are filled with defaults if 
 * they were -1 */
static int tab_snprint_star(char *line, int len, struct cat_star *cats, 
			    struct stf *stf, int f,
			    struct col_format *cfmt, int ncol)
{
	int i;
	int p = 0, ret = 0;
	char dms[64];
	double m, e;
	char c;
	char *t;
	double v;

	for (i=0; i<ncol; i++) {
		switch(cfmt[i].type) {
		case SYM_SMAG:
//			d3_printf("looking for %s in %s\n", (char *)cfmt[i].data, cats->smags);
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 3;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 4 + cfmt[i].precision;
			if (cats == NULL)
				break;
			if (get_band_by_name(cats->smags, cfmt[i].data, &m, &e))
				ret = blank_field(line+p, len - p, cfmt[i].width+1);
			else
				ret = snprintf(line+p, len - p, "%*.*f ", 
					       cfmt[i].width, cfmt[i].precision, m);
			break;
		case SYM_IMAG:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 3;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 4 + cfmt[i].precision;
			if (cats == NULL)
				break;
			if (get_band_by_name(cats->imags, cfmt[i].data, &m, &e))
				ret = blank_field(line+p, len - p, cfmt[i].width+1);
			else
				ret = snprintf(line+p, len - p, "%*.*f ", 
					       cfmt[i].width, cfmt[i].precision, m);
			break;
		case SYM_SERR:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 3;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 4 + cfmt[i].precision;
			if (cats == NULL)
				break;
			e = BIG_ERR;
			if ((!get_band_by_name(cats->smags, cfmt[i].data, &m, &e)) &&
			    e < BIG_ERR)
				ret = snprintf(line+p, len - p, "%*.*f ", 
					       cfmt[i].width, cfmt[i].precision, e);
			else
				ret = blank_field(line+p, len - p, cfmt[i].width+1);
			break;
		case SYM_IERR:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 3;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 4 + cfmt[i].precision;
			if (cats == NULL)
				break;
			e = BIG_ERR;
			if ((!get_band_by_name(cats->imags, cfmt[i].data, &m, &e)) &&
			    e < BIG_ERR)
				ret = snprintf(line+p, len - p, "%*.*f ", 
					       cfmt[i].width, cfmt[i].precision, e);
			else
				ret = blank_field(line+p, len - p, cfmt[i].width+1);
			break;
		case SYM_RA:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 2;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 9 + cfmt[i].precision;
			if (cats == NULL)
				break;
			degrees_to_dms_pr(dms, cats->ra / 15.0, 2);
			ret = string_field(line + p, len - p, cfmt[i].width + 1, dms);
			break;
		case SYM_DEC:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 1;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 9 + cfmt[i].precision;
			if (cats == NULL)
				break;
			degrees_to_dms_pr(dms, cats->dec, 1);
			ret = string_field(line + p, len - p, cfmt[i].width + 1, dms);
			break;
		case SYM_DRA:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 4;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 4 + cfmt[i].precision;
			if (cats == NULL)
				break;
			ret = snprintf(line+p, len - p, "%*.*f ", 
				       cfmt[i].width, cfmt[i].precision,
				       cats->ra);
			break;
		case SYM_DDEC:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 4;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 4 + cfmt[i].precision;
			if (cats == NULL)
				break;
			ret = snprintf(line+p, len - p, "%*.*f ", 
				       cfmt[i].width, cfmt[i].precision,
				       cats->dec);
			break;
		case SYM_MJD:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 12;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 2 + cfmt[i].precision;
			if (cats == NULL)
				break;
			if (!stf_find_double(stf, &v, 1, SYM_OBSERVATION, SYM_MJD)) {
				ret = blank_field(line+p, len - p, cfmt[i].width+1);
			} else {
				ret = snprintf(line+p, len - p, "%-*.*g ", 
					       cfmt[i].width, cfmt[i].precision,
					       v);
			}
			break;
		case SYM_RESIDUAL:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 3;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 3 + cfmt[i].precision;
			if (cats == NULL)
				break;
			if (CATS_TYPE(cats) != CAT_STAR_TYPE_APSTD) {
				ret = blank_field(line+p, len - p, cfmt[i].width+1);
			} else {
				ret = snprintf(line+p, len - p, "%*.*f ", 
					       cfmt[i].width, cfmt[i].precision,
					       cats->residual);
			}
			break;
		case SYM_STDERR:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 3;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 3 + cfmt[i].precision;
			if (cats == NULL)
				break;
			if (CATS_TYPE(cats) != CAT_STAR_TYPE_APSTD) {
				ret = blank_field(line+p, len - p, cfmt[i].width+1);
			} else {
				ret = snprintf(line+p, len - p, "%*.*f ", 
					       cfmt[i].width, cfmt[i].precision,
					       cats->stderr);
			}
			break;
		case SYM_JDATE:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 12;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 2 + cfmt[i].precision;
			if (cats == NULL)
				break;
			if (stf_find_double(stf, &v, 1, SYM_OBSERVATION, SYM_MJD)) {
				ret = snprintf(line+p, len - p, "%-*.*g ", 
					       cfmt[i].width, cfmt[i].precision,
					       mjd_to_jd(v));
			} else if (stf_find_double(stf, &v, 1, SYM_OBSERVATION, SYM_JDATE)) {
				ret = snprintf(line+p, len - p, "%-*.*g ", 
					       cfmt[i].width, cfmt[i].precision,
					       v);
			} else {
				ret = blank_field(line+p, len - p, cfmt[i].width+1);
			} 
			break;
		case SYM_AIRMASS:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 2;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 2 + cfmt[i].precision;
			if (cats == NULL)
				break;
			if (stf_find_double(stf, &v, 1, SYM_OBSERVATION, SYM_AIRMASS)) {
				ret = snprintf(line+p, len - p, "%*.*f ", 
					       cfmt[i].width, cfmt[i].precision,
					       v);
			} else {
				ret = blank_field(line+p, len - p, cfmt[i].width+1);
			}
			break;
		case SYM_OBSERVATION:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 0;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 16;
			if (cats == NULL)
				break;
			t = stf_find_string(stf, 1, SYM_OBSERVATION, SYM_OBJECT);
			if (t != NULL) {
				snprintf(dms, 63, "%s%d ", t, f);
				ret = string_field(line + p, len - p, cfmt[i].width + 1, 
						   dms);
			} else {
				snprintf(dms, 63, "%d ", f);
				ret = string_field(line + p, len - p, cfmt[i].width + 1, 
						   dms);
			}
			break;
		case SYM_FILTER:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 0;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 6;
			if (cats == NULL)
				break;
			t = stf_find_string(stf, 1, SYM_OBSERVATION, SYM_FILTER);
			if (t != NULL) {
				ret = string_field(line + p, len - p, cfmt[i].width + 1, 
						   t);
			} else {
				ret = blank_field(line+p, len - p, cfmt[i].width+1);
			} 
			break;
		case SYM_NAME:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 0;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 14;
			if (cats == NULL)
				break;
			ret = string_field(line + p, len - p, cfmt[i].width + 1, 
					   cats->name);
			break;
		case SYM_FLAGS:
			if (cfmt[i].precision < 0)
				cfmt[i].precision = 0;
			if (cfmt[i].width <= 0)
				cfmt[i].width = 9;
			if (cats == NULL)
				break;
			switch (CATS_TYPE(cats)) {
			case CAT_STAR_TYPE_CAT:
				c = 'C';
				break;
			case CAT_STAR_TYPE_APSTD:
				c = 'S';
				break;
			case CAT_STAR_TYPE_APSTAR:
				c = 'T';
				break;
			case CAT_STAR_TYPE_SREF:
			default:
				c = 'F';
				break;
			}
			if (cfmt[i].width > 2 && len-p > 2) {
				*(line+p) = c;
				*(line+p+1) = 0;
				p++;
			}
			ret = flags_field(line + p, len - p, cfmt[i].width, cats->flags, 
					  cat_flag_names);
			break;
		}
		p += ret;
		if (p >= len)
			break;
	}
	return p;
}


#define DB_FIELD 256

/* extract the data portion of a <token>=<data> field from text into out 
 * (max n-1 characters) */
static void extract_field(char *text, char *out, int n, char *token)
{
	int i, k, l;
	g_return_if_fail(out != NULL);
	if (text == NULL || token == NULL)
		return;
	l = strlen(token);
	*out = 0;
	i = 0;
	while (text[i] != 0) {
		if (!strncasecmp(token, text+i, l))
			break;
		i++;
	}
	if (text[i] == 0)
		return;
	i+= l;
	k = 0;
	while((text[i] != 0) && (n > 1) && !isblank(text[i])) {
		out[k++] = text[i++];
	}
	out[k] = 0;
	i=0;
	while (out[i] != 0) {
		if (out[i] == '_' || out[i] == '-')
			out[i] = ' ';
		i++;
	}
	d4_printf("extracted |%s| for %s\n", out);
}

#define CONV_MAX_STARS 10000
#define CONV_MAX_OBS 10


/* convert a recipy file to the aavso database format */
int recipy_to_aavso_db(FILE *inf, FILE *outf)
{
	struct cat_star *cst[CONV_MAX_STARS];
	struct obs_data *obst[CONV_MAX_OBS];
	int ret, i;
	char d[DB_FIELD];
	char s[DB_FIELD];
	char t[DB_FIELD];
	char n[DB_FIELD];
	char w[DB_FIELD];
	char p[DB_FIELD];
	char f[DB_FIELD];
	char sp[DB_FIELD];
	char sc[DB_FIELD];
	char sn[DB_FIELD];
	char si[DB_FIELD];
	double m;
	char coord[64];


	d[0] = 0; s[0] = 0; t[0] = 0; n[0] = 0; w[0] = 0; p[0] = 0; f[0] = 0;


	for (i=0; i<CONV_MAX_OBS; i++)
		obst[i] = NULL;

	ret = -1; // read_obs_report(inf, cst, CONV_MAX_STARS, obst, CONV_MAX_OBS);

	if (ret <= 0)
		return ret;

/* get data from recipy comments */
	for (i=0; i<CONV_MAX_OBS; i++) {
		if (obst[i] == NULL)
			break;
		d4_printf("rcp comment is: %s\n",
			  obst[i]->comment);
		extract_field(obst[i]->comment, d, DB_FIELD, "d=");
		extract_field(obst[i]->comment, s, DB_FIELD, "s=");
		extract_field(obst[i]->comment, t, DB_FIELD, "t=");
		extract_field(obst[i]->comment, n, DB_FIELD, "n=");
		extract_field(obst[i]->comment, w, DB_FIELD, "w=");
		extract_field(obst[i]->comment, p, DB_FIELD, "p=");
	}

	for (i=0; i<ret; i++) {
		sp[0] = 0; sn[0] = 0; si[0] = 0; sc[i] = 0;
		extract_field(cst[i]->comments, sp, DB_FIELD, "p=");
		extract_field(cst[i]->comments, sn, DB_FIELD, "n=");
		extract_field(cst[i]->comments, si, DB_FIELD, "i=");
		extract_field(cst[i]->comments, sc, DB_FIELD, "c=");
		if (si[0] == 0 && sn[0] == 0) {
			get_band_by_name(cst[i]->smags, "v(aavso)", &m, NULL);
			snprintf(si, DB_FIELD, "%.0f", m*10);
		}
		fprintf(outf, "%s\t%s\t%s\t%s\t%s\t%s\t",
			d, n, s, t, si, sn);
		degrees_to_dms_pr(coord, cst[i]->ra / 15.0, 2);
		coord[2] = ' ';
		coord[5] = ' ';
		fprintf(outf, "%s\t", coord);
		degrees_to_dms_pr(coord, cst[i]->dec, 1);
		coord[2] = ' ';
		coord[5] = ' ';
		fprintf(outf, "%s\t", coord);
		fprintf(outf, "%s\t%s\t%s\t\t%s\t%s\n",
			sp, cst[i]->name, w, p, sc);
	}

	for (i=0; i<CONV_MAX_OBS; i++) {
		if (obst[i] == NULL)
			break;
		obs_data_release(obst[i]);
	}
	for (i=0; i<ret; i++) {
		obs_data_release(cst[i]->data);
		cat_star_release(cst[i]);
	}
	return ret;
}

