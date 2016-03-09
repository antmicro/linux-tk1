/*
 * based on OV5640 driver and TC358743 driver for i.MX6
 *
 * author: Antmicro Ltd
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>

#define PLLCTL0                               0x0020
#define PLLCTL1                               0x0022

#define MASK_PLL_PRD                          0xf000
#define SET_PLL_PRD(prd) ((((prd) - 1) << 12) & MASK_PLL_PRD)

#define MASK_PLL_FBD                          0x01ff
#define SET_PLL_FBD(fbd) (((fbd) - 1) & MASK_PLL_FBD)

#define MASK_CKEN                             0x0010
#define MASK_RESETB                           0x0002
#define MASK_PLL_EN                           0x0001

#define MASK_NOL_1                            0
#define MASK_NOL_2                            2
#define MASK_NOL_3                            4
#define MASK_NOL_4                            6

/* this is true for 27 MHz refclk */
#define REFCLK_PRE_DIVIDER                    9
#define SET_PLL_FREQ(x) (SET_PLL_PRD(REFCLK_PRE_DIVIDER) | SET_PLL_FBD(DIV_ROUND_UP(x,3)))

#define FRS_BW_25                             0x0
#define FRS_BW_33                             0x1
#define FRS_BW_50                             0x2
#define FRS_BW_MAX                            0x3

#define MASK_PLL_FRS                          0x0f00
#define SET_PLL_FRS(frs,bw) (((frs | bw) << 8) & MASK_PLL_FRS)
#define GET_FRS(x) (x < 125) ? 0xC : (x < 250) ? 0x8 : (x < 500) ? 0x4 : 0x0

#define HCNT_MASK                             0x3f
#define CALC_HDR_CNT(prep, zero) ((zero & HCNT_MASK) << 8) | (prep & HCNT_MASK)

#define SYS_STATUS                            0x8520
#define CSI_STATUS                            0x0410
#define MASK_S_WSYNC                          0x0400
#define VI_STATUS0                            0x8521
#define VI_STATUS1                            0x8522
#define AU_STATUS0                            0x8523
#define VI_STATUS2                            0x8525
#define VI_STATUS3                            0x8528

#define EDID_MODE                             0x85C7
#define MASK_EDID_SPEED                       0x40
#define MASK_EDID_MODE                        0x03
#define MASK_EDID_MODE_DISABLE                0x00
#define MASK_EDID_MODE_DDC2B                  0x01
#define MASK_EDID_MODE_E_DDC                  0x02
#define EDID_LEN1                             0x85CA
#define EDID_LEN2                             0x85CB


#define SYSCTL                                0x0002
#define CONFCTL                               0x0004
#define MASK_AUTOINDEX                        0x0004
#define FIFOCTL                               0x0006
#define INTSTATUS                             0x0014
#define INTMASK                               0x0016
#define PHY_RST                               0x8535
#define MASK_RESET_CTRL                       0x01 /* Reset active low */

#define HPD_CTL                               0x8544
#define MASK_HPD_CTL0                         0x10
#define MASK_HPD_OUT0                         0x01

#define BKSV                                  0x8800

#define CSI_CONFW                             0x0500
#define MASK_MODE_SET                         0xa0000000
#define MASK_ADDRESS_CSI_CONTROL              0x03000000
#define MASK_CSI_MODE                         0x8000
#define MASK_HTXTOEN                          0x0400
#define MASK_TXHSMD                           0x0080
#define MASK_HSCKMD                           0x0020
#define HSTXVREGEN                            0x0234
#define TXOPTIONCNTRL                         0x0238
#define MASK_CONTCLKMODE                      0x00000001
#define STARTCNTRL                            0x0204
#define MASK_START                            0x00000001
#define CLW_CNTRL                             0x0140
#define MASK_CLW_LANEDISABLE                  0x0001

#define D0W_CNTRL                             0x0144
#define MASK_D0W_LANEDISABLE                  0x0001

#define D1W_CNTRL                             0x0148
#define MASK_D1W_LANEDISABLE                  0x0001

#define D2W_CNTRL                             0x014C
#define MASK_D2W_LANEDISABLE                  0x0001

#define D3W_CNTRL                             0x0150
#define MASK_D3W_LANEDISABLE                  0x0001

#define LINEINITCNT                           0x0210
#define LPTXTIMECNT                           0x0214
#define TCLK_HEADERCNT                        0x0218
#define TCLK_TRAILCNT                         0x021C
#define THS_HEADERCNT                         0x0220
#define TWAKEUP                               0x0224
#define TCLK_POSTCNT                          0x0228
#define THS_TRAILCNT                          0x022C

#define CSI_START                             0x0518
#define MASK_STRT                             0x00000001

#define PHY_CTL1                              0x8532
#define PHY_CTL2                              0x8533 /* Not in REF_01 */
#define PHY_BIAS                              0x8536 /* Not in REF_01 */
#define PHY_CSQ                               0x853F /* Not in REF_01 */
#define DDC_CTL                               0x8543

#define SYS_FREQ0                             0x8540
#define SYS_FREQ1                             0x8541

