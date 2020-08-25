// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Maxime Coquelin 2015
 * Copyright (C) STMicroelectronics 2017
 * Author:  Maxime Coquelin <mcoquelin.stm32@gmail.com>
 */

#include <linux/kernel.h>
#include <asm/mach/arch.h>
#ifdef CONFIG_ARM_SINGLE_ARMV7M
#include <asm/v7m.h>
#endif
#include <linux/phy.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/delay.h>


static const char *const stm32_compat[] __initconst = {
	"st,stm32f429",
	"st,stm32f469",
	"st,stm32f746",
	"st,stm32f769",
	"st,stm32h743",
	"st,stm32mp151",
	"st,stm32mp153",
	"st,stm32mp157",
	NULL
};

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
	phy_register_fixup_for_uid(PHY_ID_AR8035, 0xffffffef, ar8035_phy_fixup);

}

static void __init stm32mp1_init_machine(void)
{
    stm32mp1_enet_phy_init();
}


DT_MACHINE_START(STM32DT, "STM32 (Device Tree Support)")
	.dt_compat = stm32_compat,
#ifdef CONFIG_ARM_SINGLE_ARMV7M
	.restart = armv7m_restart,
#endif
	.init_machine = stm32mp1_init_machine,
MACHINE_END
