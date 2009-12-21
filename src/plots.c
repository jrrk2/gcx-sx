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

/* Multiband reduction plot routines */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "gcx.h"
#include "params.h"
#include "misc.h"
#include "obsdata.h"
#include "multiband.h"
#include "catalogs.h"
#include "recipy.h"
#include "symbols.h"
#include "sourcesdraw.h"
#include "wcs.h"

#define STD_ERR_CLAMP 30.0
#define RESIDUAL_CLAMP 2.0

/* commands prepended at the beginning of each plot */
static void plot_preamble(FILE *dfp)
{
	fprintf(dfp, "set key outside\n");
}

/* create a plot of ofr residuals versus magnitude (as a gnuplot file) */
int ofrs_plot_residual_vs_mag(FILE *dfp, GList *ofrs, int weighted)
{
	GList *sl, *osl, *bl, *bsl = NULL;
	struct star_obs *sob;
	struct o_frame *ofr = NULL;
	int n = 0, i = 0;
	int band;
	double v, u;

	g_return_val_if_fail(dfp != NULL, -1);
	plot_preamble(dfp);
	fprintf(dfp, "set xlabel 'Standard magnitude'\n");
	if (weighted) {
		fprintf(dfp, "set ylabel 'Standard errors'\n");
	} else {
		fprintf(dfp, "set ylabel 'Residuals'\n");
	}
//	fprintf(dfp, "set yrange [-1:1]\n");
//	fprintf(dfp, "set title '%s: band:%s mjd=%.5f'\n",
//		ofr->obs->objname, ofr->filter, ofr->mjd);
	fprintf(dfp, "plot ");
	
	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band < 0) 
			continue;
		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);
			if (i > 0)
				fprintf(dfp, ", ");
			fprintf(dfp, "'-' title '%s'", ofr->trans->bname);
			i++;
		}
	}
	fprintf(dfp, "\n");

	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (int) bl->data;
		osl = ofrs;
		while (osl != NULL) {
			ofr = O_FRAME(osl->data);
			osl = g_list_next(osl);
			if (ofr->band != band) 
				continue;
			sl = ofr->sol;
			while(sl != NULL) {
				sob = STAR_OBS(sl->data);
				sl = g_list_next(sl);
				if (CATS_TYPE(sob->cats) != CAT_STAR_TYPE_APSTD)
					continue;
				if (sob->weight <= 0.0001)
					continue;
				n++;
				v = sob->residual * sqrt(sob->nweight);
				u = sob->residual;
				clamp_double(&v, -STD_ERR_CLAMP, STD_ERR_CLAMP);
				clamp_double(&u, -RESIDUAL_CLAMP, RESIDUAL_CLAMP);
				if (weighted)
					fprintf(dfp, "%.5f %.5f %.5f\n", sob->ost->smag[ofr->band],
						v, sob->imagerr);
				else 
					fprintf(dfp, "%.5f %.5f %.5f\n", sob->ost->smag[ofr->band],
						u, sob->imagerr);
			}	
		}
		fprintf(dfp, "e\n");
	}
	g_list_free(bsl);
//	fprintf(dfp, "pause -1\n");
	return n;
}


/* create a plot of ofr residuals versus magnitude (as a gnuplot file) */
int ofrs_plot_zp_vs_time(FILE *dfp, GList *ofrs)
{
	GList *osl, *bl, *bsl = NULL;
	struct o_frame *ofr = NULL;
	double mjdi = 0.0;
	int n = 0, i = 0;
	int band;
	GList *asfl = NULL, *bnames = NULL;


	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band < 0) 
			continue;
		mjdi = floor(ofr->mjd);
		break;
	}
	g_return_val_if_fail(dfp != NULL, -1);
	plot_preamble(dfp);
	fprintf(dfp, "set xlabel 'Days from MJD %.1f'\n", mjdi);
	fprintf(dfp, "set ylabel 'Magnitude'\n");
	fprintf(dfp, "set title 'Fitted Frame Zeropoints'\n");
//	fprintf(dfp, "set format x \"%%.3f\"\n");
	fprintf(dfp, "set xtics autofreq\n");
//	fprintf(dfp, "set yrange [-1:1]\n");
//	fprintf(dfp, "set title '%s: band:%s mjd=%.5f'\n",
//		ofr->obs->objname, ofr->filter, ofr->mjd);
	fprintf(dfp, "plot  ");
	
	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band < 0) 
			continue;