#define NCO_F0_MOD                            0x8670
#define MASK_NCO_F0_MOD_27MHZ                 0x01


#define LOCKDET_REF0                          0x8630
#define LOCKDET_REF1                          0x8631
#define LOCKDET_REF2                          0x8632

#define SYS_INT                               0x8502
#define MASK_I_DDC                            0x01
#define SYS_INTM                              0x8512
#define PACKET_INTM                           0x8514
#define CBIT_INTM                             0x8515
#define AUDIO_INTM                            0x8516
#define PHY_CTL0                              0x8531
#define MASK_PHY_CTL                          0x01
#define ANA_CTL                               0x8545
#define MASK_APPL_PCSX                        0x30
#define MASK_APPL_PCSX_NORMAL                 0x30

#define MASK_ANALOG_ON                        0x01


#define AVM_CTL                               0x8546


#define MASK_PHY_AUTO_RST4                    0x04
#define MASK_PHY_AUTO_RST3                    0x02
#define MASK_PHY_AUTO_RST2                    0x01

#define VOUT_SET2                             0x8573
#define MASK_VOUT_422FIL_100                  0x40
#define MASK_SEL422                           0x80
#define MASK_VOUTCOLORMODE_AUTO               0x01
#define MASK_VOUTCOLORMODE_THROUGH            0x00
#define MASK_VOUTCOLORMODE_MANUAL             0x03

#define VOUT_SET3                             0x8574
#define MASK_VOUT_EXTCNT                      0x08

#define PK_INT_MODE                           0x8709
#define NO_PKT_LIMIT                          0x870B
#define NO_PKT_CLR                            0x870C
#define ERR_PK_LIMIT                          0x870D
#define NO_GDB_LIMIT                          0x9007
#define NO_PKT_LIMIT2                         0x870E

#define INIT_END                              0x854A
#define MASK_INIT_END                         0x01


#define HDCP_MODE                             0x8560
#define HDCP_REG1                             0x8563
#define HDCP_REG2                             0x8564
#define HDCP_REG3                             0x85D1

#define EDID_LEN2                             0x85CB
#define EDID_MODE                             0x85C7

#define FH_MIN0                               0x85AA
#define FH_MIN1                               0x85AB
#define FH_MAX0                               0x85AC
#define FH_MAX1                               0x85AD

#define DE_WIDTH_H_LO                         0x8582
#define DE_WIDTH_H_HI                         0x8583
#define DE_WIDTH_V_LO                         0x8588
#define DE_WIDTH_V_HI                         0x8589
#define H_SIZE_LO                             0x858A
#define H_SIZE_HI                             0x858B
#define V_SIZE_LO                             0x858C
#define V_SIZE_HI                             0x858D
#define FV_CNT_LO                             0x85A1
#define FV_CNT_HI                             0x85A2


#define MASK_IRRST                            0x0800
#define MASK_CECRST                           0x0400
#define MASK_CTXRST                           0x0200
#define MASK_HDMIRST                          0x0100

#define MASK_AUDCHNUM_2                       0x0c00
#define MASK_AUTOINDEX                        0x0004
#define MASK_ABUFEN                           0x0002
#define MASK_VBUFEN                           0x0001
#define MASK_AUDOUTSEL_I2S                    0x0010

#define MASK_YCBCRFMT                         0x00c0

#define MASK_YCBCRFMT_422_12_BIT              0x0040
#define MASK_YCBCRFMT_COLORBAR                0x0080
#define MASK_YCBCRFMT_422_8_BIT               0x00c0

#define MASK_YCBCRFMT_444                     0x0000

#define MASK_INFRMEN                          0x0020
#define PHY_EN                                0x8534
#define MASK_ENABLE_PHY                       0x01

#define MASK_PWRISO                           0x8000

#define VI_MODE                               0x8570
#define MASK_RGB_DVI                          0x8

#define VI_REP                                0x8576
#define MASK_VOUT_COLOR_SEL                   0xe0
#define MASK_VOUT_COLOR_RGB_FULL              0x00
#define MASK_VOUT_COLOR_RGB_LIMITED           0x20
#define MASK_VOUT_COLOR_601_YCBCR_FULL        0x40
#define MASK_VOUT_COLOR_601_YCBCR_LIMITED     0x60
#define MASK_VOUT_COLOR_709_YCBCR_FULL        0x80
#define MASK_VOUT_COLOR_709_YCBCR_LIMITED     0xa0
#define MASK_VOUT_COLOR_FULL_TO_LIMITED       0xc0
#define MASK_VOUT_COLOR_LIMITED_TO_FULL       0xe0
#define MASK_IN_REP_HEN                       0x10
#define MASK_IN_REP                           0x0f

