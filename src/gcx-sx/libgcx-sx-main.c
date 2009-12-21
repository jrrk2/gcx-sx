#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <jpeglib.h>
#include <usb.h>
#include "gcx-sx-port-info-list.h"
#include "gcx-sx-port.h"
#include "gcx-sx-port-library.h"
#include "gcx-sx-port-info-list.h"
#include "gcx-sx-port-info-list.h"
#include "gcx-sx-port-portability.h"
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

#define SX_VENDOR_ID	0x1278
#define SX_PRODUCT_ID  0x0325

int main(int argc, char **argv)
{
 int retval2;
 GPPortInfo info;
 GPPort *port;
 retval2 = gp_port_new(&port);
 memset(&info, 0, sizeof(info));
 strcpy(info.path, "usb:");
 info.type = GP_PORT_USB;
 gp_port_set_info(port, info);
 retval2 = gp_port_usb_find_device(port, SX_VENDOR_ID, SX_PRODUCT_ID);
 if (!retval2)
   {
     retval2 = probe(port);
     if (retval2 < 0) return retval2;
     retval2 = Reset(port);
     if (retval2 < 0) return retval2;
     retval2 = echo(port);
     if (retval2 < 0) return retval2;
     retval2 = get_params(port, &ccd_params);
     if (retval2 < 0) return retval2;
     if (!VideoImage)
       VideoImage = (USHORT *)calloc(ccd_params.width*ccd_params.height,sizeof(USHORT));
     if (!LogVideoImage)
       LogVideoImage = (UCHAR *)calloc(ccd_params.width,ccd_params.height);
            /*
             * Read pixels from camera.
             */
     xVideoBin = 1;
     yVideoBin = 1;
     exposure = 10;
     retval2 = expose(port);
     if (retval2 < 0) return retval2;
     ArrayswapLog(LogVideoImage, VideoImage, ccd_params.height/yVideoBin, ccd_params.width/xVideoBin,
		LogVideoImage+ccd_params.width/xVideoBin*ccd_params.height/yVideoBin,
		VideoImage+ccd_params.width*ccd_params.height);
     retval2 = write_jpeg_file("dump.jpg",
		     LogVideoImage,
		     ccd_params.width/xVideoBin,
		     ccd_params.height/yVideoBin,
		     1,   /* or 1 for GRAYSCALE images */
		     JCS_GRAYSCALE); /* JCS_RGB for colour or JCS_GRAYSCALE for grayscale images */
     
   }
 return 0;
}