//		d3_printf("*%d\n", ZPSTATE(ofr));
		if (ZPSTATE(ofr) == ZP_ALL_SKY) {
			asfl = g_list_prepend(asfl, ofr);
		}
		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);
			if (i > 0)
				fprintf(dfp, ", ");
			fprintf(dfp, "'-' title '%s' with errorbars ", 
				ofr->trans->bname);
			bnames = g_list_append(bnames, ofr->trans->bname);
			i++;
		}
	}
	if (asfl != NULL) 
		for (bl = bnames; bl != NULL; bl = g_list_next(bl)) {
			fprintf(dfp, ", '-' title '%s-allsky' with errorbars ", 
				(char *)(bl->data));
		}	
	fprintf(dfp, "\n");

	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (int) bl->data;
		osl = ofrs;
		while (osl != NULL) {
			ofr = O_FRAME(osl->data);
			osl = g_list_next(osl);
			if (ofr->band != band) 
				continue;
			if (ofr->zpointerr >= BIG_ERR)
				continue;
			if (ZPSTATE(ofr) < ZP_FIT_NOCOLOR)
				continue;
			n++;
			fprintf(dfp, "%.5f %.5f %.5f\n", ofr->mjd - mjdi,
				ofr->zpoint, ofr->zpointerr);
			}	
		fprintf(dfp, "e\n");
	}
	if (asfl != NULL) 
		for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
			band = (int) bl->data;
			osl = asfl;
			while (osl != NULL) {
				ofr = O_FRAME(osl->data);
				osl = g_list_next(osl);
				if (ofr->band != band) 
					continue;
				if (ofr->zpointerr >= BIG_ERR)
					continue;
				n++;
				fprintf(dfp, "%.5f %.5f %.5f\n", ofr->mjd - mjdi,
					ofr->zpoint, ofr->zpointerr);
			}	
			fprintf(dfp, "e\n");
		}
//	fprintf(dfp, "pause -1\n");
	g_list_free(bsl);
	g_list_free(asfl);
	return n;
}


/* create a plot of ofr residuals versus magnitude (as a gnuplot file) */
int ofrs_plot_zp_vs_am(FILE *dfp, GList *ofrs)
{
	GList *osl, *bl, *bsl = NULL;
	struct o_frame *ofr = NULL;
	int n = 0, i = 0;
	int band;
	GList *asfl = NULL, *bnames = NULL;

	g_return_val_if_fail(dfp != NULL, -1);
	plot_preamble(dfp);
	fprintf(dfp, "set xlabel 'Airmass'\n");
	fprintf(dfp, "set ylabel 'Magnitude'\n");
	fprintf(dfp, "set title 'Fitted Frame Zeropoints'\n");
//	fprintf(dfp, "set yrange [-1:1]\n");
//	fprintf(dfp, "set title '%s: band:%s mjd=%.5f'\n",
//		ofr->obs->objname, ofr->filter, ofr->mjd);
	fprintf(dfp, "plot  ");
	
	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band < 0) 
			continue;
//		d3_printf("*%d\n", ZPSTATE(ofr));
		if (ZPSTATE(ofr) == ZP_ALL_SKY) {
			asfl = g_list_prepend(asfl, ofr);
		}
		if (g_list_find(bsl, (gpointer)ofr->band) == NULL) {
			bsl = g_list_append(bsl, (gpointer)ofr->band);
			if (i > 0)
				fprintf(dfp, ", ");
			fprintf(dfp, "'-' title '%s' with errorbars ", 
				ofr->trans->bname);
			bnames = g_list_append(bnames, ofr->trans->bname);
			i++;
		}
	}
	if (asfl != NULL) 
		for (bl = bnames; bl != NULL; bl = g_list_next(bl)) {
			fprintf(dfp, ", '-' title '%s-allsky' with errorbars ", 
				(char *)(bl->data));
		}	
	fprintf(dfp, "\n");

	for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
		band = (int) bl->data;
		osl = ofrs;
		while (osl != NULL) {
			ofr = O_FRAME(osl->data);
			osl = g_list_next(osl);
			if (ofr->band != band) 
				continue;
			if (ofr->zpointerr >= BIG_ERR)
				continue;
			if (ZPSTATE(ofr) < ZP_FIT_NOCOLOR)
				continue;
			n++;
			fprintf(dfp, "%.5f %.5f %.5f\n", ofr->airmass,
				ofr->zpoint, ofr->zpointerr);
			}	
		fprintf(dfp, "e\n");
	}
	if (asfl != NULL) 
		for (bl = bsl; bl != NULL; bl = g_list_next(bl)) {
			band = (int) bl->data;
			osl = asfl;
			while (osl != NULL) {
				ofr = O_FRAME(osl->data);
				osl = g_list_next(osl);
				if (ofr->band != band) 
					continue;
				if (ofr->zpointerr >= BIG_ERR)
					continue;
				n++;
				fprintf(dfp, "%.5f %.5f %.5f\n", ofr->airmass,
					ofr->zpoint, ofr->zpointerr);
			}	
			fprintf(dfp, "e\n");
		}
