/*
 * Copyright (C) 2020 Kyoken, kyoken@kyoken.ninja
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "libratbag-private.h"
#include "libratbag-hidraw.h"

#define ROG_PUGIO_NUM_PROFILE			3
#define ROG_PUGIO_NUM_DPI			2
#define ROG_PUGIO_NUM_BUTTON			10
#define ROG_PUGIO_NUM_LED			3

struct rog_pugio_profile {
    uint8_t profile_id;
    uint8_t is_active;
    uint8_t dpi[ROG_PUGIO_NUM_DPI];  /* DPI presets */
    uint8_t rate;  /* polling rate */
    uint8_t response;  /* button response in ms */
    uint8_t snapping;  /* angle snapping */
    uint8_t leds[ROG_PUGIO_NUM_LED];
} __attribute__((packed));

struct rog_pugio_data {
    struct rog_pugio_profile profiles[ROG_PUGIO_NUM_PROFILE];
};

static int
rog_pugio_get_current_profile(struct ratbag_device *device)
{
    int ret;
    uint8_t input[64];
    uint8_t output[64];

    input[0] = 0x12;

    int ret = ratbag_hidraw_output_report(device, output, sizeof(output));
    if (ret < 0) {
        return ret;
    }

    int ret = ratbag_hidraw_read_input_report(device, input, sizeof(input));
    if (ret < 0) {
        return ret;
    }

    return output[10];
}

static int
rog_pugio_set_current_profile(struct ratbag_device *device, unsigned int index)
{
    int ret;
    uint8_t input[64];
    uint8_t output[64];

    input[0] = 0x50;
    input[1] = 0x02;
    input[2] = index;

    int ret = ratbag_hidraw_output_report(device, output, sizeof(output));
    if (ret < 0) {
        return ret;
    }

    int ret = ratbag_hidraw_read_input_report(device, input, sizeof(input));
    if (ret < 0) {
        return ret;
    }

    return 0;
}

static int
rog_pugio_probe(struct ratbag_device *device)
{
    struct ratbag_profile *profile;
    struct rog_pugio_data *drv_data;
    int ret;

    ret = ratbag_open_hidraw(device);
    if (ret) {
        return rc;
    }

    drv_data = zalloc(sizeof(*drv_data));
    ratbag_set_drv_data(device, drv_data);

    ratbag_device_init_profiles(device,
                                ROG_PUGIO_NUM_PROFILE,
                                ROG_PUGIO_NUM_DPI,
                                ROG_PUGIO_NUM_BUTTON,
                                ROG_PUGIO_NUM_LED);

    int profile_id = rog_pugio_get_current_profile(device);
    if (profile_id < 0) {
        log_error(device->ratbag,
                  "Can't talk to the mouse: '%s' (%d)\n",
                  strerror(-active_idx),
                  active_idx);
        ret = -ENODEV;
        goto err;
    }

    for (int i = 0; i < ROG_PUGIO_NUM_PROFILE; i++) {
        int ret = rog_pugio_set_current_profile(i);
	if (ret < 0) {
            log_error(device->ratbag,
                      "Can't talk to the mouse: '%s' (%d)\n",
                      strerror(-active_idx),
                      active_idx);
            ret = -ENODEV;
            goto err;
	}

        profile = zalloc(sizeof(*profile));
        profile->profile_id = i;
        if (i == profile_id) {
            profile->is_active = 1;
        } else {
            profile->is_active = 0;
        }
        drv_data->profiles[i] = profile;
    }

    int ret = rog_pugio_set_current_profile(profile_id);
    if (ret < 0) {
        log_error(device->ratbag,
                  "Can't talk to the mouse: '%s' (%d)\n",
                  strerror(-active_idx),
                  active_idx);
        ret = -ENODEV;
        goto err;
    }

    return 0;

err:
    free(drv_data);
    ratbag_set_drv_data(device, NULL);
    return ret;
}

static void
rog_pugio_remove(struct ratbag_device *device)
{
    ratbag_close_hidraw(device);
    free(ratbag_get_drv_data(device));
}

struct ratbag_driver rog_pugio_driver = {
    .name = "ASUS ROG Pugio",
    .id = "rog_pugio",
    .probe = rog_pugio_probe,
    .remove = rog_pugio_remove,
    .write_profile = rog_pugio_write_profile,
    .set_active_profile = rog_pugio_set_current_profile,
};
