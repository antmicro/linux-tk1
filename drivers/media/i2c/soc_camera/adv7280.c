/*
 * ADV7280 camera decoder driver, based on ADV7280 driver
 *
 * Copyright (c) 2014 Antmicro Ltd <www.antmicro.com>
 * Based on ADV7180 video decoder driver,
 * Copyright (c) 2009 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <media/v4l2-ioctl.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#define DRIVER_NAME "adv7280"

#define HW_DEINT /* Enable hardware deinterlacer */
#define CSI
#define VPP_SLAVE_ADDRESS 0x42
#define CSI_SLAVE_ADDRESS 0x44

/* User Sub Map Regs */
#define ADV7280_INPUT_CONTROL 0x00
#define ADV7280_VIDEO_SELECTION_1 0x01
#define ADV7280_VIDEO_SELECTION_2 0x02
#define ADV7280_OUTPUT_CONTROL 0x03
#define ADV7280_EXTENDED_OUTPUT_CONTROL 0x04
#define ADV7280_AUTODETECT_ENABLE 0x07
#define ADV7280_ADI_CONTROL_1 0x0E
#define ADV7280_POWER_MANAGEMENT 0x0F
#define ADV7280_STATUS_1 0x10
#define ADV7280_IDENT 0x11
#define ADV7280_SHAPING_FILTER_CONTROL_1 0x17
#define ADV7280_ADI_CONTROL_2 0x1D
#define ADV7280_PIXEL_DELAY_CONTROL 0x27
#define ADV7280_VPP_SLAVE_ADDRESS 0xFD
#define ADV7280_CSI_TX_SLAVE_ADDRESS 0xFE
#define ADV7280_OUTPUT_SYNC_SELECT_2 0x6B

/* VPP regs (Video Postprocessor) */
#define VPP_DEINT_RESET 0x41
#define VPP_I2C_DEINT_ENABLE 0x55
#define VPP_ADV_TIMING_MODE_EN 0x5B

/* CSI regs (Camera Serial Interface, MIPI-CSI2) */
#define CSI_CSITX_PWRDN 0x00
#define CSI_DPHY_PWRDN_CTL 0xDE

/* other */
#define ADV7280_EXTENDED_OUTPUT_CONTROL_NTSCDIS		0xC5
#define ADV7280_AUTODETECT_DEFAULT			0x7f

#define ADV7280_INPUT_CONTROL_COMPOSITE_IN1		0x00
#define ADV7280_INPUT_CONTROL_AD_PAL_BG_NTSC_J_SECAM	0x00

#define ADV7280_INPUT_CONTROL_NTSC_M 0x50
#define ADV7280_INPUT_CONTROL_PAL60 0x60
#define ADV7280_INPUT_CONTROL_NTSC_443 0x70
#define ADV7280_INPUT_CONTROL_PAL_BG 0x80
#define ADV7280_INPUT_CONTROL_PAL_N 0x90
#define ADV7280_INPUT_CONTROL_PAL_M 0xa0
#define ADV7280_INPUT_CONTROL_PAL_M_PED 0xb0
#define ADV7280_INPUT_CONTROL_PAL_COMB_N 0xc0
#define ADV7280_INPUT_CONTROL_PAL_COMB_N_PED 0xd0
#define ADV7280_INPUT_CONTROL_PAL_SECAM 0xe0

#define ADV7280_ADI_CTRL_IRQ_SPACE			0x20

#define ADV7280_STATUS1_IN_LOCK		0x01
#define ADV7280_STATUS1_AUTOD_MASK	0x70
#define ADV7280_STATUS1_AUTOD_NTSM_M_J	0x00
#define ADV7280_STATUS1_AUTOD_NTSC_4_43 0x10
#define ADV7280_STATUS1_AUTOD_PAL_M	0x20
#define ADV7280_STATUS1_AUTOD_PAL_60	0x30
#define ADV7280_STATUS1_AUTOD_PAL_B_G	0x40
#define ADV7280_STATUS1_AUTOD_SECAM	0x50
#define ADV7280_STATUS1_AUTOD_PAL_COMB	0x60
#define ADV7280_STATUS1_AUTOD_SECAM_525	0x70

#define ADV7280_ICONF1_ADI		0x40
#define ADV7280_ICONF1_ACTIVE_LOW	0x01
#define ADV7280_ICONF1_PSYNC_ONLY	0x10