/* status regs */
#define SYS_STATUS                            0x8520
#define MASK_S_SYNC                           0x80
#define MASK_S_AVMUTE                         0x40
#define MASK_S_HDCP                           0x20
#define MASK_S_HDMI                           0x10
#define MASK_S_PHY_SCDT                       0x08
#define MASK_S_PHY_PLL                        0x04
#define MASK_S_TMDS                           0x02
#define MASK_S_DDC5V                          0x01
#define CSI_STATUS                            0x0410
#define MASK_S_WSYNC                          0x0400
#define MASK_S_TXACT                          0x0200
#define MASK_S_RXACT                          0x0100
#define MASK_S_HLT                            0x0001
#define VI_STATUS1                            0x8522
#define MASK_S_V_GBD                          0x08
#define MASK_S_DEEPCOLOR                      0x0c
#define MASK_S_V_422                          0x02
#define MASK_S_V_INTERLACE                    0x01
#define AU_STATUS0                            0x8523
#define MASK_S_A_SAMPLE                       0x01
#define VI_STATUS3                            0x8528
#define MASK_S_V_COLOR                        0x1e
#define MASK_LIMITED                          0x01

#define HDMI_DET                              0x8552 /* Not in REF_01 */
#define MASK_HDMI_DET_MOD1                    0x80
#define MASK_HDMI_DET_MOD0                    0x40
#define MASK_HDMI_DET_V                       0x30
#define MASK_HDMI_DET_V_SYNC                  0x00
#define MASK_HDMI_DET_V_ASYNC_25MS            0x10
#define MASK_HDMI_DET_V_ASYNC_50MS            0x20
#define MASK_HDMI_DET_V_ASYNC_100MS           0x30
#define MASK_HDMI_DET_NUM                     0x0f

#define HV_RST                                0x85AF /* Not in REF_01 */
#define MASK_H_PI_RST                         0x20
#define MASK_V_PI_RST                         0x10

#define MASK_DDC5V_MODE_100MS                 2

#define VI_MUTE                               0x857F
#define MASK_AUTO_MUTE                        0xc0
#define MASK_VI_MUTE                          0x10

#define FORCE_MUTE                            0x8600
#define MASK_FORCE_AMUTE                      0x10

#define BCAPS                                 0x8840
#define BSTATUS1                              0x8842

#define MASK_MAX_EXCED                        0x08
#define MASK_REPEATER                         0x40
#define MASK_READY                            0x20

#define MASK_HDMI_RSVD                        0x80

#define VOUT_SET0                             0x8571
#define VOUT_SET1                             0x8572

#define CMD_AUD                               0x8601
#define MASK_CMD_BUFINIT                      0x04
#define MASK_CMD_LOCKDET                      0x02
#define MASK_CMD_MUTE                         0x01

#define MASK_FORCE_DMUTE                      0x01

#define ERR_PK_LIMIT                          0x870D
#define NO_PKT_LIMIT2                         0x870E
#define PK_AVI_0HEAD                          0x8710
#define PK_AVI_1HEAD                          0x8711
#define PK_AVI_2HEAD                          0x8712
#define PK_AVI_0BYTE                          0x8713
#define PK_AVI_1BYTE                          0x8714
#define PK_AVI_2BYTE                          0x8715
#define PK_AVI_3BYTE                          0x8716
#define PK_AVI_4BYTE                          0x8717
#define PK_AVI_5BYTE                          0x8718
#define PK_AVI_6BYTE                          0x8719
#define PK_AVI_7BYTE                          0x871A
#define PK_AVI_8BYTE                          0x871B
#define PK_AVI_9BYTE                          0x871C
#define PK_AVI_10BYTE                         0x871D
#define PK_AVI_11BYTE                         0x871E
#define PK_AVI_12BYTE                         0x871F
#define PK_AVI_13BYTE                         0x8720
#define PK_AVI_14BYTE                         0x8721
#define PK_AVI_15BYTE                         0x8722
#define PK_AVI_16BYTE                         0x8723

struct reg_value {
	u16 addr;
	u32 val;
	u32 flags;
};

struct _reg_size
{
	u16 startaddr, endaddr;
	int size;
}

tc358743_read_reg_size [] =
{
	{0x0000, 0x005a, 2},
	{0x0140, 0x0150, 4},
	{0x0204, 0x0238, 4},
	{0x040c, 0x0418, 4},
	{0x044c, 0x0454, 4},
	{0x0500, 0x0518, 4},
	{0x0600, 0x06cc, 4},
	{0x7000, 0x7100, 2},
	{0x8500, 0x8bff, 1},
	{0x8c00, 0x8fff, 4},
	{0x9000, 0x90ff, 1},
	{0x9100, 0x92ff, 1},
	{0, 0, 0},
};

typedef struct timings_regs {
	uint32_t frequency;
	uint32_t line_init_cnt;
	uint32_t tlpx_time_cnt;

	uint32_t tclk_prepare_cnt;
	uint32_t tclk_zero_cnt;

	uint32_t tclk_trail_cnt; /* Don't care in continous clk mode */

	uint32_t ths_prepare_cnt;
	uint32_t ths_zero_cnt;

	uint32_t twakeup_cnt;
	uint32_t tclk_post_cnt; /* Don't care in continous clk mode */

	uint32_t ths_trail_cnt;

	uint32_t fifo_delay;
} timings_regs;

