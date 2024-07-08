#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

#define CARD_ID_LEN 12

enum {
    AIME_CMD_RESET = 0x62,
    AIME_CMD_GET_FW_VERSION = 0x30,
    AIME_CMD_GET_HW_VERSION = 0x32,
    AIME_CMD_RADIO_ON = 0x40,
    AIME_CMD_RADIO_OFF = 0x41,
    AIME_CMD_POLL = 0x42,
    AIME_CMD_LED_SET_CHANNEL = 0x80,
    AIME_CMD_LED_SET_COLOR = 0x81,
    AIME_CMD_LED_GET_INFO = 0xF0,
    AIME_CMD_LED_HW_VERSION = 0xF1,
    AIME_CMD_LED_RESET = 0xF5,
    AIME_CMD_MIFARE_READ_BLOCK = 0x52,
    AIME_CMD_MIFARE_SET_KEY_SEGA = 0x54,
    AIME_CMD_MIFARE_SET_KEY_NAMCO = 0x50,
    AIME_CMD_MIFARE_SELECT = 0x43,
    AIME_CMD_MIFARE_AUTHENTICATE = 0x55
};

enum {
    POLLING_MODE_MIFARE = 0x01,
    POLLING_MODE_FELICA = 0x02,
    POLLING_MODE_BOTH = 0x03,
};

enum {
    CARD_TYPE_NONE = 0x00,
    CARD_TYPE_MIFARE = 0x10,
    CARD_TYPE_FELICA = 0x20,
    CARD_TYPE_ILLEGAL = 0xFF,
};

struct __attribute__((__packed__)) aime_req_any {
    uint8_t packet_len;
    uint8_t addr;
    uint8_t seq;
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[250];
};

struct __attribute__((__packed__)) aime_resp_any {
    uint8_t packet_len;
    uint8_t addr;
    uint8_t seq;
    uint8_t cmd;
    uint8_t status;
    uint8_t len;
    uint8_t payload[249];
};

HRESULT aime_connect(uint32_t port, int baud, bool use_custom_led_flash);
HRESULT aime_close();
HRESULT aime_reset();
HRESULT aime_led_reset();
HRESULT aime_get_fw_version(char* out, uint32_t* len);
HRESULT aime_get_hw_version(char* out, uint32_t* len);
HRESULT aime_get_led_hw_version(char* out, uint32_t* len);
HRESULT aime_get_led_info(char* out, uint32_t* len);
HRESULT aime_set_polling(bool on);
HRESULT aime_poll();
const char* aime_get_card_id();
uint8_t aime_get_card_len();
uint8_t aime_get_card_type();
HRESULT aime_led_set(uint8_t r, uint8_t g, uint8_t b);
HRESULT aime_set_mifare_key_sega(const uint8_t* key, uint8_t len);
HRESULT aime_set_mifare_key_namco(const uint8_t* key, uint8_t len);
HRESULT aime_mifare_select(uint32_t uid);
HRESULT aime_mifare_authenticate(uint32_t uid, uint8_t unknown);
HRESULT aime_mifare_read_block(uint32_t uid, uint8_t block, uint8_t* block_contents, uint8_t* block_len);