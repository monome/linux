#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "fbtft.h"

#define DRVNAME     "fb_ssd1322"
#define WIDTH       128
#define HEIGHT      64
#define BPP         4
#define GAMMA_NUM   1
#define GAMMA_LEN   15
#define DEFAULT_GAMMA "1 1 1 1 1 2 2 3 3 4 4 5 5 6 6"

static int init_display(struct fbtft_par *par)
{
	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	par->fbtftops.reset(par);
	//-1, 0xFD, 0x12,	          /* Unlock OLED driver IC */
	write_reg(par, 0xAE);             /* Display OFF (blank) */
	write_reg(par, 0xB9);	          /* Select default linear grayscale */
	write_reg(par, 0xB3, 0x91);	  /* Display divide clockratio/frequency */
	write_reg(par, 0xCA, 0x3F);	  /* Multiplex ratio, 1/64, 64 COMS enabled */
	write_reg(par, 0xA2, 0x00);	  /* Set offset, the display map starting line is COM0 */
	write_reg(par, 0xA1, 0x00);       /* Set start line position */
	write_reg(par, 0xA0, 0x16, 0x11); /* Set remap, horiz address increment, disable colum address remap, */
	/*  enable nibble remap, scan from com[N-1] to COM0, disable COM split odd even */
	if (par->info->var.rotate == 180)
		write_reg(par, 0xa0, 0x04, 0x11);
	else
		write_reg(par, 0xa0, 0x16, 0x11);

	write_reg(par, 0xAB, 0x01);	  /* Select external VDD */
	write_reg(par, 0xB4, 0xA0, 0xFD); /* Display enhancement A, external VSL, enhanced low GS display quality */
	write_reg(par, 0xC1, 0x7F);	  /* Contrast current, 256 steps, default is 0x7F */
	write_reg(par, 0xC7, 0x0F);	  /* Master contrast current, 16 steps, default is 0x0F */
	write_reg(par, 0xB1, 0xF2);	  /* Phase Length */
	//-1, 0xD1, 0x82, 0x20	          /* Display enhancement B */
	write_reg(par, 0xBB, 0x1F);	  /* Pre-charge voltage */
	write_reg(par, 0xBE, 0x04);	  /* Set VCOMH */
	write_reg(par, 0xA6);		  /* Normal display */
	write_reg(par, 0xAF);		  /* Display ON */

	// Though the display only uses 4 bits per pixel, the 4-bit-pixel
	// has to be sent through SPI one byte at a time. Thus, we need a
	// buffer twice the size of vmem. For every byte read from the
	// screen_buffer, we write two to txbuf.
	par->txbuf.len = HEIGHT*WIDTH;
	par->txbuf.buf = devm_kzalloc(par->info->device, HEIGHT*WIDTH, GFP_KERNEL);

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	//int width = par->info->var.xres;
	//int offset = (480 - width) / 8;

	fbtft_par_dbg(DEBUG_SET_ADDR_WIN, par, "%s(xs=%d, ys=%d, xe=%d, ye=%d)\n", __func__, xs, ys, xe, ye);
	//write_reg(par, 0x15, offset, offset + (width / 4) - 1);
	//write_reg(par, 0x15, 28 + xs,28+xe);
	write_reg(par, 0x15, 28,91);
	write_reg(par, 0x75, ys, ye);
	//write_reg(par, 0x75, 0, 63);
	write_reg(par, 0x5c);
}

/*
	Grayscale Lookup Table
	GS1 - GS15
	The "Gamma curve" contains the relative values between the entries in the Lookup table.

	0 = Setting of GS1 < Setting of GS2 < Setting of GS3..... < Setting of GS14 < Setting of GS15

*/
static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	unsigned long tmp[GAMMA_LEN * GAMMA_NUM];
	int i, acc = 0;

	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	for (i = 0; i < GAMMA_LEN; i++) {
		if (i > 0 && curves[i] < 1) {
			dev_err(par->info->device,
				"Illegal value in Grayscale Lookup Table at index %d. " \
				"Must be greater than 0\n", i);
			return -EINVAL;
		}
		acc += curves[i];
		tmp[i] = acc;
		if (acc > 180) {
			dev_err(par->info->device,
				"Illegal value(s) in Grayscale Lookup Table. " \
				"At index=%d, the accumulated value has exceeded 180\n", i);
			return -EINVAL;
		}
	}

	//write_reg(par, 0xB8,
	//0,1,2,3,4,5,6,7,8,
	//9,10,15,25,30,35);
	//tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6], tmp[7],
	//tmp[8], tmp[9], tmp[10], tmp[11], tmp[12], tmp[13], tmp[14]);
	//write_reg(par, 0);
	

	return 0;
}

static int blank(struct fbtft_par *par, bool on)
{
	fbtft_par_dbg(DEBUG_BLANK, par, "%s(blank=%s)\n", __func__, on ? "true" : "false");
	if (on)
		write_reg(par, 0xAE);
	else
		write_reg(par, 0xAF);
	return 0;
}

static int write_vmem(struct fbtft_par *par, size_t offset, size_t len)
{
	int ret;
	u8 *vmem8 = (u8 *) par->info->screen_buffer;
	u8 *txbuf = (u8 *) par->txbuf.buf;

	if( vmem8 == NULL || txbuf == NULL ){
		ret = -1;
		dev_err(par->info->device, "%s: returning before attemping to access nulllptr and returned: %d\n", __func__, ret);
		return ret;
	}

	/* Set data line beforehand */
	gpiod_set_value(par->gpio.dc, 1);

	for (/* NOTHING */; offset < (par->txbuf.len / 2); offset++) {
		u8 pixel_1 = vmem8[offset] & 0x0F;
		u8 pixel_2 = vmem8[offset] & 0xF0;
		// Double up the nibbles in the bytes to be sure.
		*txbuf++ = pixel_1 | (pixel_1 << 4);
		*txbuf++ = pixel_2 | (pixel_2 >> 4);
	}

	ret = par->fbtftops.write(par, par->txbuf.buf, par->txbuf.len);
	if (ret < 0)
		dev_err(par->info->device, "%s: write failed and returned: %d\n", __func__, ret);

	return ret;
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.bpp = BPP,
	.txbuflen = -2, // need a larger buffer than vmem, which is implicitly
	                // not allowed. fbtft-core.c: line-737
	.gamma_num = GAMMA_NUM,
	.gamma_len = GAMMA_LEN,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.write_vmem = write_vmem,
		.set_addr_win  = set_addr_win,
		.blank = blank,
		.set_gamma = set_gamma,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "solomon,ssd1322", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:ssd1322");
MODULE_ALIAS("platform:ssd1322");

MODULE_DESCRIPTION("SSD1322 OLED Driver");
MODULE_AUTHOR("Ryan Press");
MODULE_LICENSE("GPL");
