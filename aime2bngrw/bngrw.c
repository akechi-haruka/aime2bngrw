#include <windows.h>

#include <stdint.h>
#include <subprojects/segapi/api/api.h>
#include <subprojects/aimelib/aime.h>

#include "config.h"
#include "util/dprintf.h"
#include "bngrw.h"

static struct aime2bngrw_config* bngrw_cfg;
static PCRDATA cr_operation;
static bool is_scanning = false;
static bool initialized = false;
static bool connected = false;
static bool api_polling = false;

DWORD WINAPI api_read_signal_thread(void* data) {
    while (initialized){
        uint8_t* rgb = api_get_aime_rgb_and_clear();
        if (rgb != NULL){
            aime_led_set(rgb[0], rgb[1], rgb[2]);
        }
        if (api_get_card_switch_state()){
            bool read = api_get_card_reading_state_and_clear_switch_state();
            dprintf("BNGRW: API card switch state to: %d\n", read);
            if (read) {
                aime_clear_card();
            }
            aime_set_polling(read);
            api_polling = read;
        }
        if (api_polling && aime_get_card_type() != CARD_TYPE_NONE) {
            uint8_t type = aime_get_card_type();
            if (type == CARD_TYPE_FELICA) {
                api_send(PACKET_25_CARD_FELICA, aime_get_card_len(), (const uint8_t*)aime_get_card_id());
            } else if (type == CARD_TYPE_MIFARE) {
                api_send(PACKET_26_CARD_AIME, aime_get_card_len(), (const uint8_t*)aime_get_card_id());
            }
            api_polling = false;
        }
        Sleep(500);
    }
    return 1;
}

HRESULT bngrw_init(struct aime2bngrw_config* cfg)
{

    if (initialized){
        return S_FALSE;
    }

    if (!cfg->enable){
        return S_FALSE;
    }

    bngrw_cfg = cfg;

    dprintf("BNGRW: initialized\n");

    initialized = true;

    if (CreateThread(NULL, 0, api_read_signal_thread, 0, 0, NULL) == NULL){
        dprintf("BNGRW: API thread start fail: %ld", GetLastError());
        return E_FAIL;
    }

    return S_OK;
}

// used
_stdcall int BngRwAttach(int a1, char *Str2, int a3, int a4, BngRwCallback func, void* arg){
	dprintf("BNGRW: BngRwAttach\n");

    aime_close();

    HRESULT hr = aime_connect(bngrw_cfg->port, bngrw_cfg->high_baud ? 115200 : 38400, bngrw_cfg->use_custom_led_flash);
    if (!SUCCEEDED(hr)){
        dprintf("BNGRW: Aime connect failed: %lx\n", hr);
        return BNGRW_E_OPENDEVICE;
    }

    hr = aime_reset();
    if (!SUCCEEDED(hr)){
        dprintf("BNGRW: Aime reset failed: %lx\n", hr);
        return BNGRW_E_OPENDEVICE;
    }

    hr = aime_led_reset();
    if (!SUCCEEDED(hr)){
        dprintf("BNGRW: Aime LED reset failed: %lx\n", hr);
        return BNGRW_E_OPENDEVICE;
    }

    const uint32_t max_strinfo = 64;
    char strinfo[max_strinfo];
    uint32_t strinfo_len = max_strinfo;

    hr = aime_get_hw_version(strinfo, &strinfo_len);
    if (!SUCCEEDED(hr)){
        dprintf("Aime Reader: Aime get HW failed: %lx", hr);
        return BNGRW_E_OPENDEVICE;
    }
    dprintf("Aime Reader: HW Version: %.*s\n", strinfo_len, strinfo);

    strinfo_len = max_strinfo;
    hr = aime_get_fw_version(strinfo, &strinfo_len);
    if (!SUCCEEDED(hr)){
        dprintf("Aime Reader: Aime get FW failed: %lx\n", hr);
        return BNGRW_E_OPENDEVICE;
    }
    if (strinfo_len == 1){
        dprintf("Aime Reader: FW Version (>=gen2): %x\n", strinfo[0]);
    } else {
        dprintf("Aime Reader: FW Version: %.*s\n", strinfo_len, strinfo);
    }

    strinfo_len = max_strinfo;
    hr = aime_get_led_info(strinfo, &strinfo_len);
    if (!SUCCEEDED(hr)){
        dprintf("Aime Reader: Aime get LED Info failed: %lx\n", hr);
        return BNGRW_E_OPENDEVICE;
    }
    dprintf("Aime Reader: LED Info: %.*s\n", strinfo_len, strinfo);

    strinfo_len = max_strinfo;
    hr = aime_get_led_hw_version(strinfo, &strinfo_len);
    if (!SUCCEEDED(hr)){
        dprintf("Aime Reader: Aime get LED HW failed: %lx\n", hr);
        return BNGRW_E_OPENDEVICE;
    }
    dprintf("Aime Reader: LED HW Version: %.*s\n", strinfo_len, strinfo);

    const uint8_t k1[6] = { 0x57, 0x43, 0x43, 0x46, 0x76, 0x32 };
    const uint8_t k2[6] = { 0x60, 0x90, 0xd0, 0x06, 0x32, 0xf5 };

    hr = aime_set_mifare_key_sega(k1, 6);
    if (!SUCCEEDED(hr)){
        dprintf("BNGRW: Set MIFARE Key 1 failed: %lx\n", hr);
        return BNGRW_E_OPENDEVICE;
    }

    hr = aime_set_mifare_key_namco(k2, 6);
    if (!SUCCEEDED(hr)){
        dprintf("BNGRW: Set MIFARE Key 2 failed: %lx\n", hr);
        return BNGRW_E_OPENDEVICE;
    }

    connected = true;

	if (func != NULL){
        func(0, 0, arg);
	}
	return BNGRW_S_ACK;
}

