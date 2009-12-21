
#define USHORT unsigned short
#define UCHAR unsigned char
#define BYTE char
#define ULONG unsigned long
#define LONG long
#define DLL_IMPORT
#define DWORD int
#define BOOL int
#define CALLBACK
#define UINT unsigned int
#define WPARAM short
#define LPARAM long
#define LRESULT long

struct t_sxccd_params
{
    USHORT hfront_porch;
    USHORT hback_porch;
    USHORT width;
    USHORT vfront_porch;
    USHORT vback_porch;
    USHORT height;
    float  pix_width;
    float  pix_height;
    USHORT color_matrix;
    BYTE   bits_per_pixel;
    BYTE   num_serial_ports;
    BYTE   extra_caps;
    BYTE   vclk_delay;
};

USHORT     *VideoImage;
UCHAR      *LogVideoImage;
USHORT xVideoBin;
USHORT yVideoBin;

struct t_sxccd_params ccd_params;

int exposure;

extern int probe (GPPort *port);
extern int Reset (GPPort *port);
extern int echo (GPPort *port);
extern int get_params (GPPort *port, struct t_sxccd_params *params);
extern int expose (GPPort *port);

int write_jpeg_file( char *filename,
		     unsigned char *raw_image,
		     int width,
		     int height,
		     int bytes_per_pixel,   /* or 1 for GRACYSCALE images */
		     int color_space); /* JCS_RGB for colour or JCS_GRAYSCALE for grayscale images */

void Arrayswap (short *Imptr1, short *Imptr2, short height, short width, short *end1, short *end2);
void ArrayswapLog (unsigned char *Imptr1, unsigned short *Imptr2, short height, short width, unsigned char *end1, unsigned short *end2);
