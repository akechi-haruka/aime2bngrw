#include <windows.h>

#include <stdbool.h>
#include <stdint.h>
#include <winuser.h>
#include <subprojects/segapi/api/api.h>

#include "aime2bngrw/aime2bngrw.h"
#include "aime2bngrw/config.h"
#include "aime2bngrw/bngrw.h"
#include "aime2bngrw/util/dprintf.h"

static struct aime2bngrw_config cfg;

#define MIN_API_VER 0x010101

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    dprintf("aime2bngrw: Initializing\n");

    if (api_get_version() <= MIN_API_VER){
        dprintf("aime2bngrw: API dll is outdated! At least v.%x is required, DLL is v.%x", MIN_API_VER, api_get_version());
        return FALSE;
    }

    aime2bngrw_config_load(&cfg, ".\\aime2bngrw.ini");

    bngrw_init(&cfg);

    api_init(".\\aime2bngrw.ini");

    dprintf("aime2bngrw: Loaded\n");

    return TRUE;
}
