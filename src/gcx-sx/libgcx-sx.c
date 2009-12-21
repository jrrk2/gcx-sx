#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <jpeglib.h>
#include <assert.h>
#include <usb.h>
#include "gcx-sx-port-info-list.h"
#include "gcx-sx-port.h"
#include "gcx-sx-port-library.h"
#include "gcx-sx-port-info-list.h"
#include "gcx-sx-port.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include <dirent.h>
#include <string.h>

#include <usb.h>

#include "gcx-sx-port.h"
#include "gcx-sx-port-result.h"
#include "gcx-sx-port-log.h"

#include "libgcx-sx-main.h"

/*
 * Control request fields.
 */
#define USB_REQ_TYPE                0
#define USB_REQ                     1
#define USB_REQ_VALUE_L             2
#define USB_REQ_VALUE_H             3
#define USB_REQ_INDEX_L             4
#define USB_REQ_INDEX_H             5
#define USB_REQ_LENGTH_L            6
#define USB_REQ_LENGTH_H            7
#define USB_REQ_DATA                8
#define USB_REQ_DIR(r)              ((r)&(1<<7))
#define USB_REQ_DATAOUT             0x00
#define USB_REQ_DATAIN              0x80
#define USB_REQ_KIND(r)             ((r)&(3<<5))
#define USB_REQ_VENDOR              (2<<5)
#define USB_REQ_STD                 0
#define USB_REQ_RECIP(r)            ((r)&31)
#define USB_REQ_DEVICE              0x00
#define USB_REQ_IFACE               0x01
#define USB_REQ_ENDPOINT            0x02
#define USB_DATAIN                  0x80
#define USB_DATAOUT                 0x00
/*
 * CCD camera control commands.
 */
#define SXUSB_GET_FIRMWARE_VERSION  255
#define SXUSB_ECHO                  0
#define SXUSB_CLEAR_PIXELS          1
#define SXUSB_READ_PIXELS_DELAYED   2
#define SXUSB_READ_PIXELS           3
#define SXUSB_SET_TIMER             4
#define SXUSB_GET_TIMER             5
#define SXUSB_RESET                 6
// #define SXUSB_SET_CCD_PARMS      7
#define SXUSB_GET_CCD_PARMS         8
#define SXUSB_SET_STAR2K            9
#define SXUSB_WRITE_SERIAL_PORT     10
#define SXUSB_READ_SERIAL_PORT      11
#define SXUSB_SET_SERIAL            12
#define SXUSB_GET_SERIAL            13
#define SXUSB_CAMERA_MODEL          14

#define SXCCD_EXP_FLAGS_FIELD_ODD       1
#define SXCCD_EXP_FLAGS_FIELD_EVEN      2
#define SXCCD_EXP_FLAGS_FIELD_BOTH      (SXCCD_EXP_FLAGS_FIELD_EVEN|SXCCD_EXP_FLAGS_FIELD_ODD)

void enumerate_usb(void)
{
 int n = 0;
 int retval;
 GPPortInfo info;
 GPPortInfoList *list;
 retval = gp_port_info_list_new (&list);
 retval = gp_port_library_list (list);
 do {
     retval = gp_port_info_list_get_info (list, n, &info);
     printf("%s %s\n", info.name, info.path); // , port.pl->d.descriptor.idVendor, port.pl->d.descriptor.idProduct);
     ++n;
 }
 while (retval == GP_OK);
 gp_port_info_list_free(list);
}

int probe(GPPort *port)
     {
     int retval2;
     retval2 = gp_port_open (port);
     if (retval2 < 0)
       printf("Error: %s\n", gp_port_get_error(port));
     return 0;
     }

int echo(GPPort *port)
{
     int i;
     int retval2;
     int retval3;
     int request;
     int value;
     int index;
     char setup_data2[21];
     char setup_data3[21];
     request = SXUSB_ECHO;
     value = 0;
     index = 0;
     strncpy(setup_data2, "Hello there Starlight", sizeof(setup_data2));
     memset(setup_data3, 0, sizeof(setup_data3));
     retval2 = gp_port_usb_msg_write(port, request, value, index, setup_data2, sizeof(setup_data2));
     if (retval2 < 0) return retval2;
     printf("Echo returned %d\n", retval2);
     retval3 = gp_port_read(port, setup_data3, sizeof(setup_data3));
     if (retval3 < 0) return retval3;
     printf("Echo returned %d\n", retval3);
     for (i = 0; i < retval2; i++)
       if (isprint(setup_data3[i]))
	 printf("%c ", setup_data3[i]);
       else
	 printf("%.02X ", (unsigned char)(setup_data3[i]));
     putchar('\n');
     return 0;
}

