#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/usb/phy.h>
#include <linux/platform_device.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include <soc/cpm.h>

#ifndef BIT
#define BIT(nr)  (1UL << nr)
#endif

/*USB Parameter Control Register*/
#define USBPCR_USB_MODE			BIT(31)
#define USBPCR_AVLD_REG			BIT(30)
#define USBPCR_IDPULLUP_MASK_BIT	28
#define USBPCR_IDPULLUP_MASK_MSK		(0x3 << USBPCR_IDPULLUP_MASK_BIT)
#define USBPCR_IDPULLUP_OTG				(0x0 << USBPCR_IDPULLUP_MASK_BIT)
#define USBPCR_IDPULLUP_ALWAYS_SUSPEND	(0x1 << USBPCR_IDPULLUP_MASK_BIT)
#define USBPCR_IDPULLUP_ALWAYS			(0x2 << USBPCR_IDPULLUP_MASK_BIT)
#define USBPCR_INCR_MASK		BIT(27)
#define USBPCR_TXRISETUNE		BIT(26)		/*0*/
#define USBPCR_COMMONONN		BIT(25)
#define USBPCR_VBUSVLDEXT		BIT(24)
#define USBPCR_VBUSVLDEXTSEL	BIT(23)
#define USBPCR_POR_BIT			22
#define USBPCR_POR				BIT(USBPCR_POR_BIT)
#define USBPCR_SIDDQ			BIT(21)
#define USBPCR_OTG_DISABLE		BIT(20)
#define USBPCR_COMPDISTUNE_BIT	17
#define USBPCR_COMPDISTUNE_MSK	(0x7 << USBPCR_COMPDISTUNE_BIT)
#define USBPCR_COMPDISTUNE(x)   (((x) << USBPCR_COMPDISTUNE_BIT) & USBPCR_COMPDISTUNE_MSK)  /*4*/
#define USBPCR_OTGTUNE_BIT		14
#define USBPCR_OTGTUNE_MSK		(0x7 << USBPCR_OTGTUNE_BIT)
#define USBPCR_OTGTUNE(x)		(((x) << USBPCR_OTGTUNE_BIT) & USBPCR_OTGTUNE_MSK)			/*4*/
#define USBPCR_SQRXTUNE_BIT		11
#define USBPCR_SQRXTUNE_MSK		(0x7 << USBPCR_SQRXTUNE_BIT)
#define USBPCR_SQRXTUNE(x)		(((x) << USBPCR_SQRXTUNE_BIT) & USBPCR_SQRXTUNE_MSK)		/*3*/
#define USBPCR_TXFSLSTUNE_BIT	7
#define USBPCR_TXFSLSTUNE_MSK	(0xf << USBPCR_TXFSLSTUNE_BIT)
#define USBPCR_TXFSLSTUNE(x)	(((x) << USBPCR_TXFSLSTUNE_BIT) & USBPCR_TXFSLSTUNE_MSK)	/*2*/
#define USBPCR_TXPREEMPHTUNE	BIT(6)														/*0*/
#define USBPCR_TXHSXVTUNE_BIT	4
#define USBPCR_TXHSXVTUNE_MSK	(0x3 << USBPCR_TXHSXVTUNE_BIT)
#define USBPCR_TXHSXVTUNE(x)	(((x) << USBPCR_TXHSXVTUNE_BIT) & USBPCR_TXHSXVTUNE_MSK)	/*3*/
#define USBPCR_TXVREFTUNE_BIT	0
#define USBPCR_TXVREFTUNE_MSK	(0xf << USBPCR_TXVREFTUNE_BIT)
#define USBPCR_TXVREFTUNE(x)	(((x) << USBPCR_TXVREFTUNE_BIT) & USBPCR_TXVREFTUNE_MSK)    /*4*/

/*USB Reset Detect Timer Register*/
#define USBRDT_HB_MASK			BIT(26)
#define USBRDT_VBFIL_LD_EN		BIT(25)
#define USBRDT_IDDIG_EN			24
#define USBRDT_IDDIG_REG		23
#define USBRDT_USBRDT_MSK		(0x7fffff)
#define USBRDT_USBRDT(x)		((x) & USBRDT_USBRDT_MSK)