#define ADV7280_IMR1_ADI	0x44
#define ADV7280_IMR2_ADI	0x48
#define ADV7280_IRQ3_AD_CHANGE	0x08
#define ADV7280_ISR3_ADI	0x4A
#define ADV7280_ICR3_ADI	0x4B
#define ADV7280_IMR3_ADI	0x4C
#define ADV7280_IMR4_ADI	0x50

struct adv7280_state {
	struct v4l2_subdev	sd;
	struct work_struct	work;
	struct mutex		mutex; /* mutual excl. when accessing chip */
	int			irq;
	v4l2_std_id		curr_norm;
	bool			autodetect;
	int			active_input;
};

static v4l2_std_id adv7280_std_to_v4l2(u8 status1)
{
	switch (status1 & ADV7280_STATUS1_AUTOD_MASK) {
	case ADV7280_STATUS1_AUTOD_NTSM_M_J:
		return V4L2_STD_NTSC;
	case ADV7280_STATUS1_AUTOD_NTSC_4_43:
		return V4L2_STD_NTSC_443;
	case ADV7280_STATUS1_AUTOD_PAL_M:
		return V4L2_STD_PAL_M;
	case ADV7280_STATUS1_AUTOD_PAL_60:
		return V4L2_STD_PAL_60;
	case ADV7280_STATUS1_AUTOD_PAL_B_G:
		return V4L2_STD_PAL;
	case ADV7280_STATUS1_AUTOD_SECAM:
		return V4L2_STD_SECAM;
	case ADV7280_STATUS1_AUTOD_PAL_COMB:
		return V4L2_STD_PAL_Nc | V4L2_STD_PAL_N;
	case ADV7280_STATUS1_AUTOD_SECAM_525:
		return V4L2_STD_SECAM;
	default:
		return V4L2_STD_UNKNOWN;
	}
}

static int v4l2_std_to_adv7280(v4l2_std_id std)
{
	if (std == V4L2_STD_PAL_60)
		return ADV7280_INPUT_CONTROL_PAL60;
	if (std == V4L2_STD_NTSC_443)
		return ADV7280_INPUT_CONTROL_NTSC_443;
	if (std == V4L2_STD_PAL_N)
		return ADV7280_INPUT_CONTROL_PAL_N;
	if (std == V4L2_STD_PAL_M)
		return ADV7280_INPUT_CONTROL_PAL_M;
	if (std == V4L2_STD_PAL_Nc)
		return ADV7280_INPUT_CONTROL_PAL_COMB_N;

	if (std & V4L2_STD_PAL)
		return ADV7280_INPUT_CONTROL_PAL_BG;
	if (std & V4L2_STD_NTSC)
		return ADV7280_INPUT_CONTROL_NTSC_M;
	if (std & V4L2_STD_SECAM)
		return ADV7280_INPUT_CONTROL_PAL_SECAM;

	return -EINVAL;
}

static int adv7280_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	i2c_smbus_write_byte_data(client, reg, val);
	return 0;
}

static u32 adv7280_status_to_v4l2(u8 status1)
{
	if (!(status1 & ADV7280_STATUS1_IN_LOCK))
		return V4L2_IN_ST_NO_SIGNAL;

	return 0;
}

static int __adv7280_status(struct i2c_client *client, u32 *status,
	v4l2_std_id *std)
{
	int status1 = i2c_smbus_read_byte_data(client, ADV7280_STATUS_1);

	if (status1 < 0)
		return status1;

	if (status)
		*status = adv7280_status_to_v4l2(status1);
	if (std)
		*std = adv7280_std_to_v4l2(status1);

	return 0;
}

static inline struct adv7280_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct adv7280_state, sd);
}

static int adv7280_querystd(struct v4l2_subdev *sd, v4l2_std_id *std)
{
	struct adv7280_state *state = to_state(sd);
	int err = mutex_lock_interruptible(&state->mutex);
	if (err)
		return err;

	/* when we are interrupt driven we know the state */
	if (!state->autodetect || state->irq > 0)
		*std = state->curr_norm;
	else
		err = __adv7280_status(v4l2_get_subdevdata(sd), NULL, std);

	mutex_unlock(&state->mutex);
	return err;
}

static int adv7280_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct adv7280_state *state = to_state(sd);
	int ret = mutex_lock_interruptible(&state->mutex);
	if (ret)
		return ret;

	ret = __adv7280_status(v4l2_get_subdevdata(sd), status, NULL);
	mutex_unlock(&state->mutex);
	return ret;
}

/*
static int adv7280_g_chip_ident(struct v4l2_subdev *sd,
	struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_ADV7280, 0);
}
*/

