#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/phy.h> 

struct gpio_desc *phy;

static int ar8031_phy_fixup(struct phy_device *dev)
{
	u16 val;

             /* To enable AR8031 output a 125MHz clk from CLK_25M */
        phy_write(dev, 0xd, 0x7);
        phy_write(dev, 0xe, 0x8016);
        phy_write(dev, 0xd, 0x4007);

        val = phy_read(dev, 0xe);
        val &= 0xffe3;
        val |= 0x18;
        phy_write(dev, 0xe, val);

        phy_write(dev, 0x1d, 0x5);
        val = phy_read(dev, 0x1e);
        val |= 0x0100;
        phy_write(dev, 0x1e, val);
	return 0;
}

static int ar8035_phy_fixup(struct phy_device *dev)
{
        u16 val;

        /* Ar803x phy SmartEEE feature cause link status generates glitch,
         * which cause ethernet link down/up issue, so disable SmartEEE
         */
        phy_write(dev, 0xd, 0x3);
        phy_write(dev, 0xe, 0x805d);
        phy_write(dev, 0xd, 0x4003);

        val = phy_read(dev, 0xe);
        phy_write(dev, 0xe, val & ~(1 << 8));

        /*
         * Enable 125MHz clock from CLK_25M on the AR8031.  This
         * is fed in to the IMX6 on the ENET_REF_CLK (V22) pad.
         * Also, introduce a tx clock delay.
         *
         * This is the same as is the AR8031 fixup.
         */
        ar8031_phy_fixup(dev);

        /*check phy power*/
        val = phy_read(dev, 0x0);
        if (val & BMCR_PDOWN)
                phy_write(dev, 0x0, val & ~BMCR_PDOWN);

        return 0;
}
#define PHY_ID_AR8035 0x004dd072

static int gpio_init_probe(struct platform_device *pdev)
{
   int ret = 0;

   printk("phy init\n");

   phy = devm_gpiod_get_optional(&pdev->dev, "reset", GPIOD_OUT_HIGH);
   if (IS_ERR(phy)) {
                ret = PTR_ERR(phy);
                //printk("cannot phy %d\n", ret);
                return ret;
   }
	 
   //gpiod_set_value(phy, 0);
   //msleep(100);
   gpiod_set_value(phy, 1);
   
   
   phy_register_fixup_for_uid(PHY_ID_AR8035, 0xffffffef, ar8035_phy_fixup);

   return(0);
}

static int gpio_exit_remove(struct platform_device *pdev)
{
   printk("GPIO phy exit\n");
   
   return(0);
}

/* this structure does the matching with the device tree */
/* if it does not match the compatible field of DT, nothing happens */
static struct of_device_id myir_match[] = {
    {.compatible = "myir,ar8035"},
    {.compatible = "st,dummy"},
    {/* end node */}
};

static struct platform_driver myir_driver = {
    .probe = gpio_init_probe,
    .remove = gpio_exit_remove,
    .driver = {
        .name = "myir_driver",
                .owner = THIS_MODULE,
                .of_match_table = myir_match,
    }
};
 
module_platform_driver(myir_driver);

MODULE_AUTHOR("licy");
MODULE_DESCRIPTION("Gpio phy init");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:myir_driver");
