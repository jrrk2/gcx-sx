C = gcx-sx-port.c gcx-sx-port-info-list.c jpeg_sample.c libusb.c libgcx-sx-main.c libgcx-sx.c SwapMX25data.c
U = libgcx-sx.o libusb.o gcx-sx-port-info-list.o gcx-sx-port.o SwapMX25data.o jpeg_sample.o
CXXFLAGS = -g -fno-inline -Wall
CFLAGS = -g -Wall

all: tester

tester: libgcx-sx-main.o libgcx-sx.a
	$(CC) -o $@ $(CFLAGS) libgcx-sx-main.o libgcx-sx.a -lusb -ljpeg -lm

libgcx-sx.a: $U
	ar rc $@ $U

gcx-sx-port.o: gcx-sx-port.c \
  gcx-sx-port-info-list.h \
  gcx-sx-port.h gcx-sx-port-library.h gcx-sx-port-portability.h \
  gcx-sx-port-result.h gcx-sx-port-log.h
gcx-sx-port-info-list.o: gcx-sx-port-info-list.c gcx-sx-port-info-list.h \
  gcx-sx-port-result.h gcx-sx-port.h \
  gcx-sx-port-library.h gcx-sx-port-log.h
jpeg_sample.o: jpeg_sample.c
libusb.o: libusb.c gcx-sx-port-info-list.h gcx-sx-port.h \
  gcx-sx-port-library.h \
  gcx-sx-port-result.h \
  gcx-sx-port-log.h
libgcx-sx-main.o: libgcx-sx-main.c \
  gcx-sx-port-info-list.h gcx-sx-port.h gcx-sx-port-library.h \
  gcx-sx-port-portability.h \
  gcx-sx-port-result.h gcx-sx-port-log.h libgcx-sx-main.h
libgcx-sx.o: libgcx-sx.c \
  gcx-sx-port-info-list.h gcx-sx-port.h gcx-sx-port-library.h \
  gcx-sx-port-result.h gcx-sx-port-log.h libgcx-sx-main.h
SwapMX25data.o: SwapMX25data.c
