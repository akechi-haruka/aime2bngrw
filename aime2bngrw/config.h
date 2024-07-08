#pragma once

#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>

struct aime2bngrw_config {
    uint32_t port;
    bool high_baud;
    bool dev_reset_must_callback;
    bool enable;
    bool use_custom_led_flash;
};

void aime2bngrw_config_load(
        struct aime2bngrw_config *cfg,
        const char *filename);
