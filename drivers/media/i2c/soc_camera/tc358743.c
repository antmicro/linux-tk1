/*
 * based on OV5640 driver and TC358743 driver for i.MX6
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

struct reg_value {
	u16 addr;
	u32 val;
	u8  len;
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

static struct reg_value tc358743_setting_YUV422_2lane_color_bar_640_480_174MHz[] = {
  {0x7080, 0x00000000, 2, 0},
  {0x0002, 0x00000f00, 2, 100},
  {0x0002, 0x00000000, 2, 1000},
  {0x0006, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 2, 0},
  {0x0010, 0x0000001e, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x00008073, 2, 0},
  {0x0022, 0x00000213, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 4, 0},
//  {0x014c, 0x00000000, 0x00000000, 4, 0},
//  {0x0150, 0x00000000, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00001200, 4, 0},
  {0x0214, 0x00000002, 4, 0},
  {0x0218, 0x00000b02, 4, 0},
  {0x021c, 0x00000001, 4, 0},
  {0x0220, 0x00000103, 4, 0},
  {0x0224, 0x00004000, 4, 0},
  {0x0228, 0x00000008, 4, 0},
  {0x022c, 0x00000002, 4, 0},
  {0x0234, 0x0000001f, 4, 0},
  {0x0238, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 4, 0},
  {0x0518, 0x00000001, 4, 0},
  {0x0500, 0xA3008082, 4, 0},
// 640x480 colorbar
  {0x000a, 0x00000500, 2, 0},
  {0x7080, 0x00000082, 2, 0},
// 80 pixel black - repeate 80 times
  {0x7000, 0x0000007f, 2, (1<<24)|(80<<16)},
// 80 pixel blue - repeate 40 times
  {0x7000, 0x000000ff, 2, 0},
  {0x7000, 0x00000000, 2, (2<<24)|(40<<16)},
// 80 pixel red - repeate 40 times
  {0x7000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 2, (2<<24)|(40<<16)},
// 80 pixel pink - repeate 40 times
  {0x7000, 0x00007fff, 2, 0},
  {0x7000, 0x00007fff, 2, (2<<24)|(40<<16)},
// 80 pixel green - repeate 40 times
  {0x7000, 0x00007f00, 2, 0},
  {0x7000, 0x00007f00, 2, (2<<24)|(40<<16)},
// 80 pixel light blue - repeate 40 times
  {0x7000, 0x0000c0ff, 2, 0},
  {0x7000, 0x0000c000, 2, (2<<24)|(40<<16)},
// 80 pixel yellow - repeate 40 times
  {0x7000, 0x0000ff00, 2, 0},
  {0x7000, 0x0000ffff, 2, (2<<24)|(40<<16)},
// 80 pixel white - repeate 40 times
  {0x7000, 0x0000ff7f, 2, 0},
  {0x7000, 0x0000ff7f, 2, (2<<24)|(40<<16)},
// 480 lines
  {0x7090, 0x000001df, 2, 0},
  {0x7092, 0x00000898, 2, 0},
  {0x7094, 0x00000285, 2, 0},
  {0x7080, 0x00000083, 2, 0},
};

static struct reg_value tc358743_setting_YUV422_2lane_60fps_640_480_125Mhz[] = {
  {0x7080, 0x00000000, 2, 0},
  {0x0004, 0x00000004, 2, 0},
  {0x0002, 0x00000f00, 2, 100},
  {0x0002, 0x00000000, 2, 1000},
  {0x0006, 0x00000040, 2, 0},
//  {0x000a, 0x000005a0, 0x00000000, 2, 0},
//  {0x0010, 0x0000001e, 0x00000000, 2, 0},
  {0x0014, 0x00000000, 2, 0},
  {0x0016, 0x000005ff, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000405c, 2, 0},
  {0x0022, 0x00000613, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000d00, 4, 0},
  {0x0214, 0x00000001, 4, 0},
  {0x0218, 0x00000701, 4, 0},
  {0x021c, 0x00000000, 4, 0},
  {0x0220, 0x00000001, 4, 0},
  {0x0224, 0x00004000, 4, 0},
  {0x0228, 0x00000005, 4, 0},
  {0x022c, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 4, 0},
  {0x0238, 0x00000000, 4, 0}, //gated clock
  {0x0204, 0x00000001, 4, 0},
  {0x0518, 0x00000001, 4, 0},
  {0x0500, 0xA30080A2, 4, 0},
// HDMI Interrupt Mask
  {0x8502, 0x00000001, 1, 0},
  {0x8512, 0x000000fe, 1, 0},
  {0x8514, 0x00000000, 1, 0},
  {0x8515, 0x00000000, 1, 0},
  {0x8516, 0x00000000, 1, 0},
// HDMI Audio RefClk (26 MHz)
  {0x8531, 0x00000001, 1, 0},
  {0x8540, 0x00000a8c, 1, 0},
  {0x8630, 0x00041eb0, 1, 0},
  {0x8670, 0x00000001, 1, 0},
// HDMI PHY
  {0x8532, 0x00000080, 1, 0},
  {0x8536, 0x00000040, 1, 0},
  {0x853f, 0x0000000a, 1, 0},
// HDMI System
  {0x8543, 0x00000032, 1, 0},
  {0x8544, 0x00000000, 1, 100},
//  {0x8544, 0x00000001, 0x00000000, 1, 100},
  {0x8545, 0x00000031, 1, 0},
  {0x8546, 0x0000002d, 1, 0},
// EDID
  {0x85c7, 0x00000001, 1, 0},
  {0x85cb, 0x00000001, 1, 0},
// HDCP Setting
  {0x85d1, 0x00000001, 1, 0},
  {0x8560, 0x00000024, 1, 0},
  {0x8563, 0x00000011, 1, 0},
  {0x8564, 0x0000000f, 1, 0},
// RGB --> YUV Conversion
  {0x8573, 0x00000081, 1, 0},
  {0x8571, 0x00000002, 1, 0},
// HDMI Audio In Setting
  {0x8600, 0x00000000, 1, 0},
  {0x8602, 0x000000f3, 1, 0},
  {0x8603, 0x00000002, 1, 0},
  {0x8604, 0x0000000c, 1, 0},
  {0x8606, 0x00000005, 1, 0},
  {0x8607, 0x00000000, 1, 0},
  {0x8620, 0x00000022, 1, 0},
  {0x8640, 0x00000001, 1, 0},
  {0x8641, 0x00000065, 1, 0},
  {0x8642, 0x00000007, 1, 0},
  {0x8652, 0x00000002, 1, 0},
  {0x8665, 0x00000010, 1, 0},
// InfoFrame Extraction
  {0x8709, 0x000000ff, 1, 0},
  {0x870b, 0x0000002c, 1, 0},
  {0x870c, 0x00000053, 1, 0},
  {0x870d, 0x00000001, 1, 0},
  {0x870e, 0x00000030, 1, 0},
  {0x9007, 0x00000010, 1, 0},
  {0x854a, 0x00000001, 1, 0},
// Output Control
  {0x0004, 0x00000cf7, 2, 0},
  };

static struct reg_value tc358743_setting_YUV422_2lane_color_bar_1280_720_125MHz[] = {
  {0x7080, 0x00000000, 2, 0},
  {0x0002, 0x00000f00, 2, 100},
  {0x0002, 0x00000000, 2, 1000},
  {0x0006, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 2, 0},
  {0x0010, 0x0000001e, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000405c, 2, 0},
  {0x0022, 0x00000613, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000e00, 4, 0},
  {0x0214, 0x00000001, 4, 0},
  {0x0218, 0x00000801, 4, 0},
  {0x021c, 0x00000000, 4, 0},
  {0x0220, 0x00000001, 4, 0},
  {0x0224, 0x00004000, 4, 0},
  {0x0228, 0x00000006, 4, 0},
  {0x022c, 0x00000000, 4, 0},
  {0x0234, 0x00000007, 4, 0},
  {0x0238, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 4, 0},
  {0x0518, 0x00000001, 4, 0},
  {0x0500, 0xa30080a2, 4, 0},
// 1280x720 colorbar
  {0x000a, 0x00000a00, 2, 0},
  {0x7080, 0x00000082, 2, 0},
// 128 pixel black - repeat 128 times
  {0x7000, 0x0000007f, 2, (1<<24)|(128<<16)},
// 128 pixel blue - repeat 64 times
  {0x7000, 0x000000ff, 2, 0},
  {0x7000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel red - repeat 64 times
  {0x7000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 2, (2<<24)|(64<<16)},
// 128 pixel pink - repeat 64 times
  {0x7000, 0x00007fff, 2, 0},
  {0x7000, 0x00007fff, 2, (2<<24)|(64<<16)},
// 128 pixel green - repeat 64 times
  {0x7000, 0x00007f00, 2, 0},
  {0x7000, 0x00007f00, 2, (2<<24)|(64<<16)},
// 128 pixel light blue - repeat 64 times
  {0x7000, 0x0000c0ff, 2, 0},
  {0x7000, 0x0000c000, 2, (2<<24)|(64<<16)},
// 128 pixel yellow - repeat 64 times
  {0x7000, 0x0000ff00, 2, 0},
  {0x7000, 0x0000ffff, 2, (2<<24)|(64<<16)},
// 128 pixel white - repeat 64 times
  {0x7000, 0x0000ff7f, 2, 0},
  {0x7000, 0x0000ff7f, 2, (2<<24)|(64<<16)},
// 720 lines
  {0x7090, 0x000002cf, 2, 0},
  {0x7092, 0x00000580, 2, 0},
  {0x7094, 0x00000010, 2, 0},
  {0x7080, 0x00000083, 2, 0},
};

#if 0
static struct reg_value tc358743_setting_YUV422_2lane_30fps_720P_1280_720_125MHz[] = {
  {0x7080, 0x00000000, 2, 0},
  {0x0004, 0x00000004, 2, 0},
  {0x0002, 0x00000f00, 2, 100},
  {0x0002, 0x00000000, 2, 1000},
  {0x0006, 0x00000040, 2, 0},
  {0x0014, 0x00000000, 2, 0},
  {0x0016, 0x000005ff, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000402d, 2, 0},
  {0x0022, 0x00000213, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000e00, 4, 0},
  {0x0214, 0x00000001, 4, 0},
  {0x0218, 0x00000801, 4, 0},
  {0x021c, 0x00000001, 4, 0},
  {0x0220, 0x00000001, 4, 0},
  {0x0224, 0x00004800, 4, 0},
  {0x0228, 0x00000005, 4, 0},
  {0x022c, 0x00000000, 4, 0},
  {0x0234, 0x0000001f, 4, 0},
  {0x0238, 0x00000000, 4, 0}, //non-continuous clock
  {0x0204, 0x00000001, 4, 0},
  {0x0518, 0x00000001, 4, 0},
  {0x0500, 0xa300be82, 4, 0},
// HDMI Interrupt Mask
  {0x8502, 0x00000001, 1, 0},
  {0x8512, 0x000000fe, 1, 0},
  {0x8514, 0x00000000, 1, 0},
  {0x8515, 0x00000000, 1, 0},
  {0x8516, 0x00000000, 1, 0},
// HDMI Audio RefClk (26 MHz)
  {0x8531, 0x00000001, 1, 0},
  {0x8540, 0x0000008c, 1, 0},
  {0x8541, 0x0000000a, 1, 0},
  {0x8630, 0x000000b0, 1, 0},
  {0x8631, 0x0000001e, 1, 0},
  {0x8632, 0x00000004, 1, 0},
  {0x8670, 0x00000001, 1, 0},
// HDMI PHY
  {0x8532, 0x00000080, 1, 0},
  {0x8536, 0x00000040, 1, 0},
  {0x853f, 0x0000000a, 1, 0},
// EDID
  {0x85c7, 0x00000001, 1, 0},
  {0x85cb, 0x00000001, 1, 0},
// HDMI System
  {0x8543, 0x00000032, 1, 0},
//  {0x8544, 0x00000000, 0x00000000, 1, 1000},
//  {0x8544, 0x00000001, 0x00000000, 1, 100},
  {0x8545, 0x00000031, 1, 0},
  {0x8546, 0x0000002d, 1, 0},
// HDCP Setting
  {0x85d1, 0x00000001, 1, 0},
  {0x8560, 0x00000024, 1, 0},
  {0x8563, 0x00000011, 1, 0},
  {0x8564, 0x0000000f, 1, 0},
// Video settings
  {0x8573, 0x00000081, 1, 0},
  {0x8571, 0x00000002, 1, 0},
// HDMI Audio In Setting
  {0x8600, 0x00000000, 1, 0},
  {0x8602, 0x000000f3, 1, 0},
  {0x8603, 0x00000002, 1, 0},
  {0x8604, 0x0000000c, 1, 0},
  {0x8606, 0x00000005, 1, 0},
  {0x8607, 0x00000000, 1, 0},
  {0x8620, 0x00000022, 1, 0},
  {0x8640, 0x00000001, 1, 0},
  {0x8641, 0x00000065, 1, 0},
  {0x8642, 0x00000007, 1, 0},
//  {0x8651, 0x00000003, 0x00000000, 1, 0},	// Inverted LRCK polarity - (Sony) format
  {0x8652, 0x00000002, 1, 0},	// Left-justified I2S (Phillips) format
//  {0x8652, 0x00000000, 0x00000000, 1, 0},	// Right-justified (Sony) format
  {0x8665, 0x00000010, 1, 0},
// InfoFrame Extraction
  {0x8709, 0x000000ff, 1, 0},
  {0x870b, 0x0000002c, 1, 0},
  {0x870c, 0x00000053, 1, 0},
  {0x870d, 0x00000001, 1, 0},
  {0x870e, 0x00000030, 1, 0},
  {0x9007, 0x00000010, 1, 0},
  {0x854a, 0x00000001, 1, 0},
// Output Control
  {0x0004, 0x00000cf7, 2, 0},
  };
#endif

static struct reg_value tc358743_setting_YUV422_4lane_1080P_60fps_1920_1080_300MHz[] = {
  {0x7080, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 2, 0},
  {0x0002, 0x00000f00, 2, 100},//0},
  {0x0002, 0x00000000, 2, 1000},//0},
  {0x0006, 0x00000000, 2, 0},
  {0x0014, 0x00000000, 2, 0},
  {0x0016, 0x000005ff, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x000080c7, 2, 0},
  {0x0022, 0x00000213, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00001e00, 4, 0},
  {0x0214, 0x00000003, 4, 0},
  {0x0218, 0x00001402, 4, 0},
  {0x021c, 0x00000000, 4, 0},
  {0x0220, 0x00000003, 4, 0},
  {0x0224, 0x00004a00, 4, 0},
  {0x0228, 0x00000008, 4, 0},
  {0x022c, 0x00000002, 4, 0},
  {0x0234, 0x0000001f, 4, 0},
  {0x0238, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 4, 0},
  {0x0518, 0x00000001, 4, 0},
  {0x0500, 0xa30080a6, 4, 0},
// HDMI Interrupt Mask
  {0x8502, 0x00000001, 1, 0},
  {0x8512, 0x000000fe, 1, 0},
  {0x8514, 0x00000000, 1, 0},
  {0x8515, 0x00000000, 1, 0},
  {0x8516, 0x00000000, 1, 0},
// HDMI Audio RefClk (27 MHz)
  {0x8531, 0x00000001, 1, 0},
  {0x8540, 0x00000a8c, 1, 0},
  {0x8630, 0x00041eb0, 1, 0},
  {0x8670, 0x00000001, 1, 0},
// HDMI PHY
  {0x8532, 0x00000080, 1, 0},
  {0x8536, 0x00000040, 1, 0},
  {0x853f, 0x0000000a, 1, 0},
// HDMI System
  {0x8543, 0x00000032, 1, 0},
  {0x8544, 0x00000010, 1, 100},
  {0x8545, 0x00000031, 1, 0},
  {0x8546, 0x0000002d, 1, 0},
// EDID
  {0x85c7, 0x00000001, 1, 0},
  {0x85cb, 0x00000001, 1, 0},
// HDCP Setting
  {0x85d1, 0x00000001, 1, 0},
  {0x8560, 0x00000024, 1, 0},
  {0x8563, 0x00000011, 1, 0},
  {0x8564, 0x0000000f, 1, 0},
// RGB --> YUV Conversion
  {0x8571, 0x00000002, 1, 0},
  {0x8573, 0x00000081, 1, 0},
  {0x8576, 0x00000060, 1, 0},
// HDMI Audio In Setting
  {0x8600, 0x00000000, 1, 0},
  {0x8602, 0x000000f3, 1, 0},
  {0x8603, 0x00000002, 1, 0},
  {0x8604, 0x0000000c, 1, 0},
  {0x8606, 0x00000005, 1, 0},
  {0x8607, 0x00000000, 1, 0},
  {0x8620, 0x00000022, 1, 0},
  {0x8640, 0x00000001, 1, 0},
  {0x8641, 0x00000065, 1, 0},
  {0x8642, 0x00000007, 1, 0},
  {0x8652, 0x00000002, 1, 0},
  {0x8665, 0x00000010, 1, 0},
// InfoFrame Extraction
  {0x8709, 0x000000ff, 1, 0},
  {0x870b, 0x0000002c, 1, 0},
  {0x870c, 0x00000053, 1, 0},
  {0x870d, 0x00000001, 1, 0},
  {0x870e, 0x00000030, 1, 0},
  {0x9007, 0x00000010, 1, 0},
  {0x854a, 0x00000001, 1, 0},
// Output Control
  {0x0004, 0x00000cf7, 2, 0},
};

static struct reg_value tc358743_setting_YUV422_4lane_color_bar_1280_720_125MHz[] = {
  {0x7080, 0x00000000, 2, 0},
  {0x0002, 0x00000f00, 2, 100},
  {0x0002, 0x00000000, 2, 1000},
  {0x0006, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 2, 0},
  {0x0010, 0x0000001e, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x0000405c, 2, 0},
  {0x0022, 0x00000613, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00000e00, 4, 0},
  {0x0214, 0x00000001, 4, 0},
  {0x0218, 0x00000801, 4, 0},
  {0x021c, 0x00000000, 4, 0},
  {0x0220, 0x00000001, 4, 0},
  {0x0224, 0x00004000, 4, 0},
  {0x0228, 0x00000006, 4, 0},
  {0x022c, 0x00000000, 4, 0},
  {0x0234, 0x00000007, 4, 0}, //{0x0234, 0x00000007, 0x00000000, 4, 0},
  {0x0238, 0x00000000, 4, 0}, //non-continuous clock
  {0x0204, 0x00000001, 4, 0},
  {0x0518, 0x00000001, 4, 0},
  {0x0500, 0xa30080a2, 4, 0}, //{0x0500, 0xa30080a2, 0x00000000, 4, 0},
// 1280x720 colorbar
  {0x000a, 0x00000a00, 2, 0},
  {0x7080, 0x00000082, 2, 0},
// 128 pixel black - repeat 128 times
  {0x7000, 0x0000007f, 2, (1<<24)|(128<<16)},
// 128 pixel blue - repeat 64 times
  {0x7000, 0x000000ff, 2, 0},
  {0x7000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel red - repeat 64 times
  {0x7000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 2, (2<<24)|(64<<16)},
// 128 pixel pink - repeat 64 times
  {0x7000, 0x00007fff, 2, 0},
  {0x7000, 0x00007fff, 2, (2<<24)|(64<<16)},
// 128 pixel green - repeat 64 times
  {0x7000, 0x00007f00, 2, 0},
  {0x7000, 0x00007f00, 2, (2<<24)|(64<<16)},
// 128 pixel light blue - repeat 64 times
  {0x7000, 0x0000c0ff, 2, 0},
  {0x7000, 0x0000c000, 2, (2<<24)|(64<<16)},
// 128 pixel yellow - repeat 64 times
  {0x7000, 0x0000ff00, 2, 0},
  {0x7000, 0x0000ffff, 2, (2<<24)|(64<<16)},
// 128 pixel white - repeat 64 times
  {0x7000, 0x0000ff7f, 2, 0},
  {0x7000, 0x0000ff7f, 2, (2<<24)|(64<<16)},
// 720 lines
  {0x7090, 0x000002cf, 2, 0},
  {0x7092, 0x00000300, 2, 0},
  //{0x7092, 0x00000580, 2, 0},
  {0x7094, 0x00000010, 2, 0},
  {0x7080, 0x00000083, 2, 0},
};

static struct reg_value tc358743_setting_YUV422_4lane_color_bar_1280_720_300MHz[] = {
  {0x7080, 0x00000000, 2, 0},
  {0x0002, 0x00000f00, 2, 100},
  {0x0002, 0x00000000, 2, 1000},
  {0x0006, 0x00000000, 2, 0},
  {0x0004, 0x00000084, 2, 0},
  {0x0010, 0x0000001e, 2, 0},
// Program CSI Tx PLL
  {0x0020, 0x000080c7, 2, 0},
  {0x0022, 0x00000213, 2, 0},
// CSI Tx PHY  (32-bit Registers)
  {0x0140, 0x00000000, 4, 0},
  {0x0144, 0x00000000, 4, 0},
  {0x0148, 0x00000000, 4, 0},
  {0x014c, 0x00000000, 4, 0},
  {0x0150, 0x00000000, 4, 0},
// CSI Tx PPI  (32-bit Registers)
  {0x0210, 0x00001e00, 4, 0},
  {0x0214, 0x00000003, 4, 0},
  {0x0218, 0x00001402, 4, 0},
  {0x021c, 0x00000000, 4, 0},
  {0x0220, 0x00000003, 4, 0},
  {0x0224, 0x00004a00, 4, 0},
  {0x0228, 0x00000008, 4, 0},
  {0x022c, 0x00000002, 4, 0},
  {0x0234, 0x0000001f, 4, 0},
  {0x0238, 0x00000000, 4, 0},
  {0x0204, 0x00000001, 4, 0},
  {0x0518, 0x00000001, 4, 0},
  {0x0500, 0xa30080a6, 4, 0},
// 1280x720 colorbar
  {0x000a, 0x00000a00, 2, 0},
  {0x7080, 0x00000082, 2, 0},
// 128 pixel black - repeat 128 times
  {0x7000, 0x0000007f, 2, (1<<24)|(128<<16)},
// 128 pixel blue - repeat 64 times
  {0x7000, 0x000000ff, 2, 0},
  {0x7000, 0x00000000, 2, (2<<24)|(64<<16)},
// 128 pixel red - repeat 64 times
  {0x7000, 0x00000000, 2, 0},
  {0x7000, 0x000000ff, 2, (2<<24)|(64<<16)},
// 128 pixel pink - repeat 64 times
  {0x7000, 0x00007fff, 2, 0},
  {0x7000, 0x00007fff, 2, (2<<24)|(64<<16)},
// 128 pixel green - repeat 64 times
  {0x7000, 0x00007f00, 2, 0},
  {0x7000, 0x00007f00, 2, (2<<24)|(64<<16)},
// 128 pixel light blue - repeat 64 times
  {0x7000, 0x0000c0ff, 2, 0},
  {0x7000, 0x0000c000, 2, (2<<24)|(64<<16)},
// 128 pixel yellow - repeat 64 times
  {0x7000, 0x0000ff00, 2, 0},
  {0x7000, 0x0000ffff, 2, (2<<24)|(64<<16)},
// 128 pixel white - repeat 64 times
  {0x7000, 0x0000ff7f, 2, 0},
  {0x7000, 0x0000ff7f, 2, (2<<24)|(64<<16)},
// 720 lines
  {0x7090, 0x000002cf, 2, 0},
  {0x7092, 0x000006b8, 2, 0},
  {0x7094, 0x00000010, 2, 0},
  {0x7080, 0x00000083, 2, 0},
};

enum {
	TC358743_MODE_640x480,
	TC358743_MODE_1280x720,
	TC358743_MODE_1920x1080,
	TC358743_SIZE_LAST,
};

static struct reg_value *mode_table[] = {
	[TC358743_MODE_640x480] = tc358743_setting_YUV422_2lane_60fps_640_480_125Mhz,
	//[TC358743_MODE_1280x720] = tc358743_setting_YUV422_2lane_30fps_720P_1280_720_125MHz,
	[TC358743_MODE_1920x1080] = tc358743_setting_YUV422_4lane_1080P_60fps_1920_1080_300MHz,
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
	{1280, 720},
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

	/* If not found, select biggest */
	if (i >= TC358743_SIZE_LAST)
		i = TC358743_SIZE_LAST - 1;

	return i;
}

