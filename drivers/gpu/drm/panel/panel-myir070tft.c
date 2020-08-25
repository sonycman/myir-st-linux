// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 myir
 * Author: licy
 *
 * Based on Panel Simple driver
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>

#include <video/display_timing.h>
#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_panel.h>


struct myir_panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;
	const struct display_timing *timings;
	unsigned int num_timings;

	unsigned int bpc;

	/**
	 * @width: width (in millimeters) of the panel's active display area
	 * @height: height (in millimeters) of the panel's active display area
	 */
	struct {
		unsigned int width;
		unsigned int height;
	} size;

	u32 bus_format;
	u32 bus_flags;
};

struct myir_panel {
	struct drm_panel base;
	bool prepared;
	bool enabled;
	const struct myir_panel_desc *desc;
	struct backlight_device *backlight;
	struct regulator *dvdd;
	struct regulator *avdd;
	struct gpio_desc *enable_gpio;
        struct gpio_desc *reset_gpio;
 
};

static inline struct myir_panel *to_myir_panel(struct drm_panel *panel)
{
	return container_of(panel, struct myir_panel, base);
}

static int myir_panel_get_fixed_modes(struct myir_panel *panel)
{
	struct drm_connector *connector = panel->base.connector;
	struct drm_device *drm = panel->base.drm;
	struct drm_display_mode *mode;
	unsigned int i, num = 0;

	if (!panel->desc)
		return 0;

	for (i = 0; i < panel->desc->num_timings; i++) {
		const struct display_timing *dt = &panel->desc->timings[i];
		struct videomode vm;

		videomode_from_timing(dt, &vm);
		mode = drm_mode_create(drm);
		if (!mode) {
			dev_err(drm->dev, "failed to add mode %ux%u\n",
				dt->hactive.typ, dt->vactive.typ);
			continue;
		}

		drm_display_mode_from_videomode(&vm, mode);

		mode->type |= DRM_MODE_TYPE_DRIVER;

		if (panel->desc->num_timings == 1)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_probed_add(connector, mode);
		num++;
	}

	for (i = 0; i < panel->desc->num_modes; i++) {
		const struct drm_display_mode *m = &panel->desc->modes[i];

		mode = drm_mode_duplicate(drm, m);
		if (!mode) {
			dev_err(drm->dev, "failed to add mode %ux%u@%u\n",
				m->hdisplay, m->vdisplay, m->vrefresh);
			continue;
		}

		mode->type |= DRM_MODE_TYPE_DRIVER;

		if (panel->desc->num_modes == 1)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_set_name(mode);

		drm_mode_probed_add(connector, mode);
		num++;
	}

	connector->display_info.bpc = panel->desc->bpc;
	connector->display_info.width_mm = panel->desc->size.width;
	connector->display_info.height_mm = panel->desc->size.height;
	if (panel->desc->bus_format)
		drm_display_info_set_bus_formats(&connector->display_info,
						 &panel->desc->bus_format, 1);
	connector->display_info.bus_flags = panel->desc->bus_flags;

	return num;
}

static int myir_panel_disable(struct drm_panel *panel)
{
	struct myir_panel *p = to_myir_panel(panel);

	if (!p->enabled)
		return 0;

	if (p->backlight) {
		p->backlight->props.power = FB_BLANK_POWERDOWN;
		p->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(p->backlight);
	}

	p->enabled = false;

	return 0;
}

static int myir_panel_unprepare(struct drm_panel *panel)
{
	struct myir_panel *p = to_myir_panel(panel);

	if (!p->prepared)
		return 0;

	regulator_disable(p->avdd);

	/* Add a 100ms delay as per the panel datasheet */
	msleep(100);

	regulator_disable(p->dvdd);
	gpiod_set_value(p->enable_gpio, 0);

        gpiod_set_value(p->reset_gpio, 0);

	p->prepared = false;

	return 0;
}

static int myir_panel_prepare(struct drm_panel *panel)
{
	struct myir_panel *p = to_myir_panel(panel);
	int err;

	if (p->prepared)
		return 0;
/*
	err = regulator_enable(p->dvdd);
	if (err < 0) {
		dev_err(panel->dev, "failed to enable dvdd: %d\n", err);
		return err;
	}
*/
	/* Add a 100ms delay as per the panel datasheet */
	msleep(10);
/*
	err = regulator_enable(p->avdd);
	if (err < 0) {
		dev_err(panel->dev, "failed to enable avdd: %d\n", err);
		goto disable_dvdd;
	}
*/
	gpiod_set_value(p->enable_gpio, 1);
        usleep_range(10, 20);

        gpiod_set_value(p->reset_gpio, 1);
        usleep_range(10, 20);
	p->prepared = true;

	return 0;

disable_dvdd:
	regulator_disable(p->dvdd);
	return err;
}

static int myir_panel_enable(struct drm_panel *panel)
{
	struct myir_panel *p = to_myir_panel(panel);

	if (p->enabled)
		return 0;

	if (p->backlight) {
		p->backlight->props.state &= ~BL_CORE_FBBLANK;
		p->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(p->backlight);
	}

	p->enabled = true;

	return 0;
}