/*USB VBUS Jitter Filter Register*/
#define USBVBFIL_USBVBFIL(x)	((x) & 0xffff)
#define USBVBFIL_IDDIGFIL(x)	((x) & (0xffff << 16))

/*USB Parameter Control Register1*/
#define USBPCR1_BVLD_REG		BIT(31)
#define USBPCR1_REFCLKSEL		(0x2 << 26)
#define USBPCR1_REFCLKDIV_MSK	(0x7 << 23)
#define USBPCR1_REFCLKDIV(x)	(((x) & 0x7) << 23)
#define USBPCR1_PORT_RST		BIT(21)
#define USBPCR1_WORD_IF_16BIT	BIT(19)

#define OPCR_SPENDN_BIT			7
#define OPCR_GATE_USBPHY_CLK_BIT	23

#define SRBC_USB_SR			12

enum {
	OTG_PHY = 0,
	HOST_PHY
};

struct ingenic_usb_phy {
	struct usb_phy phy;
	struct device *dev;
	unsigned long ext_rate;
	struct clk *clk;
	struct clk *gate_clk;
	struct regmap *regmap;

	bool is_otg_phy;

	/*otg phy*/
	enum usb_dr_mode dr_mode;
	struct gpio_desc	*gpiod_drvvbus;
	struct gpio_desc	*gpiod_id;
	struct gpio_desc	*gpiod_vbus;
#define USB_DETE_VBUS	0x1
#define USB_DETE_ID	0x2
	unsigned int usb_dete_state;
	struct delayed_work	work;
	u32 usbpcr;

	/*uhc phy*/
	unsigned int has_inited;
	u32 usbpcr1;
};

static inline int dwc_usbphy_read(struct usb_phy *x, u32 reg)
{
	struct ingenic_usb_phy *jzphy = container_of(x, struct ingenic_usb_phy, phy);
	u32 val = 0;
	regmap_read(jzphy->regmap, reg, &val);
	return val;
}

static inline int dwc_usbphy_write(struct usb_phy *x, u32 val, u32 reg)
{
	struct ingenic_usb_phy *jzphy = container_of(x, struct ingenic_usb_phy, phy);

	return regmap_write(jzphy->regmap, reg, val);
}

static inline int dwc_usbphy_update_bits(struct usb_phy *x, u32 reg, u32 mask, u32 bits)
{
	struct ingenic_usb_phy *jzphy = container_of(x, struct ingenic_usb_phy, phy);

	return regmap_update_bits_check(jzphy->regmap, reg, mask, bits, NULL);
}

static irqreturn_t usb_dete_irq_handler(int irq, void *data)
{
	struct ingenic_usb_phy *jzphy = (struct ingenic_usb_phy *)data;

	schedule_delayed_work(&jzphy->work, msecs_to_jiffies(100));
	return IRQ_HANDLED;
}

static void usb_dete_work(struct work_struct *work)
{
	struct ingenic_usb_phy *jzphy =
		container_of(work, struct ingenic_usb_phy, work.work);
	struct usb_otg		*otg = jzphy->phy.otg;
	int vbus = 0, id_level = 1, usb_state = 0;

	if (jzphy->gpiod_vbus)
		vbus = gpiod_get_value_cansleep(jzphy->gpiod_vbus);

	if (jzphy->gpiod_id)
		id_level = gpiod_get_value_cansleep(jzphy->gpiod_id);

	if (vbus && id_level)
		usb_state |= USB_DETE_VBUS;
	else
		usb_state &= ~USB_DETE_VBUS;

	if (id_level)
		usb_state |= USB_DETE_ID;
	else
		usb_state &= ~USB_DETE_ID;

	if (usb_state != jzphy->usb_dete_state) {
		enum usb_phy_events	status;
		if (!(usb_state & USB_DETE_VBUS)) {
			usb_gadget_vbus_disconnect(otg->gadget);
			status = USB_EVENT_NONE;
			otg->state = OTG_STATE_B_IDLE;
			jzphy->phy.last_event = status;
			if (otg->gadget)
				atomic_notifier_call_chain(&jzphy->phy.notifier, status,
					otg->gadget);
		}

		if (!(usb_state & USB_DETE_ID)) {
			status = USB_EVENT_ID;
			otg->state = OTG_STATE_A_IDLE;
			jzphy->phy.last_event = status;
			if (otg->host)
				atomic_notifier_call_chain(&jzphy->phy.notifier, status,
						otg->host);
		}

		if (usb_state & USB_DETE_VBUS) {
			status = USB_EVENT_VBUS;
			otg->state = OTG_STATE_B_PERIPHERAL;
			jzphy->phy.last_event = status;
			usb_gadget_vbus_connect(otg->gadget);
			if (otg->gadget)
				atomic_notifier_call_chain(&jzphy->phy.notifier, status,
						otg->gadget);
		}
		jzphy->usb_dete_state = usb_state;
	}
	return;
}

