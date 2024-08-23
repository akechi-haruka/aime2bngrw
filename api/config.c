#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "api/config.h"


void api_config_load(
        struct api_config *cfg,
        const char *filename)
{

    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntA("api", "enable", 0, filename);
	GetPrivateProfileStringA(
            "api",
            "bindAddr",
            "255.255.255.255",
            cfg->bindAddr,
            _countof(cfg->bindAddr),
            filename);
    cfg->log = GetPrivateProfileIntA("api", "log", 1, filename);
    cfg->port = GetPrivateProfileIntA("api", "port", 5364, filename);
    cfg->groupId = GetPrivateProfileIntA("api", "group_id", 1, filename);
    cfg->deviceId = GetPrivateProfileIntA("api", "device_id", 1, filename);

}
