## Process this file with automake to produce Makefile.in

SUBDIRS = ccd gsc gcx-sx

PAM_CPP_FLAGS=-g 

INCLUDES = \
	-I/usr/local/include/gtk-1.2 -I/usr/local/include/glib-1.2 -I/usr/local/lib/glib/include

bin_PROGRAMS = gcx

gcx_SOURCES = \
	callbacks.c callbacks.h \
	camera.c cameragui.c cameragui.h camera.h catalogs.c catalogs.h \
	filegui.c filegui.h framelist.c gcx.c gcx.h gui.c gui.h imadjust.c \
	interface.c interface.h obsdata.c obsdata.h obslist.c \
	obslist.h params.c paramsgui.c params.h photometry.c \
	showimage.c sourcesdraw.c sourcesdraw.h staredit.c \
	support.c support.h tele.c textgui.c wcs.c wcs.h x11ops.c x11ops.h\
	helpmsg.c helpmsg.h wcsedit.c recipy.c recipy.h recipygui.c symbols.h\
	tycho2.c tycho2.h report.c sidereal_time.c nutation.c nutation.h\
	sidereal_time.h	reduce.c reduce.h misc.c misc.h reducegui.c\
	initparams.c starlist.c	fwheel.c fwheel.h guidegui.c \
	guide.c guide.h multiband.c multiband.h mbandgui.c plots.c plots.h \
	mbandrep.c starfile.c getline.h


gcx_LDADD = /usr/local/lib/libglib.a /usr/local/lib/libgtk.a ccd/libccd.a gsc/libgsc.a gcx-sx/libgcx-sx.a /usr/local/lib/libgdk.a \
	/usr/local/lib/libglib.a /usr/local/lib/libgtk.a /usr/local/lib/libgdk.a /usr/local/lib/libgmodule.a \
	-lX11 -lXext -lusb -lm

CLEANFILES = *~