//	fprintf(dfp, "pause -1\n");
	g_list_free(bsl);
	g_list_free(asfl);
	return n;
}


/* create a plot of ofr residuals versus color (as a gnuplot file) */
int ofrs_plot_residual_vs_col(struct mband_dataset *mbds, FILE *dfp, 
			      int band, GList *ofrs, int weighted)
{
	GList *sl, *osl;
	struct star_obs *sob;
	struct o_frame *ofr = NULL;
	int n = 0, i = 0;
	double v, u;

//	d3_printf("plot: band is %d\n", band);
	g_return_val_if_fail(dfp != NULL, -1);
	g_return_val_if_fail(mbds != NULL, -1);
	g_return_val_if_fail(band >= 0, -1);
	g_return_val_if_fail(mbds->trans[band].c1 >= 0, -1);
	g_return_val_if_fail(mbds->trans[band].c2 >= 0, -1);
	
	plot_preamble(dfp);
	fprintf(dfp, "set xlabel '%s-%s'\n", mbds->trans[mbds->trans[band].c1].bname, 
		mbds->trans[mbds->trans[band].c2].bname);
	if (weighted) {
		fprintf(dfp, "set ylabel 'Standard errors'\n");
	} else {
		fprintf(dfp, "set ylabel 'Residuals'\n");
	}
//	fprintf(dfp, "set ylabel '%s zeropoint fit weighted residuals'\n", mbds->bnames[band]);
//	fprintf(dfp, "set yrange [-1:1]\n");
	fprintf(dfp, "set title 'Transformation: %s = %s_i + %s_0 + %.3f * (%s - %s)'\n",
		mbds->trans[band].bname, mbds->trans[band].bname, 
		mbds->trans[band].bname, mbds->trans[band].k, 
		mbds->trans[mbds->trans[band].c1].bname, mbds->trans[mbds->trans[band].c2].bname);
//	fprintf(dfp, "set pointsize 1.5\n");
	fprintf(dfp, "plot ");
	
	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band != band) 
			continue;
		if (i > 0)
			fprintf(dfp, ", ");
//		fprintf(dfp, "'-' title '%s(%s)'", 
//			ofr->obs->objname, ofr->xfilter);
		fprintf(dfp, "'-' notitle ");
		i++;
	}
	fprintf(dfp, "\n");

	osl = ofrs;
	while (osl != NULL) {
		ofr = O_FRAME(osl->data);
		osl = g_list_next(osl);
		if (ofr->band != band) 
			continue;
		if (ofr->tweight < 0.0000000001) 
			continue;
		sl = ofr->sol;
		while(sl != NULL) {
			sob = STAR_OBS(sl->data);
			sl = g_list_next(sl);
			if (CATS_TYPE(sob->cats) != CAT_STAR_TYPE_APSTD)
				continue;
			if (sob->weight <= 0.00001)
				continue;
			n++;
			if (sob->ost->smagerr[mbds->trans[band].c1] < 9 
			    && sob->ost->smagerr[mbds->trans[band].c2] < 9) {
				v = sob->residual * sqrt(sob->nweight);
				u = sob->residual;
				clamp_double(&v, -STD_ERR_CLAMP, STD_ERR_CLAMP);
				clamp_double(&u, -RESIDUAL_CLAMP, RESIDUAL_CLAMP);
				if (weighted) 
					fprintf(dfp, "%.5f %.5f %.5f\n", 
						sob->ost->smag[mbds->trans[band].c1] 
						- sob->ost->smag[mbds->trans[band].c2],
						v, sob->imagerr);
				else 
					fprintf(dfp, "%.5f %.5f %.5f\n", 
						sob->ost->smag[mbds->trans[band].c1] 
						- sob->ost->smag[mbds->trans[band].c2],
						u, sob->imagerr);
			}
		}	
		fprintf(dfp, "e\n");
	}
//	fprintf(dfp, "pause -1\n");
	return n;
}