// used
_stdcall int BngRwDevReset(int a1, BngRwCallback func, void* arg){
	dprintf("BNGRW: BngRwDevReset\n");

    if (!initialized){
        return BNGRW_E_NOINIT;
    }

	if (func != NULL && bngrw_cfg->dev_reset_must_callback){
	    dprintf("BNGRW: callback function\n");
        func(0, 0, arg);
        return BNGRW_S_ACK;
    }
	return BNGRW_S_OK;
}

_stdcall int BngRwExReadMifareAllBlock(int a1, int a2){
	dprintf("BNGRW: BngRwExReadMifareAllBlock (stub)\n");
	return BNGRW_E_UNSUPPORTED;
}

// used
_stdcall int BngRwFin(){
	dprintf("BNGRW: BngRwFin\n");

    connected = false;

    aime_close();

	return BNGRW_S_OK;
}

_stdcall int BngRwGetFwVersion(int a1){
	dprintf("BNGRW: BngRwGetFwVersion (stub)\n");
	return BNGRW_E_UNSUPPORTED;
}

_stdcall int BngRwGetStationID(int a1){
	dprintf("BNGRW: BngRwGetStationID (stub)\n");
	return BNGRW_E_UNSUPPORTED;
}

_stdcall int BngRwGetTotalRetryCount(int a1){
	dprintf("BNGRW: BngRwGetTotalRetryCount (stub)\n");
	return BNGRW_E_UNSUPPORTED;
}

const char* BngRwGetVersion(){
	return "Ver 1.6.0";
}

// used
_stdcall int BngRwInit(){

    if (!initialized){
        return BNGRW_E_NOINIT;
    }

    dprintf("BNGRW: BngRwInit\n");

	return BNGRW_S_OK;
}

// used
_stdcall int BngRwIsCmdExec(int a1){
	if (a1 != 0){
		//dprintf("BNGRW: BngRwIsCmdExec: %d\n", a1);
		if (a1 == 1 && is_scanning){
		    return BNGRW_S_ACK;
		}
	}
	return BNGRW_S_OK;
}

_stdcall int BngRwReqAction(int a1, int a2, int a3, int a4){
	dprintf("BNGRW: BngRwReqAction (stub)\n");
	return BNGRW_E_UNSUPPORTED;
}

_stdcall int BngRwReqAiccAuth(unsigned int a1, signed int a2, int a3, DWORD *a4, int a5, int a6, int *a7){
	dprintf("BNGRW: BngRwReqAiccAuth (stub)\n");
    return BNGRW_E_UNSUPPORTED;
}

// used
_stdcall int BngRwReqBeep(int a1, int beep_type, BngRwCallback func, void* arg){
	dprintf("BNGRW: BngRwReqBeep: %d, %d, %p, %p\n", a1, beep_type, func, arg);

    if (beep_type == BNGRW_BEEPTYPE_TEST){
        Beep(750, 1000);
    } else if (beep_type == BNGRW_BEEPTYPE_OK){
        Beep(1500, 250);
    } else if (beep_type == BNGRW_BEEPTYPE_NG){
        Beep(500, 250);
    } else if (beep_type != BNGRW_BEEPTYPE_OFF){
        dprintf("BNGRW: unimpl beeptype\n");
    }

    if (func != NULL){
        func(0, 3, 0);
    }

	return BNGRW_S_ACK;
}