static int myir_panel_get_modes(struct drm_panel *panel)
{
	struct myir_panel *p = to_myir_panel(panel);

	/* add hard-coded panel modes */
	return myir_panel_get_fixed_modes(p);
}

static int myir_panel_get_timings(struct drm_panel *panel,
				    unsigned int num_timings,
				    struct display_timing *timings)
{
	struct myir_panel *p = to_myir_panel(panel);
	unsigned int i;

	if (p->desc->num_timings < num_timings)
		num_timings = p->desc->num_timings;

	if (timings)
		for (i = 0; i < num_timings; i++)
			timings[i] = p->desc->timings[i];

	return p->desc->num_timings;
}

static const struct drm_panel_funcs myir_panel_funcs = {
	.disable = myir_panel_disable,
	.unprepare = myir_panel_unprepare,
	.prepare = myir_panel_prepare,
	.enable = myir_panel_enable,
	.get_modes = myir_panel_get_modes,
	.get_timings = myir_panel_get_timings,
};

static int myir_panel_probe(struct device *dev,
					const struct myir_panel_desc *desc)
{
	struct device_node *backlight;
	struct myir_panel *panel;
	int err,ret;

	panel = devm_kzalloc(dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	panel->enabled = false;
	panel->prepared = false;
	panel->desc = desc;


	panel->enable_gpio = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
        if (IS_ERR(panel->enable_gpio)) {
                ret = PTR_ERR(panel->enable_gpio);
                dev_err(dev, "cannot get enable-gpio %d\n", ret);
                return ret;
        }


        panel->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
        if (IS_ERR(panel->reset_gpio)) {
                ret = PTR_ERR(panel->reset_gpio);
                dev_err(dev, "cannot get reset-gpios %d\n", ret);
                return ret;
        }
/*
	panel->dvdd = devm_regulator_get(dev, "dvdd");
	if (IS_ERR(panel->dvdd))
		return PTR_ERR(panel->dvdd);

	panel->avdd = devm_regulator_get(dev, "avdd");
	if (IS_ERR(panel->avdd))
		return PTR_ERR(panel->avdd);

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		panel->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!panel->backlight)
			return -EPROBE_DEFER;
	}
*/
	drm_panel_init(&panel->base);
	panel->base.dev = dev;
	panel->base.funcs = &myir_panel_funcs;
    
   
	err = drm_panel_add(&panel->base);
	if (err < 0){
		return err;
    }


	dev_set_drvdata(dev, panel);

	return 0;
}

static int myir_panel_remove(struct platform_device *pdev)
{
	struct myir_panel *panel = dev_get_drvdata(&pdev->dev);

	drm_panel_remove(&panel->base);

	myir_panel_disable(&panel->base);

	if (panel->backlight)
		put_device(&panel->backlight->dev);

	return 0;
}

static void myir_panel_shutdown(struct platform_device *pdev)
{
	struct myir_panel *panel = dev_get_drvdata(&pdev->dev);

	myir_panel_disable(&panel->base);
}

static const struct display_timing myir_070tft_timing = {
/*	.pixelclock = { 33300000, 33300000, 33300000 },
	.hactive = { 800, 800, 800 },
	.hfront_porch = {  210, 210, 210 },
	.hback_porch = { 46, 46, 46 },
	.hsync_len = { 40, 40, 40 },

	.vactive = { 480, 480, 480 },
	.vfront_porch = { 22, 22, 22 },
	.vback_porch = { 23, 23, 23 },
	.vsync_len = { 1, 1, 1 },
*/

   //myir ya157c
	.pixelclock = {33000000,33000000,33000000},
	.hactive = {800,800,800},
	.hfront_porch = {210,210,210},
	.hback_porch = {46,46,46},
	.hsync_len = {1,1,1},
	.vactive = {480,480,480},
	.vfront_porch = {23,23,23},
	.vback_porch = {22,22,22},
	.vsync_len = {20,20,20},
	//.flags = DISPLAY_FLAGS_DE_LOW,


};

static const struct myir_panel_desc myir_070tft = {
	.timings = &myir_070tft_timing,
	.num_timings = 1,
	.bpc = 6,
	.size = {
		.width = 154,
		.height = 86,

	},
	.bus_format = MEDIA_BUS_FMT_RGB888_1X24,
	//.bus_flags = DRM_BUS_FLAG_DE_HIGH | DRM_BUS_FLAG_PIXDATA_POSEDGE,

};

static const struct of_device_id platform_of_match[] = {
	{
		.compatible = "myir,070tft",
		.data = &myir_070tft,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, platform_of_match);

static int myir_panel_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;

	id = of_match_node(platform_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	return myir_panel_probe(&pdev->dev, id->data);
}

static struct platform_driver myir_panel_platform_driver = {
	.driver = {
		.name = "myir_panel",
		.of_match_table = platform_of_match,
	},
	.probe = myir_panel_platform_probe,
	.remove = myir_panel_remove,
	.shutdown = myir_panel_shutdown,
};
module_platform_driver(myir_panel_platform_driver);

MODULE_AUTHOR("licy");
MODULE_DESCRIPTION("myir 070-tft panel driver");
MODULE_LICENSE("GPL v2");