/* plot stars vs time */
int plot_star_mag_vs_time(FILE *dfp, GList *sobs)
{
	GList *osl, *sl = NULL;
	struct o_frame *ofr = NULL;
	struct star_obs *sob = NULL, *sol;
	double mjdi = 0.0;
	int n = 0, i = 0;
	int band;

	if (sobs == NULL)
		return -1;
	ofr = STAR_OBS(sobs->data)->ofr;
	band = ofr->band;
	mjdi = floor(ofr->mjd);

	g_return_val_if_fail(dfp != NULL, -1);
	plot_preamble(dfp);
	fprintf(dfp, "set xlabel 'Days from MJD %.1f'\n", mjdi);
//	fprintf(dfp, "set ylabel 'Magnitude'\n");
//	fprintf(dfp, "set title 'Fitted Frame Zeropoints'\n");
//	fprintf(dfp, "set format x \"%%.3f\"\n");
	fprintf(dfp, "set xtics autofreq\n");
//	fprintf(dfp, "set yrange [-1:1]\n");
//	fprintf(dfp, "set title '%s: band:%s mjd=%.5f'\n",
//		ofr->obs->objname, ofr->filter, ofr->mjd);
	fprintf(dfp, "plot  ");
	
	osl = sobs;
	while (osl != NULL) {
		sob = STAR_OBS(osl->data);
		ofr = sob->ofr;
		osl = g_list_next(osl);
		if (i > 0)
			fprintf(dfp, ", ");
		fprintf(dfp, "'-' title '%s(%s)' with errorbars ", 
			sob->cats->name, ofr->trans->bname);
		i++;
	}
	fprintf(dfp, "\n");

	osl = sobs;
	while (osl != NULL) {
		sob = STAR_OBS(osl->data);
		ofr = sob->ofr;
		osl = g_list_next(osl);
		if (ofr->band != band) 
			continue;
		
		for (sl = sob->ost->sol; sl != NULL; sl = sl->next) {
			sol = STAR_OBS(sl->data);
			if (sol->ofr->band != band)
				continue;
			n++;
			if (CATS_TYPE(sol->cats) == CAT_STAR_TYPE_APSTAR
			    && sol->err < BIG_ERR)
				fprintf(dfp, "%.5f %.5f %.5f\n", sol->ofr->mjd - mjdi,
					sol->mag, sol->err);
			else if (CATS_TYPE(sol->cats) == CAT_STAR_TYPE_APSTAR) 
				fprintf(dfp, "%.5f %.5f %.5f\n", sol->ofr->mjd - mjdi,
					sol->imag, sol->imagerr);
			else if (CATS_TYPE(sol->cats) == CAT_STAR_TYPE_APSTD
			    && sol->ofr->zpstate >= ZP_ALL_SKY)  
				fprintf(dfp, "%.5f %.5f %.5f\n", sol->ofr->mjd - mjdi,
					sol->residual, sqrt( sqr(sol->imagerr) +
						sqr(sol->ost->smagerr[band])));
			else if (CATS_TYPE(sol->cats) == CAT_STAR_TYPE_APSTD) 
				fprintf(dfp, "%.5f %.5f %.5f\n", sol->ofr->mjd - mjdi,
					sol->imag, sol->imagerr);
		}	
		fprintf(dfp, "e\n");
	}
	return n;
}

/* generate a vector plot of astrometric errors */
int stf_plot_astrom_errors(FILE *dfp, struct stf *stf, struct wcs *wcs) 
{
	GList *asl;
	double x, y;
	struct cat_star *cats;
	int n = 0;
	double r, me;

	n = stf_centering_stats(stf, wcs, &r, &me);

	if (n < 1)
		return 0;

	fprintf(dfp, "set xlabel 'pixels'\n");
	fprintf(dfp, "set ylabel 'pixels'\n");
	fprintf(dfp, "set nokey\n");
	fprintf(dfp, "set title 'Frame vs catalog positions (%.0fX) rms:%.2f, max:%.2f'\n",
		P_DBL(WCS_PLOT_ERR_SCALE), r, me);
	fprintf(dfp, "plot '-' with vector\n");

	asl = stf_find_glist(stf, 0, SYM_STARS);
	n = 0;
	for (; asl != NULL; asl = asl->next) {
		cats = CAT_STAR(asl->data);
		w_xypix(wcs, cats->ra, cats->dec, &x, &y);
		if (cats->flags & INFO_POS) {
			n++;
			fprintf (dfp, "%.3f %.3f %.3f %.3f\n", 
				 x, -y, P_DBL(WCS_PLOT_ERR_SCALE) * (cats->pos[POS_X] - x), 
				 -P_DBL(WCS_PLOT_ERR_SCALE) * (cats->pos[POS_Y] - y) );
		}
	}
	return n;
}