static int adv7280_s_std(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct adv7280_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = mutex_lock_interruptible(&state->mutex);
	if (ret)
		return ret;

	/* all standards -> autodetect */
	if (std == V4L2_STD_ALL) {
		ret = adv7280_write_reg(client,
			ADV7280_INPUT_CONTROL,
			ADV7280_INPUT_CONTROL_AD_PAL_BG_NTSC_J_SECAM |
					(ADV7280_INPUT_CONTROL_COMPOSITE_IN1 +
					 state->active_input));
		if (ret < 0)
			goto out;

		__adv7280_status(client, NULL, &state->curr_norm);
		state->autodetect = true;
	} else {
		ret = v4l2_std_to_adv7280(std) |
				(ADV7280_INPUT_CONTROL_COMPOSITE_IN1 +
				 state->active_input);
		if (ret < 0)
			goto out;

		ret = adv7280_write_reg(client,
			ADV7280_INPUT_CONTROL, ret);
		if (ret < 0)
			goto out;

		state->curr_norm = std;
		state->autodetect = false;
	}
	ret = 0;
out:
	mutex_unlock(&state->mutex);
	return ret;
}

/*
static int adv7280_set_bus_param(struct soc_camera_device *icd,
				 unsigned long flags)
{
	return 0;
}
*/

/* Request bus settings on camera side */
/*
static unsigned long adv7280_query_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_link *icl = to_soc_camera_link(icd);

	unsigned long flags = SOCAM_PCLK_SAMPLE_RISING | SOCAM_MASTER |
		SOCAM_VSYNC_ACTIVE_HIGH | SOCAM_HSYNC_ACTIVE_HIGH |
		SOCAM_DATA_ACTIVE_HIGH | SOCAM_DATAWIDTH_8;

	return soc_camera_apply_sensor_flags(icl, flags);
}
*/

static enum v4l2_mbus_pixelcode adv7280_codes[] = {
	V4L2_MBUS_FMT_UYVY8_2X8,
};

static int adv7280_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	enum v4l2_colorspace cspace;
	enum v4l2_mbus_pixelcode code = mf->code;

	switch (code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
		cspace = V4L2_COLORSPACE_SRGB;
		break;
	default:
		return -EINVAL;
	}

	mf->code        = code;
	mf->colorspace  = cspace;

	return adv7280_s_std(sd, V4L2_STD_ALL);
}

static int adv7280_try_fmt(struct v4l2_subdev *sd,
                          struct v4l2_mbus_framefmt *mf)
{
#ifdef HW_DEINT
	mf->field = V4L2_FIELD_NONE;
#else
	mf->field = V4L2_FIELD_INTERLACED_TB;
#endif
	mf->code = V4L2_MBUS_FMT_UYVY8_2X8;
	mf->colorspace = V4L2_COLORSPACE_SRGB;

	// PAL
	mf->width = 640;
	mf->height = 576;

	return 0;
}

static int adv7280_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *scsd = soc_camera_i2c_to_desc(client);

	return soc_camera_set_power(&client->dev, scsd, on);
}

static int adv7280_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
                           enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(adv7280_codes))
		return -EINVAL;

	*code = adv7280_codes[index];

	return 0;
}

/*
static struct soc_camera_ops adv7280_ops = {
	.set_bus_param		= adv7280_set_bus_param,
	.query_bus_param	= adv7280_query_bus_param,
};
*/

static const struct v4l2_subdev_video_ops adv7280_video_ops = {
	.s_mbus_fmt		= adv7280_s_fmt,
	.try_mbus_fmt		= adv7280_try_fmt,
	.enum_mbus_fmt		= adv7280_enum_fmt,
	.querystd		= adv7280_querystd,
	.g_input_status		= adv7280_g_input_status,
};

static const struct v4l2_subdev_core_ops adv7280_core_ops = {
	//.g_chip_ident		= adv7280_g_chip_ident,
	.s_std			= adv7280_s_std,
	.s_power		= adv7280_s_power,
};

static const struct v4l2_subdev_ops adv7280_subdev_ops = {
	.core			= &adv7280_core_ops,
	.video			= &adv7280_video_ops,
};

