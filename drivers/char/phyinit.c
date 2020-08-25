#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <asm/mach/arch.h>
#include <linux/phy.h> 
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>

#define DEVICE_NAME "phy_dev"
int ar8035_misc_dev_open(struct inode *inode, struct file *filp)
{
	printk("misc dev open!\n");
	return 0 ;
}
int ar8035_misc_dev_close(struct inode *inode, struct file *filp)
{
	printk("misc dev close!\n");
	return 0 ;
}

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

static void __init stm32mp1_enet_phy_init(void)
{
    	static struct device_node *nphy = NULL;
   	int gpio;
    	enum of_gpio_flags flags;

	nphy = of_find_compatible_node(NULL,NULL,"myir,ar8035");
	if (NULL != nphy)
	{
           	gpio = of_get_named_gpio_flags(nphy, "phy-gpio", 0, &flags);
            	if(!gpio_is_valid(gpio)){
                    printk("the phy gpio failed\n");
                    
            	}else{   
                	if(gpio_request(gpio, "phy-gpio")){
                    		printk("can not request gpio\n");
                	}else{
                    		gpio_direction_output(gpio, 1);                                                        
                	}
			gpio_free(gpio);
            	}
	 }

	phy_register_fixup_for_uid(PHY_ID_AR8035, 0xffffffef, ar8035_phy_fixup);
}

struct file_operations ar8035_file_ops = {
	.owner = THIS_MODULE ,
	.open = ar8035_misc_dev_open,
	.release = ar8035_misc_dev_close,
};
struct miscdevice ar8035_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR ,
	.name = DEVICE_NAME ,
	.fops = &ar8035_file_ops,	
};


static int __init ar8035_misc_dev_init(void) 
{
	int ret_error ;
	int ret = misc_register(&ar8035_misc_dev);
	if(ret != 0){
	   ret_error = ret ;
	   printk("misc register fair!\n");
	   goto fair ;
	}
	printk("ar8035 phy init\n");
    	stm32mp1_enet_phy_init();

	return ret ;
fair:
	return ret_error ;
}

static void __exit ar8035_misc_dev_exit(void) 
{
	misc_deregister(&ar8035_misc_dev);
}

module_init(ar8035_misc_dev_init);
module_exit(ar8035_misc_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("licy add misc driver");
