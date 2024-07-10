#include <windows.h>

#include <stdbool.h>
#include <stdint.h>
#include <winuser.h>
#include <api/api.h>

#include "aime2bngrw/aime2bngrw.h"
#include "aime2bngrw/config.h"
#include "aime2bngrw/bngrw.h"
#include "aime2bngrw/util/dprintf.h"

static struct aime2bngrw_config cfg;

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    dprintf("aime2bngrw: Initializing\n");

    aime2bngrw_config_load(&cfg, ".\\aime2bngrw.ini");

    bngrw_init(&cfg);

    api_init();

    dprintf("aime2bngrw: Loaded\n");

    return TRUE;
}
