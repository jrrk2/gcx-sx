## Process this file with automake to produce Makefile.in

INCLUDES = \
        -I/usr/local/include/gtk-1.2 -I/usr/local/include/glib-1.2 -I/usr/local/lib/glib/include

noinst_LIBRARIES = libccd.a

libccd_a_SOURCES = \
	ccd_frame.c median.c badpix.c \
	edb.c aphot.c worldpos.c sources.c \
	warp.c recipy.c errlog.c ccd.h

CLEANFILES = *~
