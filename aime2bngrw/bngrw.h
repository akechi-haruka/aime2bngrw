#pragma once
#include <stdint.h>

#include "config.h"

enum BngRwMCarrier
{
    BNGRW_MCARRIER_UNKNOWN = 0,
    BNGRW_MCARRIER_DOCOMO = 256,
    BNGRW_MCARRIER_DOCOMO_ANDROID = 257,
    BNGRW_MCARRIER_AU = 512,
    BNGRW_MCARRIER_SOFTBANK = 768,
    BNGRW_MCARRIER_WILLCOM = 1024,
};

enum BngRwCardType
{
    BNGRW_CARDTYPE_UNKNOWN = 0,
    BNGRW_CARDTYPE_MIFAREUNKNOWN = 256,
    BNGRW_CARDTYPE_MIFARE1K = 257,
    BNGRW_CARDTYPE_MIFAREMINI = 258,
    BNGRW_CARDTYPE_FELICAUNKNOWN = 512,
    BNGRW_CARDTYPE_FELICALITE = 513,
    BNGRW_CARDTYPE_FELICASTD = 514,
    BNGRW_CARDTYPE_FELICAPLUG = 515,
    BNGRW_CARDTYPE_MFELICA = 528,
};

enum BngRwIdType
{
    BNGRW_IDTYPE_UNKNOWN = 0,
    BNGRW_IDTYPE_BNG = 256,
    BNGRW_IDTYPE_BNG_AUTH = 257,
    BNGRW_IDTYPE_BNG_OTHER = 258,
    BNGRW_IDTYPE_SEGA = 512,
    BNGRW_IDTYPE_TITO = 768,
    BNGRW_IDTYPE_MOBILE = 2048,
};

// banapass reader card object
struct BngRwCardHandle
{
  /* 0x0000 */ enum BngRwCardType eCardType;
  /* 0x0004 */ int iIdLen;
  /* 0x0008 */ int iFelicaOS;
  /* 0x000c */ unsigned char ucChipId[16];
}; /* size: 0x001c */

// banapass reader read action result object
struct BngRwResWaitTouch
{
  /* 0x0000 */ struct BngRwCardHandle hCardHandle;
  /* 0x001c */ unsigned char ucChipId[16];
  /* 0x002c */ char strChipId[36];
  /* 0x0050 */ char strAccessCode[24];
  /* 0x0068 */ enum BngRwIdType eIdType;
  /* 0x006c */ enum BngRwCardType eCardType;
  /* 0x0070 */ enum BngRwMCarrier eCarrierType;
  /* 0x0074 */ int iFelicaOS;
  /* 0x0078 */ char strBngProductId[8];
  /* 0x0080 */ unsigned int uiBngId;
  /* 0x0084 */ unsigned short usBngTypeCode;
  /* 0x0086 */ unsigned char ucBngRegionCode;
  /* 0x0087 */ unsigned char ucBlock1[16];
  /* 0x0097 */ unsigned char ucBlock2[16];
  /* 0x00a7 */ char __PADDING__[1];
}; /* size: 0x00a8 */

// callback function when reader finishes
typedef void (*BngRwReqCallback)(int iDev, int response, struct BngRwResWaitTouch *result, void *arg);
typedef __cdecl void* (*BngRwCallback)(int iDev, int response, void *arg);

// segatools data object for read operation
typedef struct CardReaderData {
	int timeout;
	BngRwReqCallback on_scan;
	struct BngRwResWaitTouch response;
	void* card_reader; // don't care
} CRDATA, *PCRDATA;

enum BngRwStat
{
    BNGRW_S_OK = 0,
    BNGRW_S_ACK = 1,
    BNGRW_S_EXEC = 2,
    BNGRW_S_CANCEL = 3,
    BNGRW_S_TOUCHWAIT = 10,
    BNGRW_S_TOUCH = 11,
    BNGRW_S_TAKEOFF = 12,
    BNGRW_E_NACK = -1,
    BNGRW_E_BUSY = -2,
    BNGRW_E_INVALIDARG = -100,
    BNGRW_E_NOINIT = -101,
    BNGRW_E_TIMEOUT = -200,
    BNGRW_E_LESSINFO = -201,
    BNGRW_E_OPENDEVICE = -300,
    BNGRW_E_TIMEOUTDEVICE = -301,
    BNGRW_E_UNSUPPORTED = -400,
    BNGRW_E_OTHERCARD = -401,
    BNGRW_E_OTHER = -900,
    BNGRW_E_KEY = -901,
};

enum BngRwLedType
{
    BNGRW_LEDTYPE_OFF = 0,
    BNGRW_LEDTYPE_RED = 1,
    BNGRW_LEDTYPE_GREEN = 2,
    BNGRW_LEDTYPE_BLUE = 3,
    BNGRW_LEDTYPE_TEST = 4,
    BNGRW_LEDTYPE_BLUE_FULL = 5,
    BNGRW_LEDTYPE_BLUE_HIGH = 6,
    BNGRW_LEDTYPE_BLUE_MID = 7,
    BNGRW_LEDTYPE_BLUE_LOW = 8,
    BNGRW_LEDTYPE_RED_BLINK = 9,
    BNGRW_LEDTYPE_END = 10,
};

enum BngRwBeepType
{
    BNGRW_BEEPTYPE_OFF = 0,
    BNGRW_BEEPTYPE_OK = 1,
    BNGRW_BEEPTYPE_NG = 2,
    BNGRW_BEEPTYPE_WARNING = 3,
    BNGRW_BEEPTYPE_WARNING2 = 4,
    BNGRW_BEEPTYPE_TEST = 5,
    BNGRW_BEEPTYPE_END = 6,
};

HRESULT bngrw_init(struct aime2bngrw_config* cfg);

_stdcall int BngRwAttach(int a1, char *Str2, int a3, int a4, BngRwCallback func, void* a6);
_stdcall int BngRwDevReset(int a1, BngRwCallback a2, void* a3);
_stdcall int BngRwExReadMifareAllBlock(int a1, int a2);
_stdcall int BngRwFin();
_stdcall int BngRwGetFwVersion(int a1);
_stdcall int BngRwGetStationID(int a1);
_stdcall int BngRwGetTotalRetryCount(int a1);
const char* BngRwGetVersion();
_stdcall int BngRwInit();
_stdcall int BngRwIsCmdExec(int a1);
_stdcall int BngRwReqAction(int a1, int a2, int a3, int a4);
_stdcall int BngRwReqAiccAuth(unsigned int a1, signed int a2, int a3, DWORD *a4, int a5, int a6, int *a7);
_stdcall int BngRwReqBeep(int a1, int a2, BngRwCallback a3, void* a4);
_stdcall int BngRwReqCancel(int a1);
_stdcall int BngRwReqFwCleanup(int a1, int a2, int a3);
_stdcall int BngRwReqFwVersionup(int a1, int a2, int a3, int a4);
_stdcall int BngRwReqLatchID(int a1, int a2, int a3);
_stdcall int BngRwReqLed(unsigned int a1, unsigned int led_type, BngRwCallback a3, void* arg);
_stdcall int BngRwReqSendMailTo(int a1, int a2, int a3, int a4, void *Src, void *a6, void *a7, void *a8, int a9, int a10);
_stdcall int BngRwReqSendUrlTo(int a1, int a2, int a3, int a4, void *Src, void *a6, int a7, int a8);
_stdcall int BngRwReqWaitTouch(int a1, int a2, int a3, BngRwReqCallback a4, void* a5);
_stdcall int BngRwSetLedPower(int a1, int a2, int a3);