timings_regs timings[] = {
	/* 640x480 @ 75 */
	{  500, 3328, 1, 1,  7, 0, 1,  0, 16384, 5, 0, 256},
	/* 800x600 @ 75 */
	{  594, 3712, 3, 3, 20, 0, 3,  1, 18562, 8, 2, 180},
	/* 1024x768 @ 75 */
	{  600, 7689, 3, 2, 20, 0, 3,  0, 18944, 8, 2, 128},
	/* 1280x720 @ 60 */
	{  825, 5160, 5, 4, 29, 0, 5,  5, 18000, 0, 4, 180},
	/* 1280x1024 @ 75 and 1920x1080 @ 60 */
	{ 1075, 6000, 5, 4, 29, 0, 5, 80, 18000, 0, 4, 256},
	{},
};

static struct reg_value tc358743_reset[] = {
	{CONFCTL, 0, 100},

	{PHY_RST, 0x0, 100},
	{PHY_RST, 0x1, 100},
	{PHY_EN, 0, 100},
	{PHY_EN, 1, 100},

	/* reset */
	{SYSCTL, MASK_IRRST | MASK_CECRST | MASK_CTXRST | MASK_HDMIRST, 100},
	{SYSCTL, 0x00000000, 1000},

	/* set ref frequency to 27 MHz */
	{SYS_FREQ0, (2700) & 0xFF, 0},
	{SYS_FREQ1, (2700 >> 8) & 0xFF, 0},

	{LOCKDET_REF0, (270000) & 0xFF, 0},
	{LOCKDET_REF1, (270000 >> 8) & 0xFF, 0},
	{LOCKDET_REF2, (270000 >> 16) & 0xFF, 0},
	{FH_MIN0, (270) & 0xFF, 0},
	{FH_MIN1, (270 >> 8) & 0xFF, 0},
	{FH_MAX0, (27 * 66) & 0xFF, 0},
	{FH_MAX1, ((27 * 66) >> 8) & 0xFF, 0},
	{NCO_F0_MOD, MASK_NCO_F0_MOD_27MHZ, 0},
};

static timings_regs tc358743_get_best_timings(uint32_t frequency) {
	int count = 0;
	timings_regs best_timings = {};
	while (1) {
		if (timings[count].frequency == 0) break;
		best_timings = timings[count];
		if (timings[count].frequency >= frequency) break;
		count++;
	}
	return best_timings;
}

static struct reg_value tc358743_2lanes_start[] = {
	{CLW_CNTRL, 0x0000, 0},

	/* Make all lanes active */
	{D0W_CNTRL, 0x0000, 0},
	{D1W_CNTRL, 0x0000, 0},

	{D2W_CNTRL, 0x0000, 0},
	{D3W_CNTRL, 0x0000, 0},
	{HSTXVREGEN, 0x001f, 0},

	/* Continuous clock mode */
	{TXOPTIONCNTRL, MASK_CONTCLKMODE, 0},
	{STARTCNTRL, MASK_START, 0},
	{CSI_START, MASK_STRT, 0},

	/* Use two lanes */
	{CSI_CONFW, MASK_MODE_SET | MASK_ADDRESS_CSI_CONTROL | MASK_CSI_MODE | MASK_TXHSMD | MASK_HSCKMD | MASK_NOL_2, 0},

	/* Output Control */
	{CONFCTL, MASK_VBUFEN | MASK_INFRMEN | MASK_YCBCRFMT_422_8_BIT | MASK_AUTOINDEX, 0},
};

static struct reg_value tc358743_setting_hdmi[] = {
	/* HDMI interrupt mask */
	{SYS_INT, 0x0000, 100},
	{SYS_INTM, 0x0000, 0},
	{PACKET_INTM, 0x0000, 0},
	{CBIT_INTM, 0x0000, 0},
	{AUDIO_INTM, 0x0000, 0},

	{PHY_CTL0, MASK_PHY_CTL, 0},
	/* HDMI PHY */
	{PHY_CTL1, 0x0080, 0},
	{PHY_BIAS, 0x0040, 0},
	{PHY_CSQ, 0x000a, 0},

	{HDMI_DET, MASK_HDMI_DET_V_SYNC | MASK_HDMI_DET_MOD0 | MASK_HDMI_DET_MOD1, 0},
	{HV_RST, MASK_H_PI_RST | MASK_V_PI_RST, 0},
	{PHY_CTL2, MASK_PHY_AUTO_RST2 | MASK_PHY_AUTO_RST3 | MASK_PHY_AUTO_RST4, 0},

	/* HDMI system */
	{DDC_CTL, 0x0030 | MASK_DDC5V_MODE_100MS, 0},
	{ANA_CTL, MASK_APPL_PCSX_NORMAL | MASK_ANALOG_ON, 0},
	{AVM_CTL, 0x002d, 0},
	{VI_MODE, MASK_RGB_DVI, 0},
	{VI_MUTE, MASK_AUTO_MUTE, 0},
	{CMD_AUD, MASK_CMD_MUTE, 0},

	/* EDID */
	{EDID_MODE, 0x0001, 2},
	{EDID_LEN2, 0x0001, 0},

