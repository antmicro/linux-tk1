/*
 * Copyright (c) Antmicro Ltd.  All rights reserved.
 * based on ov5640 driver
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
#include <linux/firmware.h>
#include <linux/gpio.h>

#define AP1302_NAME					"ap1302"
#define AP1302_CHIP_ID				0x0265
#define AP1302_FW_WINDOW_OFFSET		0x8000
#define AP1302_FW_WINDOW_SIZE		0x2000

#define REG_CHIP_VERSION			0x0000
#define REG_CHIP_REV				0x0050

#define REG_ERROR					0x0006
#define REG_CTRL					0x1000
#define REG_ENABLE					0x1008
#define REG_PREVIEW_WIDTH			0x2000
#define REG_PREVIEW_HEIGHT			0x2002
#define REG_PREVIEW_ROI_X0			0x2004
#define REG_PREVIEW_ROI_Y0			0x2006
#define REG_PREVIEW_ROI_X1			0x2008
#define REG_PREVIEW_ENABLE			0x2010
#define REG_PREVIEW_OUT_FMT			0x2012
#define REG_PREVIEW_SENSOR_MODE		0x2014
#define REG_PREVIEW_MAX_FPS			0x2020
#define REG_PREVIEW_AE_USG			0x2022
#define REG_PREVIEW_HINF_CTRL		0x2030
#define REG_PREVIEW_HINF_SPOOF_W	0x2032
#define REG_PREVIEW_HINF_SPOOF_H	0x2034
#define REG_AE_CTRL					0x5002
#define REG_AE_MANUAL_GAIN			0x5006
#define REG_AE_MANUAL_EXP_TIME		0x500C
#define REG_AE_BV_MIN				0x5010
#define REG_AF_CTRL					0x5058
#define REG_AWB_MANUAL_TEMP			0x510A
#define REG_BOOTDATA_STAGE			0x6002
#define REG_GAMMA					0x700A
#define REG_CLS_LOCK_CONN			0x54C2
#define REG_PREVIEW_DIV_HINF_MIPI	0x2064
#define REG_SIPS_CRC				0xF052

#define FW_OFFSET					0x8000
#define FW_PLL_SIZE					832
#define FW_CHUNK_SIZE				0x600

#define REG_16B	2
#define REG_32B	4

static int ap1302_write_reg(struct i2c_client*, u16, u32, int);

enum {
	AP1302_MODE_640x480,
	AP1302_MODE_1280x720,
	AP1302_MODE_1920x1080,
	AP1302_MODE_4224x3156,
	AP1302_SIZE_LAST,
};

#define to_ap1302(sd)		container_of(sd, struct ap1302_priv, subdev)

struct ap1302_priv {
	struct v4l2_subdev		subdev;
	struct v4l2_mbus_framefmt	mf;

	int				ident;
	u16				chip_id;
	u8				revision;

	int mode;

	struct i2c_client *client;
	const struct firmware *fw;
};

static enum v4l2_mbus_pixelcode ap1302_codes[] = {
	V4L2_MBUS_FMT_UYVY8_2X8,
};

static const struct v4l2_frmsize_discrete ap1302_frmsizes[AP1302_SIZE_LAST] = {
	{640, 480},
	{1280, 720},
	{1920, 1080},
	{4224, 3156},
};

static int ap1302_find_mode(u32 width, u32 height)
{
	int i;

	for (i = 0; i < AP1302_SIZE_LAST; i++) {
		if ((ap1302_frmsizes[i].width >= width) &&
		    (ap1302_frmsizes[i].height >= height))
			break;
	}

	/* If not found, select biggest */
	if (i >= AP1302_SIZE_LAST)
		i = AP1302_SIZE_LAST - 1;

	return i;
}

static void ap1302_set_default_fmt(struct ap1302_priv *priv)
{
	struct v4l2_mbus_framefmt *mf = &priv->mf;

	mf->width = ap1302_frmsizes[AP1302_MODE_640x480].width;
	mf->height = ap1302_frmsizes[AP1302_MODE_640x480].height;
	mf->code = V4L2_MBUS_FMT_UYVY8_2X8;
	mf->field = V4L2_FIELD_NONE;
	mf->colorspace = V4L2_COLORSPACE_SRGB;
}