static int jzphy_set_suspend(struct usb_phy *x, int suspend);
static int jzphy_init(struct usb_phy *x)
{
	struct ingenic_usb_phy *jzphy = container_of(x, struct ingenic_usb_phy, phy);
	unsigned long rate;
	u32 usbpcr, usbpcr1;
	unsigned long usbclk_sel = 0;

	if (!IS_ERR_OR_NULL(jzphy->clk)) {
		switch (jzphy->ext_rate/1000000) {
			case 50:
				usbclk_sel += 2;
			case 24:
				usbclk_sel++;
			case 20:
				usbclk_sel++;
			case 19:
				usbclk_sel++;
			case 12:
				usbclk_sel++;
			case 10:
				usbclk_sel++;
			case 9:
				break;
			default:
				return;
		}
		if (rate != clk_get_rate(jzphy->clk))
			clk_set_rate(jzphy->clk, rate);
		clk_prepare_enable(jzphy->clk);
	}

	unsigned int aaa = *(volatile unsigned int *)0xb0000024;
	aaa &= ~(1 << 23);
	*(volatile unsigned int *)0xb0000024 = aaa;

	if (!IS_ERR_OR_NULL(jzphy->gate_clk))
		clk_prepare_enable(jzphy->gate_clk);

	jzphy_set_suspend(x, 0);

	/* fil */
	dwc_usbphy_write(x, 0, CPM_USBVBFIL);

	/* rdt */
	dwc_usbphy_write(x, USBRDT_USBRDT(0x96) | USBRDT_VBFIL_LD_EN, CPM_USBRDT);

	if (jzphy->is_otg_phy) {
		usbpcr = USBPCR_COMMONONN | USBPCR_VBUSVLDEXT | USBPCR_VBUSVLDEXTSEL |
			USBPCR_SQRXTUNE(7) | USBPCR_TXPREEMPHTUNE | USBPCR_TXHSXVTUNE(1) |
			USBPCR_TXVREFTUNE(7);
		usbpcr1 = dwc_usbphy_read(x, CPM_USBPCR1);
		usbpcr1 &= ~(USBPCR1_REFCLKDIV_MSK);
		usbpcr1 = USBPCR1_REFCLKSEL | USBPCR1_REFCLKDIV(usbclk_sel) | USBPCR1_WORD_IF_16BIT;
		if (jzphy->dr_mode == USB_DR_MODE_PERIPHERAL) {
			pr_info("DWC PHY IN DEVICE ONLY MODE\n");
			usbpcr1 |= USBPCR1_BVLD_REG;
			usbpcr |= USBPCR_OTG_DISABLE;
		} else {
			pr_info("DWC PHY IN OTG MODE\n");
			usbpcr |= USBPCR_USB_MODE;
		}
		dwc_usbphy_write(x, usbpcr1, CPM_USBPCR1);
	} else {
	}
	/**
	 * Power-On Reset(POR)
	 * Function:This customer-specific signal resets all test registers and state machines
	 * in the USB 2.0 nanoPHY.
	 * The POR signal must be asserted for a minimum of 10 ??s.
	 * For POR timing information:
	 *
	 * T0: Power-on reset (POR) is initiated. 0 (reference)
	 * T1: T1 indicates when POR can be set to 1???b0. (To provide examples, values for T2 and T3 are also shown
	 * where T1 = T0 + 30 ??s.); In general, T1 must be ??? T0 + 10 ??s. T0 + 10 ??s ??? T1
	 * T2: T2 indicates when PHYCLOCK, CLK48MOHCI, and CLK12MOHCI are available at the macro output, based on
	 * the USB 2.0 nanoPHY reference clock source.
	 * Crystal:
	      ??? When T1 = T0 + 10 ??s:
	        T2 < T1 + 805 ??s = T0 + 815 ??s
	      ??? When T1 = T0 + 30 ??s:
	        T2 < T1 + 805 ??s = T0 + 835 ??s
	* see "Reset and Power-Saving Signals" on page 60 an ???Powering Up and Powering Down the USB 2.0
	* nanoPHY??? on page 73.
	*/
	usbpcr |= USBPCR_POR;
	dwc_usbphy_write(x, usbpcr, CPM_USBPCR);
	mdelay(1);
	usbpcr &= ~USBPCR_POR;
	dwc_usbphy_write(x, usbpcr, CPM_USBPCR);
	mdelay(1);
	return 0;
}