static int tc358743_write_reg(struct i2c_client *client, u16 reg, u32 val, int len)
{
	int i = 0;
	u32 data = val;
	u8 au8Buf[6] = {0};
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
	if(!size) {
		pr_err("%s:write reg error:reg=%x is not found\n",__func__, reg);
		return -1;
	}

	if(size == 3) {
		size = 2;
	} else
		if(size != len) {
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

static int tc358743_write_table(struct i2c_client *client, struct reg_value table[], int table_length)
{
	u32 lines_to_repeat = 0;
	u32 repeats = 0;
	u32 delay_ms = 0;
	u16 addr = 0;
	u32 val = 0;
	u8  len = 0;
	int ret = 0;
	int i;

	for (i = 0; i < table_length; ++i) {
		addr = table[i].addr;
		val = table[i].val;
		len = table[i].len;
		delay_ms = (table[i].flags & 0xffff);

		ret = tc358743_write_reg(client, addr, val, len);
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
	tc358743_write_reg(client, 0x85c7, 0x01, 1);
	tc358743_write_reg(client, 0x85ca, 0x00, 1);
	tc358743_write_reg(client, 0x85cb, 0x01, 1);
	return 0;
}

static int tc358743_toggle_hpd(struct i2c_client *client, int active)
{
	int ret = 0;
	if(active)
	{
		ret += tc358743_write_reg(client, 0x8544, 0x00, 1);
		mdelay(500);
		ret += tc358743_write_reg(client, 0x8544, 0x10, 1);
	}
	else
	{
		ret += tc358743_write_reg(client, 0x8544, 0x10, 1);
		mdelay(500);
		ret += tc358743_write_reg(client, 0x8544, 0x00, 1);
	}
	return ret;
}

static void tc358743_set_default_fmt(struct tc358743_priv *priv)
{
	struct v4l2_mbus_framefmt *mf = &priv->mf;

	mf->width = tc358743_frmsizes[TC358743_MODE_640x480].width;
	mf->height = tc358743_frmsizes[TC358743_MODE_640x480].height;
	mf->code = V4L2_MBUS_FMT_UYVY8_2X8;
	mf->field = V4L2_FIELD_NONE;
	mf->colorspace = V4L2_COLORSPACE_SRGB;
}

/* Start/Stop streaming from the device */
static int tc358743_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct tc358743_priv *priv = to_tc358743(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (!enable) {
		tc358743_set_default_fmt(priv);
		return 0;
	}

	return ret;
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

/* set the format we will capture in */
static int tc358743_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct tc358743_priv *priv = to_tc358743(sd);
	int ret;

	ret = tc358743_try_fmt(sd, mf);
	if (ret < 0)
		return ret;

	priv->mode = tc358743_find_mode(mf->width, mf->height);

	memcpy(&priv->mf, mf, sizeof(struct v4l2_mbus_framefmt));

	return 0;
}

static int tc358743_s_power(struct v4l2_subdev *sd, int on)
{
	struct tc358743_priv *priv = to_tc358743(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *scsd = soc_camera_i2c_to_desc(client);

	if (on)
		tc358743_s_fmt(sd, &priv->mf);

	return soc_camera_set_power(&client->dev, scsd, on);
}

static int tc358743_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(tc358743_codes))
		return -EINVAL;

	*code = tc358743_codes[index];

	return 0;
}

static int tc358743_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left		= 0;
	a->bounds.top		= 0;
	a->bounds.width		= tc358743_frmsizes[TC358743_MODE_640x480].width;
	a->bounds.height	= tc358743_frmsizes[TC358743_MODE_640x480].height;
	a->defrect		= a->bounds;
	a->type			= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int tc358743_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left		= 0;
	a->c.top		= 0;
	a->c.width		= tc358743_frmsizes[TC358743_MODE_640x480].width;
	a->c.height		= tc358743_frmsizes[TC358743_MODE_640x480].height;
	a->type			= V4L2_BUF_TYPE_VIDEO_CAPTURE;

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

static struct v4l2_subdev_video_ops tc358743_video_ops = {
	.s_stream		= tc358743_s_stream,
	.s_mbus_fmt		= tc358743_s_fmt,
	.try_mbus_fmt		= tc358743_try_fmt,
	.enum_mbus_fmt		= tc358743_enum_fmt,
	.cropcap		= tc358743_cropcap,
	.g_crop			= tc358743_g_crop,
	//.querystd 		= tc358743_querystd,
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
	scsd = soc_camera_i2c_to_desc(client);
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

	/*
	 * check and show product ID and manufacturer ID
	 */
	soc_camera_power_on(&client->dev, scsd);


	tc358743_set_default_fmt(priv);

	//tc358743_write_table(client, tc358743_setting_YUV422_2lane_color_bar_1280_720_125MHz,
	//		ARRAY_SIZE(tc358743_setting_YUV422_2lane_color_bar_1280_720_125MHz));

	tc358743_write_table(client, tc358743_setting_YUV422_2lane_60fps_640_480_125Mhz,
			ARRAY_SIZE(tc358743_setting_YUV422_2lane_60fps_640_480_125Mhz));

	//tc358743_write_table(client, tc358743_setting_YUV422_4lane_color_bar_1280_720_300MHz,
	//		ARRAY_SIZE(tc358743_setting_YUV422_4lane_color_bar_1280_720_300MHz));

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
	.driver = {
		.name = "tc358743",
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