/* Start/Stop streaming from the device */
static int ap1302_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ap1302_priv *priv = to_ap1302(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (!enable) {
		ap1302_set_default_fmt(priv);
		return 0;
	}

	ap1302_write_reg(client, REG_PREVIEW_OUT_FMT, 0x0030, REG_16B);
	ap1302_write_reg(client, REG_PREVIEW_WIDTH, priv->mf.width, REG_16B);
	ap1302_write_reg(client, REG_PREVIEW_HEIGHT, priv->mf.height, REG_16B);
	ap1302_write_reg(client, REG_PREVIEW_SENSOR_MODE, (priv->mf.height > 1080) ? 0x0040 : 0x0041, REG_16B);
	ap1302_write_reg(client, REG_PREVIEW_HINF_SPOOF_W, 0x0000, REG_16B);
	ap1302_write_reg(client, REG_PREVIEW_HINF_SPOOF_H, 0x0000, REG_16B);

	return ret;
}

static int ap1302_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *mf)
{
	int mode;

	mode = ap1302_find_mode(mf->width, mf->height);
	mf->width = ap1302_frmsizes[mode].width;
	mf->height = ap1302_frmsizes[mode].height;

	mf->field = V4L2_FIELD_NONE;
	mf->code = V4L2_MBUS_FMT_UYVY8_2X8;
	mf->colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

/* set the format we will capture in */
static int ap1302_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct ap1302_priv *priv = to_ap1302(sd);
	int ret;

	ret = ap1302_try_fmt(sd, mf);
	if (ret < 0)
		return ret;

	priv->mode = ap1302_find_mode(mf->width, mf->height);

	memcpy(&priv->mf, mf, sizeof(struct v4l2_mbus_framefmt));

	return 0;
}

static int ap1302_s_power(struct v4l2_subdev *sd, int on)
{
	struct ap1302_priv *priv = to_ap1302(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *scsd = soc_camera_i2c_to_desc(client);

	if (on)
		ap1302_s_fmt(sd, &priv->mf);

	return soc_camera_set_power(&client->dev, scsd, on);
}

static int ap1302_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(ap1302_codes))
		return -EINVAL;

	*code = ap1302_codes[index];

	return 0;
}

static int ap1302_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	return 0;
}

static int ap1302_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	return 0;
}

static int ap1302_querystd(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	return 0;
}

/* Get chip identification */
static int ap1302_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *id)
{
	struct ap1302_priv *priv = to_ap1302(sd);

	id->ident = priv->ident;
	id->revision = priv->revision;

	return 0;
}

static struct v4l2_subdev_video_ops ap1302_video_ops = {
	.s_stream		= ap1302_s_stream,
	.s_mbus_fmt		= ap1302_s_fmt,
	.try_mbus_fmt	= ap1302_try_fmt,
	.enum_mbus_fmt	= ap1302_enum_fmt,
	.cropcap		= ap1302_cropcap,
	.g_crop			= ap1302_g_crop,
	.querystd 		= ap1302_querystd,
};

	int (*reset)(struct v4l2_subdev *sd, u32 val);

static struct v4l2_subdev_core_ops ap1302_core_ops = {
	.g_chip_ident	= ap1302_g_chip_ident,
	.s_power		= ap1302_s_power,
};

static struct v4l2_subdev_ops ap1302_subdev_ops = {
	.core			= &ap1302_core_ops,
	.video			= &ap1302_video_ops,
};

static int ap1302_read_reg(struct i2c_client *client, u16 addr, u16 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, REG_16B);

	if (err != 2)
		return -EINVAL;

	*val = (data[2] << 8) | data[3];

	return 0;
}