static void jzphy_shutdown(struct usb_phy *x)
{

	struct ingenic_usb_phy *jzphy = container_of(x, struct ingenic_usb_phy, phy);

	jzphy_set_suspend(x, 1);

	if (jzphy->is_otg_phy)
		dwc_usbphy_update_bits(x, CPM_USBPCR,
				USBPCR_OTG_DISABLE|USBPCR_SIDDQ,
				USBPCR_OTG_DISABLE|USBPCR_SIDDQ);
	if (!IS_ERR_OR_NULL(jzphy->clk))
		clk_disable_unprepare(jzphy->clk);
	if (!IS_ERR_OR_NULL(jzphy->gate_clk))
		clk_disable_unprepare(jzphy->gate_clk);
}

static int jzphy_set_vbus(struct usb_phy *x, int on)
{
	struct ingenic_usb_phy *jzphy = container_of(x, struct ingenic_usb_phy, phy);

	printk("OTG VBUS %s\n", on ? "ON" : "OFF");
	gpiod_set_value(jzphy->gpiod_drvvbus, on);
	return 0;
}

static int jzphy_set_suspend(struct usb_phy *x, int suspend)
{
	struct ingenic_usb_phy *jzphy = container_of(x, struct ingenic_usb_phy, phy);
	int suspend_bit = jzphy->is_otg_phy ? OPCR_USB_SPENDN : OPCR_USB_SPENDN1;

	pr_debug("usb phy suspend %d suspend bit %x\n", suspend, suspend_bit);
	dwc_usbphy_update_bits(x, CPM_OPCR,
			suspend_bit,
			suspend ? ~suspend_bit : suspend_bit);
	udelay(suspend ? 5 : 45);
	return 0;
}

static int jzphy_set_wakeup(struct usb_phy *x, bool enabled)
{
	return 0;
}

static int jzphy_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	if (!otg)
		return -ENODEV;
	otg->host = host;
	return 0;
}

static int jzphy_set_peripheral(struct usb_otg *otg, struct usb_gadget *gadget)
{
	if (!otg)
		return -ENODEV;
	otg->gadget = gadget;
	return 0;
}