	{BCAPS, MASK_REPEATER | MASK_READY, 0}, /* Turn off HDMI */
	{BSTATUS1, MASK_MAX_EXCED, 0},

	/* HDCP Settings */
	{HDCP_REG3, 0x0001, 0},
	{HDCP_MODE, 0x0024, 0},
	{HDCP_REG1, 0x0011, 0},
	{HDCP_REG2, 0x000f, 0},

	/* RGB to YUV Conversion */
	{VOUT_SET0, 0x0002,0},
	{VOUT_SET2, MASK_VOUTCOLORMODE_AUTO | MASK_SEL422  | MASK_VOUT_422FIL_100, 0},
	{VOUT_SET3, MASK_VOUT_EXTCNT, 0},
	{VI_REP, MASK_VOUT_COLOR_601_YCBCR_LIMITED, 0},

	/* InfoFrame extraction */
	{PK_INT_MODE, 0x00ff, 0},
	{NO_PKT_LIMIT, 0x002c, 0},
	{NO_PKT_CLR, 0x0053, 0},
	{ERR_PK_LIMIT, 0x0001, 0},
	{NO_PKT_LIMIT2, 0x0030, 0},
	{NO_GDB_LIMIT, 0x0010, 0},
	{INIT_END, MASK_INIT_END, 0},
};

enum {
	TC358743_MODE_640x480,
	TC358743_MODE_800x600,
	TC358743_MODE_1024x768,
	TC358743_MODE_1280x720,
	TC358743_MODE_1280x1024,
	TC358743_MODE_1680x1050,
	TC358743_MODE_1920x1080,
	TC358743_SIZE_LAST,
};

#define to_tc358743(sd)		container_of(sd, struct tc358743_priv, subdev)

struct tc358743_priv {
	struct v4l2_subdev		subdev;
	struct v4l2_mbus_framefmt	mf;
	int				ident;
	u16				chip_id;
	u8				revision;
	int mode;
	struct i2c_client *client;
};

static enum v4l2_mbus_pixelcode tc358743_codes[] = {
	V4L2_MBUS_FMT_UYVY8_2X8,
};

static const struct v4l2_frmsize_discrete tc358743_frmsizes[TC358743_SIZE_LAST] = {
	{640, 480},
	{800, 600},
	{1024, 768},
	{1280, 720},
	{1280, 1024},
	{1680, 1050},
	{1920, 1080},
};

/* EDID taken from BENQ G2220HD display */
static u8 hdmi_edid[] = {
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x09, 0xd1, 0x21, 0x78, 0x45, 0x54, 0x00, 0x00,
	0x26, 0x14, 0x01, 0x03, 0x80, 0x30, 0x1b, 0x78, 0x2e, 0x35, 0x81, 0xa6, 0x56, 0x48, 0x9a, 0x24,
	0x12, 0x50, 0x54, 0xa5, 0x6b, 0x80, 0x71, 0x00, 0x81, 0xc0, 0x81, 0x40, 0x81, 0x80, 0xa9, 0xc0,
	0xb3, 0x00, 0xd1, 0xc0, 0x01, 0x01, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
	0x45, 0x00, 0xdd, 0x0c, 0x11, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xff, 0x00, 0x48, 0x39, 0x41,
	0x30, 0x30, 0x34, 0x35, 0x35, 0x53, 0x4c, 0x30, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x32,
	0x4c, 0x18, 0x53, 0x11, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc,
	0x00, 0x42, 0x65, 0x6e, 0x51, 0x20, 0x47, 0x32, 0x32, 0x32, 0x30, 0x48, 0x44, 0x0a, 0x00, 0xd7,
};

static int tc358743_find_mode(u32 width, u32 height)
{
	int i;

	for (i = 0; i < TC358743_SIZE_LAST; i++) {
		if ((tc358743_frmsizes[i].width >= width) &&
		                (tc358743_frmsizes[i].height >= height))
			break;
	}

	/* If not found, select the biggest */
	if (i >= TC358743_SIZE_LAST)
		i = TC358743_SIZE_LAST - 1;

	return i;
}

static int get_register_size(u16 reg) {
	int i = 0;
	int size = 0;
	while(0 != tc358743_read_reg_size[i].startaddr ||
	      0 != tc358743_read_reg_size[i].endaddr ||
	      0 != tc358743_read_reg_size[i].size)
	{
		if(tc358743_read_reg_size[i].startaddr <= reg && tc358743_read_reg_size[i].endaddr >= reg)
		{
			size = tc358743_read_reg_size[i].size;
			break;
		}
		i++;
	}
	if (size == 0) {
		printk(KERN_ERR "Unkown register 0x%04x size!!", reg);
	}
	return size;
}

static uint32_t i2c_rd(struct i2c_client *client, u16 reg)
{
	int err;
	uint32_t result = 0;
	uint8_t *values = (uint8_t*)&result;
	u8 buf[2] = { reg >> 8, reg & 0xff };
	int n = get_register_size(reg);
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = n,
			.buf = values,
		},
	};
	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != ARRAY_SIZE(msgs)) {
		pr_err("%s: reading register 0x%x from 0x%x failed\n", __func__, reg, client->addr);
	}
	return result;
}