static void adv7280_work(struct work_struct *work)
{
	struct adv7280_state *state = container_of(work, struct adv7280_state,
		work);
	struct i2c_client *client = v4l2_get_subdevdata(&state->sd);
	u8 isr3;

	mutex_lock(&state->mutex);
	adv7280_write_reg(client, ADV7280_ADI_CONTROL_1,
		ADV7280_ADI_CTRL_IRQ_SPACE);
	isr3 = i2c_smbus_read_byte_data(client, ADV7280_ISR3_ADI);
	/* clear */
	adv7280_write_reg(client, ADV7280_ICR3_ADI, isr3);
	adv7280_write_reg(client, ADV7280_ADI_CONTROL_1, 0);

	if (isr3 & ADV7280_IRQ3_AD_CHANGE && state->autodetect)
		__adv7280_status(client, NULL, &state->curr_norm);
	mutex_unlock(&state->mutex);

	enable_irq(state->irq);
}

static irqreturn_t adv7280_irq(int irq, void *devid)
{
	struct adv7280_state *state = devid;

	schedule_work(&state->work);

	disable_irq_nosync(state->irq);

	return IRQ_HANDLED;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i) {
	struct soc_camera_device *icd = file->private_data;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct adv7280_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 val;

	if (i < 6) {
		state->active_input = i;
		val = i2c_smbus_read_byte_data(client,
				ADV7280_INPUT_CONTROL);
		val &= 0xf0;
		val |= (ADV7280_INPUT_CONTROL_COMPOSITE_IN1 +
			state->active_input);
		return adv7280_write_reg(client,
				ADV7280_INPUT_CONTROL, val);
	}
	return -EINVAL;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i) {
	struct soc_camera_device *icd = file->private_data;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct adv7280_state *state = to_state(sd);

	*i = state->active_input;

	return 0;
}

/*
 * Generic i2c probe
 * concerning the addresses: i2c wants 7 bit (without the r/w bit), so '>>1'
 */