int get_params(GPPort *port, struct t_sxccd_params *params)
{
    int retval2;
    int request;
    int value;
    int index;
    unsigned char setup_data[21];
    request = SXUSB_GET_CCD_PARMS;
    value = 0;
    index = 0;
    memset(setup_data, 0, sizeof(setup_data));
    retval2 = gp_port_usb_msg_read(port, request, value, index, (char *)setup_data, 17);
    if (retval2 < 0) return retval2;
    printf("Get Params returned %d\n", retval2);
    params->hfront_porch = setup_data[0];
    params->hback_porch = setup_data[1];
    params->width = setup_data[2] | (setup_data[3] << 8);
    params->vfront_porch = setup_data[4];
    params->vback_porch = setup_data[5];
    params->height = setup_data[6] | (setup_data[7] << 8);
    params->pix_width = (setup_data[8] | (setup_data[9] << 8)) / 256.0;
    params->pix_height = (setup_data[10] | (setup_data[11] << 8)) / 256.0;
    params->color_matrix = setup_data[12] | (setup_data[13] << 8);
    params->bits_per_pixel = setup_data[14];
    params->num_serial_ports = setup_data[15];
    params->extra_caps = setup_data[16];
    printf("hfront_porch = %d\n", params->hfront_porch);
    printf("hback_porch = %d\n", params->hback_porch);
    printf("width = %d\n", params->width);
    printf("vfront_porch = %d\n", params->vfront_porch);
    printf("vback_porch = %d\n", params->vback_porch);
    printf("height = %d\n", params->height);
    printf("pix_width = %f microns\n", params->pix_width);
    printf("pix_height = %f microns\n", params->pix_height);
    printf("color_matrix = %d\n", params->color_matrix);
    printf("bits_per_pixel = %d\n", params->bits_per_pixel);
    printf("num_serial_ports = %d\n", params->num_serial_ports);
    printf("extra_caps = %d\n", params->extra_caps);
    printf("vclk_delay = %d\n", params->vclk_delay);
    fflush(stdout);
    return 0;
     }

int Reset(GPPort *port)
{
     int retval2;
     int request;
     int value;
     int index;
     unsigned char setup_data[1];
     request = SXUSB_RESET;
     value = 0;
     index = 0;
     retval2 = gp_port_usb_msg_write(port, request, value, index, (char *)setup_data, 0);
     if (retval2 < 0) return retval2;
     printf("Reset returned %d\n", retval2);
     return 0;
}

int ExposePixels(GPPort *port, USHORT flags, USHORT camIndex, USHORT xoffset, USHORT yoffset,
		 USHORT width, USHORT height, USHORT xbin, USHORT ybin, ULONG msec)
{
     int retval2;
     int request;
     int value;
     int index;
     unsigned char setup_data[14];
     request = SXUSB_READ_PIXELS_DELAYED;
     value = flags;
     index = camIndex;
     memset(setup_data, 0, sizeof(setup_data));
     setup_data[0] = xoffset & 0xFF;
     setup_data[1] = xoffset >> 8;
     setup_data[2] = yoffset & 0xFF;
     setup_data[3] = yoffset >> 8;
     setup_data[4] = width & 0xFF;
     setup_data[5] = width >> 8;
     setup_data[6] = height & 0xFF;
     setup_data[7] = height >> 8;
     setup_data[8] = xbin;
     setup_data[9] = ybin;
     setup_data[10] = msec;
     setup_data[11] = msec >> 8;
     setup_data[12] = msec >> 16;
     setup_data[13] = msec >> 24;
     retval2 = gp_port_usb_msg_write(port, request, value, index, (char *)setup_data, sizeof(setup_data));
     return retval2;
}

int ReadPixels(GPPort *port, USHORT *pixels, ULONG count)
{
  int retval;
  int bytread = 0;
  int siz = count * sizeof(USHORT);
  while (siz > 512)
    {
      retval = gp_port_read(port, bytread+(char *)pixels, 512);
      if (retval <= 0) return retval;
      bytread += retval;
      siz -= retval;
    }
  retval = gp_port_read(port, bytread+(char *)pixels, siz);
  if (retval < 0) return -1;
  bytread += retval;
  assert(bytread == count * sizeof(USHORT));
  return bytread;
}

int expose(GPPort *port)
{
	enum {camIndex = 0};
	USHORT xoffset = 0;
	USHORT yoffset = 0;

    /* begin next exposure */
    ExposePixels(port, SXCCD_EXP_FLAGS_FIELD_BOTH, camIndex, xoffset, yoffset, ccd_params.width, ccd_params.height, xVideoBin, yVideoBin, exposure);
    usleep(exposure * 2000);
    ReadPixels(port, VideoImage, ccd_params.width*ccd_params.height/xVideoBin/yVideoBin);
    return 0;
}