static int i2c_wr(struct i2c_client *client, u16 reg, u32 val)
{
	int i = 0;
	u32 data = val;
	u8 au8Buf[6] = {0};
	int size = get_register_size(reg);
	int len = get_register_size(reg);

	if(!size) {
		pr_err("%s:write reg error:reg=%x is not found\n",__func__, reg);
		return -1;
	}

	if(size == 3) {
		size = 2;
	} else if(size != len) {
		pr_err("%s:write reg len error:reg=%x %d instead of %d\n",
		       __func__, reg, len, size);
		return 0;
	}

	while(len > 0) {
		i = 0;
		au8Buf[i++] = (reg >> 8) & 0xff;
		au8Buf[i++] = reg & 0xff;
		while(size-- > 0) {
			au8Buf[i++] = (u8)data;
			data >>= 8;
		}

		if (i2c_master_send(client, au8Buf, i) < 0) {
			pr_err("%s:write reg error:reg=%x,val=%x\n",
			       __func__, reg, val);
			return -1;
		}
		len -= (u8)size;
		reg += (u16)size;
	}

	return 0;
}

static int tc358743_set_pll(struct i2c_client *client, uint32_t frequency)
{
	int ret = 0;

	timings_regs timings = tc358743_get_best_timings(frequency);
	if (timings.frequency == 0) return 1;

	printk(KERN_DEBUG "Setting pll to frequency %d (%d) MHz\n",
			   frequency, timings.frequency);

	ret += i2c_wr(client, FIFOCTL, timings.fifo_delay);

	/* Disable all interrupts except the hdmi-rx */
	ret += i2c_wr(client, INTSTATUS, 0x0);
	ret += i2c_wr(client, INTMASK, 0x5ff);

	ret += i2c_wr(client, PLLCTL0, SET_PLL_FREQ(frequency));

	ret += i2c_wr(client, PLLCTL1,
		      SET_PLL_FRS(GET_FRS(frequency), FRS_BW_50) | MASK_RESETB | MASK_PLL_EN);
	mdelay(1);
	ret += i2c_wr(client, PLLCTL1,
		      SET_PLL_FRS(GET_FRS(frequency), FRS_BW_50) | MASK_RESETB | MASK_PLL_EN | MASK_CKEN);

	ret += i2c_wr(client, LINEINITCNT, timings.line_init_cnt);
	ret += i2c_wr(client, LPTXTIMECNT, timings.tlpx_time_cnt);

	ret += i2c_wr(client, TCLK_HEADERCNT,
		      CALC_HDR_CNT(timings.tclk_prepare_cnt, timings.tclk_zero_cnt));

	ret += i2c_wr(client, TCLK_TRAILCNT, timings.tclk_trail_cnt);

	ret += i2c_wr(client, THS_HEADERCNT,
		      CALC_HDR_CNT(timings.ths_prepare_cnt, timings.ths_zero_cnt));

	ret += i2c_wr(client, TWAKEUP, timings.twakeup_cnt);
	ret += i2c_wr(client, TCLK_POSTCNT, timings.tclk_post_cnt);
	ret += i2c_wr(client, THS_TRAILCNT, timings.ths_trail_cnt);

	return ret;
}

static int tc358743_write_table(struct i2c_client *client, struct reg_value table[], int table_length)
{
	u32 lines_to_repeat = 0;
	u32 repeats = 0;
	u32 delay_ms = 0;
	u16 addr = 0;
	u32 val = 0;
	int ret = 0;
	int i;

	for (i = 0; i < table_length; ++i) {
		addr = table[i].addr;
		val = table[i].val;
		delay_ms = (table[i].flags & 0xffff);

		ret = i2c_wr(client, addr, val);
		if (ret < 0)
			break;

		if (delay_ms)
			msleep(delay_ms);

		if(((table[i].flags >> 16) & (0xff)) != 0) {
			if(!repeats) {
				repeats = ((table[i].flags >> 16) & 0xff);
				lines_to_repeat = ((table[i].flags >> 24) & 0xff);
			}
			if(--repeats > 0) {
				i -= lines_to_repeat;
			}
		}
	}

	return ret;
}

static int tc358743_write_edid(struct i2c_client *client, u8 *edid, int len)
{
	int i = 0, off = 0;
	u8 au8Buf[8+2] = {0};
	int size = 0;
	u16 reg;

	reg = 0x8C00;
	off = 0;
	size = ARRAY_SIZE(au8Buf)-2;
	printk(KERN_DEBUG "Write EDID: %d (%d)\n", len, size);
	while(len > 0)
	{
		i = 0;
		au8Buf[i++] = (reg >> 8) & 0xff;
		au8Buf[i++] = reg & 0xff;
		while(i < ARRAY_SIZE(au8Buf))
		{
			au8Buf[i++] = edid[off++];
		}

		if (i2c_master_send(client, au8Buf, i) < 0) {
			pr_err("%s:write reg error:reg=%x,val=%x\n",
			       __func__, reg, off);
			return -1;
		}
		len -= (u8)size;
		reg += (u16)size;
	}
	printk(KERN_DEBUG "Activate EDID\n");
	i2c_wr(client, EDID_MODE, MASK_EDID_MODE_DDC2B);
	i2c_wr(client, EDID_LEN1, 0x00);
	i2c_wr(client, EDID_LEN2, 0x01);
	return 0;
}