static int adv7280_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct adv7280_state *state;
	struct soc_camera_device *icd = client->dev.platform_data;
	struct v4l2_subdev *sd;
	u8 ident;
	int ret;
	struct v4l2_ioctl_ops *ops;
	struct i2c_client vpp_client = {
		.flags = client->flags,
		.addr = VPP_SLAVE_ADDRESS,
		.name = "ADV7180_VPP_SLAVE",
		.adapter = client->adapter,
		.dev = client->dev,
	};
	struct i2c_client csi_client = {
		.flags = client->flags,
		.addr = CSI_SLAVE_ADDRESS,
		.name = "ADV7180_CSI_SLAVE",
		.adapter = client->adapter,
		.dev = client->dev,
	};

	printk("probe, id=%s\n", id->name);
	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	v4l_info(client, "chip found @ 0x%02x (%s)\n",
			client->addr << 1, client->adapter->name);

	state = kzalloc(sizeof(struct adv7280_state), GFP_KERNEL);
	if (state == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ident = i2c_smbus_read_byte_data(client, ADV7280_IDENT);
	v4l_info(client, "ident reg is 0x%02x\n", ident);

	state->irq = client->irq;
	INIT_WORK(&state->work, adv7280_work);
	mutex_init(&state->mutex);
	state->autodetect = true;
	state->active_input = 0; // input 1
	sd = &state->sd;
	v4l2_i2c_subdev_init(sd, client, &adv7280_subdev_ops);
	//icd->ops = &adv7280_ops;

	/* Reset */
	ret = adv7280_write_reg(client,
		ADV7280_POWER_MANAGEMENT, 0xA0);
	if (ret < 0)
		goto err_unreg_subdev;
	msleep(10);

	/* Initialize adv7280 */
	/* Exit Power Down Mode */
	ret = adv7280_write_reg(client,
		ADV7280_POWER_MANAGEMENT, 0x00);
	if (ret < 0)
		goto err_unreg_subdev;

	/* analog devices recommends */
	ret = adv7280_write_reg(client,
		ADV7280_ADI_CONTROL_1, 0x80);
	if (ret < 0)
		goto err_unreg_subdev;

	/* analog devices recommends */
	ret = adv7280_write_reg(client,
		0x9C, 0x00);
	if (ret < 0)
		goto err_unreg_subdev;

	/* analog devices recommends */
	ret = adv7280_write_reg(client,
		0x9C, 0xFF);
	if (ret < 0)
		goto err_unreg_subdev;

	/* Enter User Sub Map */
	ret = adv7280_write_reg(client,
		ADV7280_ADI_CONTROL_1, 0x00);
	if (ret < 0)
		goto err_unreg_subdev;

	/* Enable Pixel & Sync output drivers */
	ret = adv7280_write_reg(client,
		ADV7280_OUTPUT_CONTROL, 0x0C);
	if (ret < 0)
		goto err_unreg_subdev;

	/* Power-up INTRQ, HS & VS pads */
	ret = adv7280_write_reg(client,
		ADV7280_EXTENDED_OUTPUT_CONTROL, 0x07);
	if (ret < 0)
		goto err_unreg_subdev;

	/* Enable SH1 */
	/*
	ret = adv7280_write_reg(client,
		ADV7280_SHAPING_FILTER_CONTROL_1, 0x41);
	if (ret < 0)
		goto err_unreg_subdev;
	*/

	/* Disable comb filtering */
	/*
	ret = adv7280_write_reg(client,
		0x39, 0x24);
	if (ret < 0)
		goto err_unreg_subdev;
	*/

	/* Enable LLC output driver */
	ret = adv7280_write_reg(client,
		ADV7280_ADI_CONTROL_2, 0x40);
	if (ret < 0)
		goto err_unreg_subdev;

	/* VSYNC on VS/FIELD/SFL pin */
	ret = adv7280_write_reg(client,
		ADV7280_OUTPUT_SYNC_SELECT_2, 0x01);
	if (ret < 0)
		goto err_unreg_subdev;

	/* Enable autodetection */
	ret = adv7280_write_reg(client, ADV7280_INPUT_CONTROL,
		ADV7280_INPUT_CONTROL_AD_PAL_BG_NTSC_J_SECAM |
				(ADV7280_INPUT_CONTROL_COMPOSITE_IN1 +
				 state->active_input));
	if (ret < 0)
		goto err_unreg_subdev;

	ret = adv7280_write_reg(client, ADV7280_AUTODETECT_ENABLE,
		ADV7280_AUTODETECT_DEFAULT);
	if (ret < 0)
		goto err_unreg_subdev;

	/* ITU-R BT.656-4 compatible
	ret = adv7280_write_reg(client,
		ADV7280_EXTENDED_OUTPUT_CONTROL,
		ADV7280_EXTENDED_OUTPUT_CONTROL_NTSCDIS);
	if (ret < 0)
		goto err_unreg_subdev;
		*/

	/* analog devices recommends */
	ret = adv7280_write_reg(client,
		0x52, 0xCD);
	if (ret < 0)
		goto err_unreg_subdev;

	/* analog devices recommends */
	ret = adv7280_write_reg(client,
		0x80, 0x51);
	if (ret < 0)
		goto err_unreg_subdev;

	/* analog devices recommends */
	ret = adv7280_write_reg(client,
		0x81, 0x51);
	if (ret < 0)
		goto err_unreg_subdev;

	/* analog devices recommends */
	ret = adv7280_write_reg(client,
		0x82, 0x68);
	if (ret < 0)
		goto err_unreg_subdev;

#ifdef HW_DEINT
	/* Set VPP Map */
	ret = adv7280_write_reg(client,
		ADV7280_VPP_SLAVE_ADDRESS, (VPP_SLAVE_ADDRESS << 1));
	if (ret < 0)
		goto err_unreg_subdev;

	/* VPP - not documented */
	ret = adv7280_write_reg(&vpp_client,
		0xA3, 0x00);
	if (ret < 0)
		goto err_unreg_subdev;

	/* VPP - Enbable Advanced Timing Mode */
	ret = adv7280_write_reg(&vpp_client,
		VPP_ADV_TIMING_MODE_EN, 0x00);
	if (ret < 0)
		goto err_unreg_subdev;

	/* VPP - Enable Deinterlacer */
	ret = adv7280_write_reg(&vpp_client,
		VPP_I2C_DEINT_ENABLE, 0x80);
	if (ret < 0)
		goto err_unreg_subdev;
#endif

#ifdef CSI
	/* Set CSI Map */
	ret = adv7280_write_reg(client,
		ADV7280_CSI_TX_SLAVE_ADDRESS, (CSI_SLAVE_ADDRESS << 1));
	if (ret < 0)
		goto err_unreg_subdev;

	/* Power up MIPI D-PHY */
	ret = adv7280_write_reg(&csi_client,
		CSI_DPHY_PWRDN_CTL, 0x02);
	if (ret < 0)
		goto err_unreg_subdev;


	ret = adv7280_write_reg(&csi_client,
		0x02 , 0x18);
	if (ret < 0)
		goto err_unreg_subdev;

	/* analog devices recommends */
	ret = adv7280_write_reg(&csi_client,
		0xD2 , 0xF7);
	if (ret < 0)
		goto err_unreg_subdev;

	/* analog devices recommends */
	ret = adv7280_write_reg(&csi_client,
		0xD8 , 0x65);
	if (ret < 0)
		goto err_unreg_subdev;

	/* analog devices recommends */
	ret = adv7280_write_reg(&csi_client,
		0xE0 , 0x09);
	if (ret < 0)
		goto err_unreg_subdev;

	/* analog devices recommends */
	ret = adv7280_write_reg(&csi_client,
		0x2C , 0x00);
	if (ret < 0)
		goto err_unreg_subdev;

	/* MIPI-CSI2 power up */
	ret = adv7280_write_reg(&csi_client,
		CSI_CSITX_PWRDN , 0x00);
	if (ret < 0)
		goto err_unreg_subdev;
#endif

	/* read current norm */
	__adv7280_status(client, NULL, &state->curr_norm);

	/* register for interrupts */
	if (state->irq > 0) {
		ret = request_irq(state->irq, adv7280_irq, 0, DRIVER_NAME,
			state);
		if (ret)
			goto err_unreg_subdev;

		ret = adv7280_write_reg(client, ADV7280_ADI_CONTROL_1,
			ADV7280_ADI_CTRL_IRQ_SPACE);
		if (ret < 0)
			goto err_unreg_subdev;

		/* config the Interrupt pin to be active low */
		ret = adv7280_write_reg(client, ADV7280_ICONF1_ADI,
			ADV7280_ICONF1_ACTIVE_LOW | ADV7280_ICONF1_PSYNC_ONLY);
		if (ret < 0)
			goto err_unreg_subdev;

		ret = adv7280_write_reg(client, ADV7280_IMR1_ADI, 0);
		if (ret < 0)
			goto err_unreg_subdev;

		ret = adv7280_write_reg(client, ADV7280_IMR2_ADI, 0);
		if (ret < 0)
			goto err_unreg_subdev;

		/* enable AD change interrupts interrupts */
		ret = adv7280_write_reg(client, ADV7280_IMR3_ADI,
			ADV7280_IRQ3_AD_CHANGE);
		if (ret < 0)
			goto err_unreg_subdev;

		ret = adv7280_write_reg(client, ADV7280_IMR4_ADI, 0);
		if (ret < 0)
			goto err_unreg_subdev;

		ret = adv7280_write_reg(client, ADV7280_ADI_CONTROL_1,
			0);
		if (ret < 0)
			goto err_unreg_subdev;
	}

	/*
	 * this is the only way to support more than one input as soc_camera
	 * assumes in its own vidioc_s(g)_input implementation that only one
	 * input is present we have to override that with our own handlers.
	 */
	/*
	ops = (struct v4l2_ioctl_ops*)icd->vdev->ioctl_ops;
	ops->vidioc_s_input = &vidioc_s_input;
	ops->vidioc_g_input = &vidioc_g_input;
	*/

	return 0;

err_unreg_subdev:
	mutex_destroy(&state->mutex);
	v4l2_device_unregister_subdev(sd);
	kfree(state);
err:
	printk(KERN_ERR DRIVER_NAME ": Failed to probe: %d\n", ret);
	return ret;
}