static int ap1302_write_reg(struct i2c_client *client, u16 addr, u32 value, int size)
{
	int count;
	struct i2c_msg msg[1];
	unsigned char data[6];

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	if (size == 1) {
		data[2] = (u8) (value & 0xff);
	}
	else if (size == 2)	{
		data[2] = (u8) (value >> 8);
		data[3] = (u8) (value & 0xff);
	}
	else if (size == 4) {
		data[2] = (u8) (value >> 24);
		data[3] = (u8) (value >> 16);
		data[4] = (u8) (value >> 8);
		data[5] = (u8) (value & 0xff);
	}
	else
		return -1;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = (size + 2);
	msg[0].buf = data;

	count = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (count == ARRAY_SIZE(msg)) {
		return 0;
	}
	dev_err(&client->dev,
		"ap1302: i2c transfer failed, addr: %x, value: %02x\n",
	       addr, (u32)value);
	return -EIO;
}

static int ap1302_write_bulk_reg(struct i2c_client *client, u16 reg_addr, u8 *data, int len)
{
	int err;
	struct i2c_msg msg;
	u8 data_with_addr[len+2];

	if (!client->adapter)
		return -ENODEV;

	data_with_addr[0] = (u8)(reg_addr >> 8);
	data_with_addr[1] = (u8)(reg_addr & 0xFF);
	memcpy((u8*)&data_with_addr[2], data, len);

	len = (len + 2);
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data_with_addr;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err == 1)
		return 0;

	dev_err(&client->dev, "ap1302: i2c transfer failed at %x\n",
		(int)data[0] << 8 | data[1]);

	return err;
}

static int ap1302_request_firmware(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ap1302_priv *priv = to_ap1302(sd);
	int ret;
	ret = request_firmware(&priv->fw, "ap1302_fw.bin", &client->dev);
	if (ret)
		dev_err(&client->dev,
			"ap1302_request_firmware failed. ret=%d\n", ret);
	return ret;
}

static int ap1302_write_firmware(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ap1302_priv *priv = to_ap1302(sd);
	int err = 0;
	u16 pos = 0, ppos = 0, smpos = 0;
	int fw_size = priv->fw->size;
	u8 *fw_data = (u8 *)(priv->fw->data);
	u32 packet_size = 0, sm_packet_size = 0;

	err += ap1302_write_reg(client, REG_SIPS_CRC, 0xFFFF, REG_16B);

	/* write pll firmware */
	err += ap1302_write_bulk_reg(client, 0x8000, fw_data, FW_PLL_SIZE);

	/* write the rest of the firmware */
	err += ap1302_write_reg(client, REG_BOOTDATA_STAGE, 0x0002, REG_16B);
	pos = FW_PLL_SIZE;
	ppos = FW_PLL_SIZE;

	/* split fw into packets (8192B) */
	for (pos = FW_PLL_SIZE; pos < fw_size; pos += packet_size) {
		if (fw_size - pos < AP1302_FW_WINDOW_SIZE - ppos)
			packet_size = fw_size - pos;
		else
			packet_size = AP1302_FW_WINDOW_SIZE - ppos;

		/* split each packet into chunks (each having FW_CHUNK_SIZE) */
		sm_packet_size = FW_CHUNK_SIZE;
		for (smpos = 0; smpos < packet_size; smpos += sm_packet_size) {
			if ((smpos + sm_packet_size) > packet_size)
				sm_packet_size = packet_size - smpos;
				err += ap1302_write_bulk_reg(client, (FW_OFFSET + ppos + smpos),
					(u8 *)(fw_data + pos + smpos), sm_packet_size);
		}

		msleep(10);

		ppos += packet_size;
		if (ppos >= AP1302_FW_WINDOW_SIZE)
			ppos = 0;
	}

	/* TODO: check CRC and ERROR regs */
	ap1302_write_reg(client, REG_BOOTDATA_STAGE, 0xFFFF, REG_16B);

	return err;
}

