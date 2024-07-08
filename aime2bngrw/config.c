#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "aime2bngrw/util/dprintf.h"
#include "aime2bngrw/config.h"

void aime2bngrw_config_load(
        struct aime2bngrw_config *cfg,
        const char *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->port = GetPrivateProfileIntA("aime", "port", 3, filename);
    cfg->high_baud = GetPrivateProfileIntA("aime", "high_baud", 1, filename) != 0;
    cfg->enable = GetPrivateProfileIntA("bngrw", "enable", 1, filename) != 0;
    cfg->dev_reset_must_callback = GetPrivateProfileIntA("bngrw", "dev_reset_must_callback", 1, filename) != 0;
    cfg->use_custom_led_flash = GetPrivateProfileIntA("aime", "use_custom_led_flash", 1, filename) != 0;
}