static int tc358743_toggle_hpd(struct i2c_client *client, int active)
{
	int ret = 0;

	if(active)
	{
		ret += i2c_wr(client, HPD_CTL, 0x00);
		mdelay(500);
		ret += i2c_wr(client, HPD_CTL, MASK_HPD_CTL0);
	} else {
		ret += i2c_wr(client, HPD_CTL, MASK_HPD_CTL0);
		mdelay(500);
		ret += i2c_wr(client, HPD_CTL, 0x00);
	}
	return ret;
}

static void tc358743_set_default_fmt(struct tc358743_priv *priv)
{
	struct v4l2_mbus_framefmt *mf = &priv->mf;

	mf->code = V4L2_MBUS_FMT_UYVY8_2X8;
	mf->field = V4L2_FIELD_NONE;
	mf->colorspace = V4L2_COLORSPACE_SRGB;
}

/* Start/Stop streaming from the device */
static int tc358743_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct tc358743_priv *priv = to_tc358743(sd);

	if (!enable) {
		printk(KERN_DEBUG "Disabling stream");
		i2c_wr(priv->client, CONFCTL, 0);
		return 0;
	}

	return 0;
}

static int tc358743_try_fmt(struct v4l2_subdev *sd,
                            struct v4l2_mbus_framefmt *mf)
{
	int mode;

	mode = tc358743_find_mode(mf->width, mf->height);
	mf->width = tc358743_frmsizes[mode].width;
	mf->height = tc358743_frmsizes[mode].height;
	mf->field = V4L2_FIELD_NONE;
	mf->code = V4L2_MBUS_FMT_UYVY8_2X8;
	mf->colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}


struct aviInfoFrame {
	u8 f17;
	u8 y10;
	u8 a0;
	u8 b10;
	u8 s10;
	u8 c10;
	u8 m10;
	u8 r3210;
	u8 itc;
	u8 ec210;
	u8 q10;
	u8 sc10;
	u8 f47;
	u8 vic;
	u8 yq10;
	u8 cn10;
	u8 pr3210;
	u16 etb;
	u16 sbb;
	u16 elb;
	u16 srb;
};

static inline bool is_hdmi(struct v4l2_subdev *sd)
{
	struct tc358743_priv *priv = to_tc358743(sd);
	return i2c_rd(priv->client, SYS_STATUS) & MASK_S_HDMI;
}

/* set the format we will capture in */
static int tc358743_s_fmt(struct v4l2_subdev *sd,
                          struct v4l2_mbus_framefmt *mf)
{
	struct tc358743_priv *priv = to_tc358743(sd);
	int ret;
	uint32_t v2, v, width, height, v_size, h_size, fv_cnt, fps;

	ret = tc358743_try_fmt(sd, mf);
	if (ret < 0)
		return ret;

	priv->mode = tc358743_find_mode(mf->width, mf->height);

	memcpy(&priv->mf, mf, sizeof(struct v4l2_mbus_framefmt));

	tc358743_write_table(priv->client, tc358743_reset, ARRAY_SIZE(tc358743_reset));
	tc358743_write_table(priv->client, tc358743_setting_hdmi, ARRAY_SIZE(tc358743_setting_hdmi));
	msleep(100);

	v = i2c_rd(priv->client, DE_WIDTH_H_LO);
	v2 = i2c_rd(priv->client, DE_WIDTH_H_HI) & 0x1f;
	width = (v2 << 8) + v;
	v = i2c_rd(priv->client, DE_WIDTH_V_LO);
	v2 = i2c_rd(priv->client, DE_WIDTH_V_HI) & 0x1f;
	height = (v2 << 8) + v;
	v = i2c_rd(priv->client, H_SIZE_LO);
	v2 = i2c_rd(priv->client, H_SIZE_HI) & 0x1f;
	h_size = (v2 << 8) + v;
	v = i2c_rd(priv->client, V_SIZE_LO);
	v2 = i2c_rd(priv->client, V_SIZE_HI) & 0x3f;
	v_size = ((v2 << 8) + v) / 2;
	v = i2c_rd(priv->client, FV_CNT_LO);
	v2 = i2c_rd(priv->client, FV_CNT_HI) & 0x3;
	fv_cnt = (v2 << 8) + v;
	fps = fv_cnt > 0 ? DIV_ROUND_CLOSEST(10000, fv_cnt) : 0;

	printk(KERN_DEBUG "Image is %dx%d@%d, ~%d Gbps bandwidth (~%dMHz/lane)",
			   width, height, fps,
			   DIV_ROUND_UP(fps*width*height*16, 1000*1000),
			   DIV_ROUND_UP(fps*width*height*8, 1000*1000));