static const struct of_device_id of_matchs[];
static int usb_phy_ingenic_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct ingenic_usb_phy *jzphy;
	bool is_otg_phy = false;
	struct clk *clk;
	int ret;

	match = of_match_device(of_matchs, &pdev->dev);
	if (!match)
		return -ENODEV;

	if ((int)match->data == OTG_PHY)
		is_otg_phy = true;

	jzphy = (struct ingenic_usb_phy *)
		devm_kzalloc(&pdev->dev, sizeof(*jzphy), GFP_KERNEL);
	if (!jzphy)
		return -ENOMEM;

	jzphy->phy.init = jzphy_init;
	jzphy->phy.shutdown = jzphy_shutdown;
	jzphy->phy.dev = &pdev->dev;
	jzphy->is_otg_phy = is_otg_phy;
	jzphy->regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "ingenic,syscon");
	if (IS_ERR(jzphy->regmap)) {
		dev_err(&pdev->dev, "failed to find regmap for usb phy\n");
		return PTR_ERR(jzphy->regmap);
	}

	if (is_otg_phy) {
		if (of_find_property(pdev->dev.of_node, "dr_mode", NULL))
			jzphy->dr_mode = usb_get_dr_mode(&pdev->dev);
		else
			jzphy->dr_mode = USB_DR_MODE_HOST;

		jzphy->gpiod_id = devm_gpiod_get_optional(&pdev->dev,"ingenic,id-dete", GPIOD_IN);
		jzphy->gpiod_vbus = devm_gpiod_get_optional(&pdev->dev,"ingenic,vbus-dete", GPIOD_ASIS);
		if (jzphy->gpiod_id || jzphy->gpiod_vbus)
			INIT_DELAYED_WORK(&jzphy->work, usb_dete_work);

		if (!IS_ERR_OR_NULL(jzphy->gpiod_id)) {
			ret = devm_request_irq(&pdev->dev,
					gpiod_to_irq(jzphy->gpiod_id),
					usb_dete_irq_handler,
					IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
					"id_dete",
					(void *)jzphy);
			if (ret)
				return ret;
		} else {
			jzphy->usb_dete_state |= USB_DETE_ID;
			jzphy->gpiod_id = NULL;
		}

		if (!IS_ERR_OR_NULL(jzphy->gpiod_vbus)) {
			ret = devm_request_irq(&pdev->dev,
					gpiod_to_irq(jzphy->gpiod_vbus),
					usb_dete_irq_handler,
					IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
					"vbus_dete",
					(void *)jzphy);
			if (ret)
				return ret;
		} else {
			jzphy->usb_dete_state &= ~USB_DETE_VBUS;
			jzphy->gpiod_vbus = NULL;
		}

		jzphy->gpiod_drvvbus = devm_gpiod_get_optional(&pdev->dev,"ingenic,drvvbus", GPIOD_OUT_LOW);
		if (IS_ERR_OR_NULL(jzphy->gpiod_drvvbus))
			jzphy->gpiod_drvvbus = NULL;
		jzphy->phy.set_vbus = jzphy_set_vbus;
		jzphy->phy.set_suspend = jzphy_set_suspend;
		jzphy->phy.set_wakeup = jzphy_set_wakeup;
		jzphy->phy.otg = devm_kzalloc(&pdev->dev, sizeof(*jzphy->phy.otg),
				GFP_KERNEL);
		if (!jzphy->phy.otg)
			return -ENOMEM;

		jzphy->phy.otg->state		= OTG_STATE_UNDEFINED;
		jzphy->phy.otg->usb_phy		= &jzphy->phy;
		jzphy->phy.otg->set_host	= jzphy_set_host;
		jzphy->phy.otg->set_peripheral	= jzphy_set_peripheral;
	}

	jzphy->clk = devm_clk_get(&pdev->dev, "cgu_usb");
	if (IS_ERR_OR_NULL(jzphy->clk))
		return -ENODEV;

	jzphy->gate_clk = devm_clk_get(&pdev->dev, "gate_usbphy");
	if (IS_ERR_OR_NULL(jzphy->gate_clk))
		return -ENODEV;

	clk = clk_get(NULL, "ext");
	if (!IS_ERR_OR_NULL(clk)) {
		jzphy->ext_rate = clk_get_rate(clk);
		clk_put(clk);
	}

	ret = usb_add_phy_dev(&jzphy->phy);
	if (ret) {
		dev_err(&pdev->dev, "can't register transceiver, err: %d\n",
			ret);
		return ret;
	}
	platform_set_drvdata(pdev, jzphy);

	return 0;
}

static int usb_phy_ingenic_remove(struct platform_device *pdev)
{
	struct ingenic_usb_phy *jzphy = platform_get_drvdata(pdev);

	usb_remove_phy(&jzphy->phy);

	clk_disable_unprepare(jzphy->clk);
	return 0;
}

static const struct of_device_id of_matchs[] = {
	{ .compatible = "ingenic,otgphy", .data = (void *)OTG_PHY},
	{ .compatible = "ingenic,usbphy", .data = (void *)HOST_PHY},
	{ }
};
MODULE_DEVICE_TABLE(of, ingenic_xceiv_dt_ids);

static struct platform_driver usb_phy_ingenic_driver = {
	.probe		= usb_phy_ingenic_probe,
	.remove		= usb_phy_ingenic_remove,
	.driver		= {
		.name	= "usb_phy",
		.of_match_table = of_matchs,
	},
};

static int __init usb_phy_ingenic_init(void)
{
	return platform_driver_register(&usb_phy_ingenic_driver);
}
subsys_initcall(usb_phy_ingenic_init);

static void __exit usb_phy_ingenic_exit(void)
{
	platform_driver_unregister(&usb_phy_ingenic_driver);
}
module_exit(usb_phy_ingenic_exit);