// used
_stdcall int BngRwReqCancel(int a1){
	dprintf("BNGRW: BngRwReqCancel\n");
    if (cr_operation != NULL){
	    dprintf("BNGRW: stopping reader\n");
        cr_operation->timeout = 5;
    }

    HRESULT hr = aime_set_polling(false);
    if (!SUCCEEDED(hr)){
        return BNGRW_E_TIMEOUTDEVICE;
    }
    return BNGRW_S_ACK;
}

_stdcall int BngRwReqFwCleanup(int a1, int a2, int a3){
	dprintf("BNGRW: BngRwReqFwCleanup (stub)\n");
    return BNGRW_E_UNSUPPORTED;
}

_stdcall int BngRwReqFwVersionup(int a1, int a2, int a3, int a4){
	dprintf("BNGRW: BngRwReqFwVersionup (stub)\n");
    return BNGRW_E_UNSUPPORTED;
}

_stdcall int BngRwReqLatchID(int a1, int a2, int a3){
	dprintf("BNGRW: BngRwReqLatchID\n");
    return BNGRW_E_UNSUPPORTED;
}

// used
_stdcall int BngRwReqLed(unsigned int a1, unsigned int led_type, BngRwCallback func, void* arg){
	dprintf("BNGRW: BngRwReqLed: %d, %d, %p, %p\n", a1, led_type, func, arg);

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    if (led_type == BNGRW_LEDTYPE_TEST){
        r = g = b = 255;
    } else if (led_type == BNGRW_LEDTYPE_GREEN){
        g = 255;
    } else if (led_type == BNGRW_LEDTYPE_BLUE || led_type == BNGRW_LEDTYPE_BLUE_FULL){
        b = 255;
    } else if (led_type == BNGRW_LEDTYPE_BLUE_HIGH){
        b = 191;
    } else if (led_type == BNGRW_LEDTYPE_BLUE_MID){
        b = 128;
    } else if (led_type == BNGRW_LEDTYPE_BLUE_LOW){
        b = 63;
    } else if (led_type == BNGRW_LEDTYPE_RED){
        r = 255;
    } else if (led_type == BNGRW_LEDTYPE_RED_BLINK){
        r = 255;
        dprintf("BNGRW: led type red blink unimpl\n");
    }

    HRESULT hr = aime_led_set(r, g, b);
    if (!SUCCEEDED(hr)){
        return BNGRW_E_TIMEOUT;
    }

    /*if (func != NULL){
        func(0, 0, arg);
    }*/

	return BNGRW_S_ACK;
}

_stdcall int BngRwReqSendMailTo(int a1, int a2, int a3, int a4, void *Src, void *a6, void *a7, void *a8, int a9, int a10){
	dprintf("BNGRW: BngRwReqSendMailTo\n");
    return BNGRW_E_UNSUPPORTED;
}

// used
_stdcall int BngRwReqSendUrlTo(int a1, int a2, int a3, int a4, void *Src, void *a6, int a7, int a8){
	dprintf("BNGRW: BngRwReqSendUrlTo\n");
    return BNGRW_E_UNSUPPORTED;
}

void tohex(const char * in, const size_t insz, char * out, size_t outsz)
{
    const char * pin = in;
    const char * hex = "0123456789ABCDEF";
    char * pout = out;
    for(; pin < in+insz; pout +=2, pin++){
        pout[0] = hex[(*pin>>4) & 0xF];
        pout[1] = hex[ *pin     & 0xF];
        if (pout - out > outsz){
            /* Better to truncate output string than overflow buffer */
            /* it would be still better to either return a status */
            /* or ensure the target buffer is large enough and it never happen */
            break;
        }
    }
}