	if (width * fps == 0) {
		printk(KERN_ERR "width or fps 0");
		return 0;
	}

	if ((width != mf->width)) {
		printk(KERN_ERR "Wrong width (%d vs %d)", width, mf->width);
		return 0;
	}

	/* XXX the chip sometimes miscalculates the height of the image,
	 * therefore, we base the output mostly on the width of the image */
	if (width == 1920) {
		/* works for 1920x1080 @ 60 */
		ret = tc358743_set_pll(priv->client, 1075);
		/* 872 is in the middle between 1024 and 720, should be a safe
		 * value for both 1280x720 and 1280x1024 */
	} else if (width == 1280 && height >= 872) {
		/* works on 1280x1024 @ 75 */
		ret = tc358743_set_pll(priv->client, 1075);
	} else if (width == 1280 && height < 872) {
		/* works on 1280x720 @ 60 */
		ret = tc358743_set_pll(priv->client, 825);
	} else if (width == 1024) {
		/* works on 1024x768 @ 75 */
		ret = tc358743_set_pll(priv->client, 600);
	} else if (width >= 800) {
		/* works for 800x600@75 */
		ret = tc358743_set_pll(priv->client, 594);
	} else if (width == 640) {
		/* works on 640x480 @ 75 */
		ret = tc358743_set_pll(priv->client, 500);
	} else {
		return 0;
	}

	tc358743_write_table(priv->client,
			     tc358743_2lanes_start,
			     ARRAY_SIZE(tc358743_2lanes_start));

	return ret;
}

static int tc358743_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
                             enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(tc358743_codes))
		return -EINVAL;

	*code = tc358743_codes[index];

	return 0;
}

/* Get chip identification */
static int tc358743_g_chip_ident(struct v4l2_subdev *sd,
                                 struct v4l2_dbg_chip_ident *id)
{
	struct tc358743_priv *priv = to_tc358743(sd);

	id->ident = priv->ident;
	id->revision = priv->revision;

	return 0;
}

static int tc358743_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *scsd = soc_camera_i2c_to_desc(client);

	return soc_camera_set_power(&client->dev, scsd, on);
}

static struct v4l2_subdev_video_ops tc358743_video_ops = {
	.s_stream		= tc358743_s_stream,
	.s_mbus_fmt		= tc358743_s_fmt,
	.try_mbus_fmt		= tc358743_try_fmt,
	.enum_mbus_fmt		= tc358743_enum_fmt,
};

static struct v4l2_subdev_core_ops tc358743_core_ops = {
	.g_chip_ident		= tc358743_g_chip_ident,
	.s_power		= tc358743_s_power,
};

static struct v4l2_subdev_ops tc358743_subdev_ops = {
	.core			= &tc358743_core_ops,
	.video			= &tc358743_video_ops,
};

/*
 * i2c_driver function
 */
static int tc358743_probe(struct i2c_client *client,
                          const struct i2c_device_id *did)
{
	struct tc358743_priv *priv;
	struct soc_camera_subdev_desc *scsd;
	int ret = 0;

	/* Checking soc-camera interface */
	scsd = client->dev.platform_data;
	if (!scsd) {
		dev_err(&client->dev, "Missing soc_camera_link for driver\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&client->dev, sizeof(struct tc358743_priv),
	                    GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "Failed to allocate private data!\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &tc358743_subdev_ops);

	priv->client = client;
	priv->ident = V4L2_IDENT_OV5640;

	/*
	 * check and show product ID and manufacturer ID
	 */
	soc_camera_power_on(&client->dev, scsd);

	tc358743_set_default_fmt(priv);
	tc358743_write_table(client, tc358743_reset, ARRAY_SIZE(tc358743_reset));
	tc358743_write_table(client, tc358743_setting_hdmi, ARRAY_SIZE(tc358743_setting_hdmi));

	if((ret = tc358743_write_edid(client, hdmi_edid, ARRAY_SIZE(hdmi_edid))))
		printk(KERN_ERR "%s: Fail to write EDID to tc35874!\n", __FUNCTION__);

	tc358743_toggle_hpd(client, 1);

	soc_camera_power_off(&client->dev, scsd);

	return ret;
}

static int tc358743_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id tc358743_id[] = {
	{ "tc358743", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tc358743_id);

static struct i2c_driver tc358743_i2c_driver = {
	.driver         = {
		.name   = "tc358743",
		.owner  = THIS_MODULE,
	},
	.probe    = tc358743_probe,
	.remove   = tc358743_remove,
	.id_table = tc358743_id,
};

static int __init tc358743_module_init(void)
{
	return i2c_add_driver(&tc358743_i2c_driver);
}

static void __exit tc358743_module_exit(void)
{
	i2c_del_driver(&tc358743_i2c_driver);
}

module_init(tc358743_module_init);
module_exit(tc358743_module_exit);

MODULE_DESCRIPTION("SoC Camera driver for Toshiba TC358743 HDMI to CSI-2 bridge");
MODULE_AUTHOR("Wojciech Bieganski <wbieganski@antmicro.com>");
MODULE_LICENSE("GPL v2");
