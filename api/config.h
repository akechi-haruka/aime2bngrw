#pragma once

#include <stddef.h>
#include <stdint.h>

struct api_config {
    uint8_t enable;
	uint8_t groupId;
    uint8_t deviceId;
    uint8_t log;
    uint16_t port;
	char bindAddr[16];
};

void api_config_load(
        struct api_config *cfg,
        const char *filename);