static int ap1302_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct ap1302_priv *priv;
	struct soc_camera_subdev_desc *scsd;
	u16 chip_id = 0;
	int ret = 0;

	/* Checking soc-camera interface */
	scsd = soc_camera_i2c_to_desc(client);
	if (!scsd) {
		dev_err(&client->dev, "Missing soc_camera_link for driver\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&client->dev, sizeof(struct ap1302_priv),
				GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "Failed to allocate private data!\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &ap1302_subdev_ops);

	priv->client = client;

	soc_camera_power_on(&client->dev, scsd);

	/* TODO: this should be moved to boardfile, because
	 * now the driver works only on Jetson TK1 */
	gpio_set_value(219, 0);
	msleep(100);
	gpio_set_value(219, 1);
	msleep(100);

	ap1302_set_default_fmt(priv);

	ap1302_read_reg(client, REG_CHIP_VERSION, &chip_id);
	printk(KERN_INFO "AP1302 chip id = 0x%04x\n", chip_id);
	if (chip_id != 0x265) {
		soc_camera_power_off(&client->dev, scsd);
		return -1;
	}

	ret = ap1302_request_firmware(&(priv->subdev));
	if (ret) {
		dev_err(&client->dev, "Cannot request ap1302 firmware.\n");
		return -1;
	}

	/* loading firmware */
	ret += ap1302_write_firmware(&(priv->subdev));

	/* configuring isp */
	ret += ap1302_write_reg(client, REG_PREVIEW_WIDTH, 0x0280, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_HEIGHT, 0x01E0, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_OUT_FMT, 0x0030, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_MAX_FPS, 0xF000, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_HINF_CTRL, 0x0034, REG_16B);
	ret += ap1302_write_reg(client, REG_AF_CTRL, 0x0000, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_ENABLE, 0x0086, REG_16B);
	ret += ap1302_write_reg(client, REG_AE_BV_MIN, 0x0100, REG_16B);
	ret += ap1302_write_reg(client, REG_AE_MANUAL_GAIN, 0x0000, REG_16B);
	ret += ap1302_write_reg(client, REG_AE_CTRL, 0x002A, REG_16B);
	ret += ap1302_write_reg(client, REG_ENABLE, 0x0001, REG_16B);
	ret += ap1302_write_reg(client, REG_AE_MANUAL_EXP_TIME, 0x00003CF0, REG_32B);
	ret += ap1302_write_reg(client, REG_AWB_MANUAL_TEMP, 0x11F8, REG_16B);
	ret += ap1302_write_reg(client, REG_GAMMA, 0x2000, REG_16B);
	ret += ap1302_write_reg(client, REG_CLS_LOCK_CONN, 0x0024, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_ROI_X0, 0x8000, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_ROI_X1, 0x7FFF, REG_16B);
	ret += ap1302_write_reg(client, REG_CLS_LOCK_CONN, 0x0024, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_DIV_HINF_MIPI, 0x00030000, REG_32B);
	ret += ap1302_write_reg(client, REG_CTRL, 0x0001, REG_16B);
	ret += ap1302_write_reg(client, REG_CTRL, 0x0000, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_AE_USG, 0x0000, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_WIDTH, 0x0280, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_HEIGHT, 0x01E0, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_SENSOR_MODE, 0x0041, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_HINF_SPOOF_W, 0x0000, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_HINF_SPOOF_H, 0x0000, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_OUT_FMT, 0x0030, REG_16B);

	/* setting default mode: 640x480 */
	ret += ap1302_write_reg(client, REG_PREVIEW_WIDTH, 0x0280, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_HEIGHT, 0x01E0, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_SENSOR_MODE, 0x0041, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_HINF_SPOOF_W, 0x0000, REG_16B);
	ret += ap1302_write_reg(client, REG_PREVIEW_HINF_SPOOF_H, 0x0000, REG_16B);

	return ret;
}

static int ap1302_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ap1302_id[] = {
	{ "ap1302", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ap1302_id);

static struct i2c_driver ap1302_i2c_driver = {
	.driver = {
		.name = "ap1302",
	},
	.probe    = ap1302_probe,
	.remove   = ap1302_remove,
	.id_table = ap1302_id,
};

static int __init ap1302_module_init(void)
{
	return i2c_add_driver(&ap1302_i2c_driver);
}

static void __exit ap1302_module_exit(void)
{
	i2c_del_driver(&ap1302_i2c_driver);
}

module_init(ap1302_module_init);
module_exit(ap1302_module_exit);

MODULE_DESCRIPTION("SoC Camera driver for AP1302 ISP from ON Semiconductor");
MODULE_AUTHOR("Wojciech Bieganski <wbieganski@antmicro.com>");
MODULE_LICENSE("GPL v2");