DWORD WINAPI BngRwReqWaitTouchThread(void* data) {
	PCRDATA arg = (PCRDATA)data;
	if (arg->on_scan != NULL){
		dprintf("BNGRW: scanning (with timeout %d)...\n", arg->timeout);
		while (arg->timeout == -1 || arg->timeout --> 0){
			if (aime_get_card_type() != CARD_TYPE_NONE){
				dprintf("BNGRW: detected card\n");

                const int max_card_len = 24;

                int card_type = aime_get_card_type();
                char bngrw_card_id[max_card_len];
                memset(bngrw_card_id, 0, max_card_len);
                int card_id_len = aime_get_card_len();
                dprintf("BNGRW: aime card id len=%d\n", card_id_len);
                tohex(aime_get_card_id(), card_id_len, bngrw_card_id, max_card_len);
                int bngrw_card_type = BNGRW_CARDTYPE_UNKNOWN;
                int bngrw_id_type = BNGRW_IDTYPE_UNKNOWN;

                if (card_type == CARD_TYPE_MIFARE){
                    bngrw_card_type = BNGRW_CARDTYPE_MIFARE1K;
                    bngrw_id_type = BNGRW_IDTYPE_SEGA;
                } else if (card_type == CARD_TYPE_FELICA){
                    bngrw_card_type = BNGRW_CARDTYPE_FELICALITE;
                    bngrw_id_type = BNGRW_IDTYPE_SEGA;
                } else if (card_type == CARD_TYPE_ILLEGAL){
                    bngrw_card_type = BNGRW_CARDTYPE_UNKNOWN;
                    bngrw_id_type = BNGRW_IDTYPE_UNKNOWN;
                }

                dprintf("BNGRW: card id: %s (%d, %d)\n", bngrw_card_id, bngrw_card_type, bngrw_id_type);

                arg->response.hCardHandle.eCardType = bngrw_card_type;
                arg->response.hCardHandle.iIdLen = card_id_len;
                arg->response.hCardHandle.iFelicaOS = card_type == CARD_TYPE_FELICA;
                strcpy((char*)arg->response.hCardHandle.ucChipId, "0AAAAAAAAAAAAAAA");
                memcpy((char*)arg->response.ucChipId, aime_get_card_id(), aime_get_card_len());
                memcpy((char*)arg->response.strChipId, aime_get_card_id(), aime_get_card_len());
                memcpy((char*)arg->response.strAccessCode, bngrw_card_id, max_card_len);
                arg->response.eIdType = bngrw_id_type;
                arg->response.eCardType = bngrw_card_type;
                arg->response.eCarrierType = BNGRW_MCARRIER_UNKNOWN;
                arg->response.iFelicaOS = card_type == CARD_TYPE_FELICA;
                strcpy((char*)arg->response.strBngProductId, "12345678");
                arg->response.uiBngId = 123456789;
                arg->response.usBngTypeCode = 1;
                arg->response.ucBngRegionCode = 1;
                strcpy((char*)arg->response.ucBlock1, "0AAAAAAAAAAAAAAA");
                strcpy((char*)arg->response.ucBlock2, "0AAAAAAAAAAAAAAA");

				dprintf("BNGRW: sending to game\n");
				arg->on_scan(0, 0, (void*)&(arg->response), arg->card_reader);
				is_scanning = false;
                api_block_card_reader(false);
				return BNGRW_S_OK;
			}
			Sleep(10);
		}
		dprintf("BNGRW: timed out (%d)\n", arg->timeout);
		arg->on_scan(0, 3, (void*)&(arg->response), arg->card_reader);
	} else {
		dprintf("BNGRW: no scan function\n");
	}
    api_block_card_reader(false);
	is_scanning = false;
    return BNGRW_S_OK;
}


// used
_stdcall int BngRwReqWaitTouch(int a1, int timeout, int a3, BngRwReqCallback on_scan, void* a5){
	dprintf("BNGRW: BngRwReqWaitTouch: %d, %d, %x, %p, %p\n", a1, timeout, a3, on_scan, a5);

    api_block_card_reader(true);
    api_polling = false;

    HRESULT hr = aime_set_polling(true);
    if (!SUCCEEDED(hr)){
        return BNGRW_E_TIMEOUTDEVICE;
    }

	PCRDATA arg = (PCRDATA) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CRDATA));
	cr_operation = arg;
	arg->timeout = timeout;
	arg->on_scan = on_scan;
	arg->card_reader = a5;
	if (CreateThread(NULL, 0, BngRwReqWaitTouchThread, arg, 0, NULL) == NULL){
		dprintf("BNGRW: thread start fail");
	} else {
	    is_scanning = true;
	}
	return BNGRW_S_ACK;
}

_stdcall int BngRwSetLedPower(int a1, int a2, int a3){
	dprintf("BNGRW: BngRwSetLedPower (stub)\n");
    return BNGRW_E_UNSUPPORTED;
}

