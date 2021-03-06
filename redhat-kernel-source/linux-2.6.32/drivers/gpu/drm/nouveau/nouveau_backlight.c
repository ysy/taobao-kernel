/*
 * Copyright (C) 2009 Red Hat <mjg@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * Authors:
 *  Matthew Garrett <mjg@redhat.com>
 *
 * Register locations derived from NVClock by Roderick Colenbrander
 */

#include <linux/backlight.h>
#include <linux/acpi.h>

#include "drmP.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"
#include "nouveau_reg.h"
#include "nouveau_encoder.h"

static int
nv40_get_intensity(struct backlight_device *bd)
{
	struct drm_device *dev = bl_get_data(bd);
	int val = (nv_rd32(dev, NV40_PMC_BACKLIGHT) & NV40_PMC_BACKLIGHT_MASK)
									>> 16;

	return val;
}

static int
nv40_set_intensity(struct backlight_device *bd)
{
	struct drm_device *dev = bl_get_data(bd);
	int val = bd->props.brightness;
	int reg = nv_rd32(dev, NV40_PMC_BACKLIGHT);

	nv_wr32(dev, NV40_PMC_BACKLIGHT,
		 (val << 16) | (reg & ~NV40_PMC_BACKLIGHT_MASK));

	return 0;
}

static const struct backlight_ops nv40_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.get_brightness = nv40_get_intensity,
	.update_status = nv40_set_intensity,
};

static int
nv50_get_intensity(struct backlight_device *bd)
{
	struct nouveau_encoder *nv_encoder = bl_get_data(bd);
	struct drm_device *dev = nv_encoder->base.base.dev;
	int or = nv_encoder->or;
	u32 div = 1025;
	u32 val;

	val  = nv_rd32(dev, NV50_PDISP_SOR_PWM_CTL(or));
	val &= NV50_PDISP_SOR_PWM_CTL_VAL;
	return ((val * 100) + (div / 2)) / div;
}

static int
nv50_set_intensity(struct backlight_device *bd)
{
	struct nouveau_encoder *nv_encoder = bl_get_data(bd);
	struct drm_device *dev = nv_encoder->base.base.dev;
	int or = nv_encoder->or;
	u32 div = 1025;
	u32 val = (bd->props.brightness * div) / 100;

	nv_wr32(dev, NV50_PDISP_SOR_PWM_CTL(or),
		     NV50_PDISP_SOR_PWM_CTL_NEW | val);
	return 0;
}

static const struct backlight_ops nv50_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.get_brightness = nv50_get_intensity,
	.update_status = nv50_set_intensity,
};

static int
nva3_get_intensity(struct backlight_device *bd)
{
	struct nouveau_encoder *nv_encoder = bl_get_data(bd);
	struct drm_device *dev = nv_encoder->base.base.dev;
	int or = nv_encoder->or;
	u32 div, val;

	div  = nv_rd32(dev, NV50_PDISP_SOR_PWM_DIV(or));
	val  = nv_rd32(dev, NV50_PDISP_SOR_PWM_CTL(or));
	val &= NVA3_PDISP_SOR_PWM_CTL_VAL;
	if (div && div >= val)
		return ((val * 100) + (div / 2)) / div;

	return 100;
}

static int
nva3_set_intensity(struct backlight_device *bd)
{
	struct nouveau_encoder *nv_encoder = bl_get_data(bd);
	struct drm_device *dev = nv_encoder->base.base.dev;
	int or = nv_encoder->or;
	u32 div, val;

	div = nv_rd32(dev, NV50_PDISP_SOR_PWM_DIV(or));
	val = (bd->props.brightness * div) / 100;
	if (div) {
		nv_wr32(dev, NV50_PDISP_SOR_PWM_CTL(or), val |
			     NV50_PDISP_SOR_PWM_CTL_NEW |
			     NVA3_PDISP_SOR_PWM_CTL_UNK);
		return 0;
	}

	return -EINVAL;
}

static const struct backlight_ops nva3_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.get_brightness = nva3_get_intensity,
	.update_status = nva3_set_intensity,
};

int
nouveau_backlight_init(struct drm_device *dev)
{
#ifdef CONFIG_ACPI
	if (acpi_video_backlight_support()) {
		NV_INFO(dev, "ACPI backlight interface available, "
			     "not registering our own\n");
		return 0;
	}
#endif

	return 0;
}

void
nouveau_backlight_exit(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	if (dev_priv->backlight) {
		backlight_device_unregister(dev_priv->backlight);
		dev_priv->backlight = NULL;
	}
}