static int adv7280_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adv7280_state *state = to_state(sd);

	if (state->irq > 0) {
		free_irq(client->irq, state);
		if (cancel_work_sync(&state->work)) {
			/*
			 * Work was pending, therefore we need to enable
			 * IRQ here to balance the disable_irq() done in the
			 * interrupt handler.
			 */
			enable_irq(state->irq);
		}
	}

	mutex_destroy(&state->mutex);
	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id adv7280_id[] = {
	{"adv7280", 0},
	{"adv7280-M", 1},
	{},
};
MODULE_DEVICE_TABLE(i2c, adv7280_id);

static struct i2c_driver adv7280_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= DRIVER_NAME,
	},
	.probe		= adv7280_probe,
	.remove		= adv7280_remove,
	.id_table	= adv7280_id,
};

static __init int adv7280_init(void)
{
	return i2c_add_driver(&adv7280_driver);
}

static __exit void adv7280_exit(void)
{
	i2c_del_driver(&adv7280_driver);
}

module_init(adv7280_init);
module_exit(adv7280_exit);

MODULE_DESCRIPTION("Analog Devices ADV7280/ADV7280-M video decoder driver");
MODULE_AUTHOR("Wojciech Bieganski <wbieganski@antmicro.com>");
MODULE_LICENSE("GPL v2");
