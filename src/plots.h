#ifdef _PLOTS_H_
#else

int ofrs_plot_residual_vs_mag(FILE *dfp, GList *ofrs, int weighted);
int ofrs_plot_residual_vs_col(struct mband_dataset *mbds, FILE *dfp, 
			      int band, GList *ofrs, int weighted);
int ofrs_plot_zp_vs_time(FILE *dfp, GList *ofrs);
int ofrs_plot_zp_vs_am(FILE *dfp, GList *ofrs);
int plot_star_mag_vs_time(FILE *plfp, GList *sobs);
int stf_plot_astrom_errors(FILE *dfp, struct stf *stf, struct wcs *wcs);
int stf_centering_stats(struct stf *stf, struct wcs *wcs, double *rms, double *max);

#endif
