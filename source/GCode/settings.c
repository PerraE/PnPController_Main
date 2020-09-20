/*
 * settings.c
 *
 *  Created on: 13 sep. 2020
 *      Author: perra
 */

#include "PnPContoller_Main.h"

//settings_t settings;

const settings_t defaults = {

    .limits.flags.hard_enabled = DEFAULT_HARD_LIMIT_ENABLE,
    .limits.flags.soft_enabled = DEFAULT_SOFT_LIMIT_ENABLE,
    .limits.flags.check_at_init = DEFAULT_CHECK_LIMITS_AT_INIT,
    .limits.invert.mask = INVERT_LIMIT_PIN_MASK,
    .limits.disable_pullup.mask = DISABLE_LIMIT_PINS_PULL_UP_MASK
};
