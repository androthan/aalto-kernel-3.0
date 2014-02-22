/* USB charger/OTG code for Samsung YP-GS1
 * Jonathan Grundmann, androthan@gmail.com, 2014
 */

/* Copyright (C) 2013 Dheeraj CVR (cvr.dheeraj@gmail.com)
 */

/* Based on mach-omap2/board-latona-connector.c
 */

/* This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/fsa9480.h>
#include <linux/usb/otg.h>
#include <linux/delay.h>
#include <linux/i2c/twl.h>
#include <linux/mutex.h>
#include <linux/switch.h>
#include <mach/board-aalto.h>
#include <linux/wakelock.h>

struct aalto_otg {
	struct otg_transceiver	otg;
	struct device		dev;

	struct mutex		lock;

	bool			usb_connected;
	bool			charger_connected;
	int			current_device;
	int                     irq_ta_nconnected;
	struct wake_lock 	usb_lock;
};

static struct aalto_otg aalto_otg_xceiv;

static int aalto_otg_set_peripheral(struct otg_transceiver *otg,
				 struct usb_gadget *gadget)
{
	otg->gadget = gadget;
	if (!gadget)
		otg->state = OTG_STATE_UNDEFINED;
	return 0;
}

static void aalto_usb_attach(struct aalto_otg *aalto_otg)
{
	pr_info("[%s]\n", __func__);

	if (!aalto_otg->usb_connected) {
		aalto_otg->otg.state = OTG_STATE_B_IDLE;
		aalto_otg->otg.default_a = false;
		aalto_otg->otg.last_event = USB_EVENT_VBUS;

		atomic_notifier_call_chain(&aalto_otg->otg.notifier,
				   USB_EVENT_VBUS, aalto_otg->otg.gadget);

		aalto_otg->usb_connected = true;

		if(!wake_lock_active(&aalto_otg->usb_lock))
			wake_lock(&aalto_otg->usb_lock);
	}
}

static void aalto_detach(struct aalto_otg *aalto_otg)
{
	pr_info("[%s]\n", __func__);

	if ( aalto_otg->usb_connected || aalto_otg->charger_connected ) {
		aalto_otg->otg.state = OTG_STATE_B_IDLE;
		aalto_otg->otg.default_a = false;
		aalto_otg->otg.last_event = USB_EVENT_NONE;

		atomic_notifier_call_chain(&aalto_otg->otg.notifier,
				   USB_EVENT_NONE, aalto_otg->otg.gadget);

		aalto_otg->usb_connected = false;
		aalto_otg->charger_connected = false;

		if(wake_lock_active(&aalto_otg->usb_lock))
			wake_unlock(&aalto_otg->usb_lock);
	}
}

static void aalto_charger_attach(struct aalto_otg *aalto_otg)
{
	pr_info("[%s]\n", __func__);

	if (!aalto_otg->charger_connected) {
		aalto_otg->otg.state = OTG_STATE_B_IDLE;
		aalto_otg->otg.default_a = false;
		aalto_otg->otg.last_event = USB_EVENT_CHARGER;
		atomic_notifier_call_chain(&aalto_otg->otg.notifier,
			USB_EVENT_CHARGER, aalto_otg->otg.gadget);

		aalto_otg->charger_connected = true;
	}
}

static void aalto_fsa_device_detected(int device)
{
	struct aalto_otg *aalto_otg = &aalto_otg_xceiv;

	mutex_lock(&aalto_otg->lock);

	pr_debug("detected %x\n", device);
	switch (device) {
	case MICROUSBIC_USB_CABLE:
		aalto_usb_attach(aalto_otg);
		break;
	case MICROUSBIC_USB_CHARGER:
	case MICROUSBIC_5W_CHARGER:
	case MICROUSBIC_TA_CHARGER:
		aalto_charger_attach(aalto_otg);
		break;
	case MICROUSBIC_NO_DEVICE:
	default:
		aalto_detach(aalto_otg);
		break;
	}

	mutex_unlock(&aalto_otg->lock);
}

static struct fsa9480_platform_data aalto_fsa9480_pdata = {
	.detected	= aalto_fsa_device_detected,
};

static struct i2c_board_info __initdata aalto_connector_i2c2_boardinfo[] = {
	{
		I2C_BOARD_INFO("fsa9480", 0x25),
		.flags = I2C_CLIENT_WAKE,
		.irq = OMAP_GPIO_IRQ(OMAP_GPIO_JACK_NINT),
		.platform_data = &aalto_fsa9480_pdata,
	},
};

void __init aalto_connector_init(void)
{
	struct aalto_otg *aalto_otg = &aalto_otg_xceiv;
	int ret;

	mutex_init(&aalto_otg->lock);

	wake_lock_init(&aalto_otg->usb_lock, WAKE_LOCK_SUSPEND, "aalto_usb_wakelock");

	device_initialize(&aalto_otg->dev);
	dev_set_name(&aalto_otg->dev, "%s", "aalto_otg");
	ret = device_add(&aalto_otg->dev);
	if (ret) {
		pr_err("%s: cannot reg device '%s' (%d)\n", __func__,
		       dev_name(&aalto_otg->dev), ret);
		return;
	}

	dev_set_drvdata(&aalto_otg->dev, aalto_otg);

	aalto_otg->otg.dev		= &aalto_otg->dev;
	aalto_otg->otg.label		= "aalto_otg_xceiv";
	aalto_otg->otg.set_peripheral	= aalto_otg_set_peripheral;

	ATOMIC_INIT_NOTIFIER_HEAD(&aalto_otg->otg.notifier);

	ret = otg_set_transceiver(&aalto_otg->otg);
	if (ret)
		pr_err("aalto_otg: cannot set transceiver (%d)\n", ret);

	i2c_register_board_info(2, aalto_connector_i2c2_boardinfo,
				ARRAY_SIZE(aalto_connector_i2c2_boardinfo));
}
