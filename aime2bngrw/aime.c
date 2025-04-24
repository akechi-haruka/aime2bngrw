#include "aime.h"
#include "util/dprintf.h"
#include "util/dump.h"
#include <windows.h>

#include <stdint.h>
#include <stdbool.h>
#include <subprojects/segapi/api/api.h>
#include <stdio.h>

#define NAME "Aime Reader"
#define SUPER_VERBOSE 1

#define CHECK_OFFSET_BOUNDARY(pos, max) \
if (pos >= max){ \
    return E_NOT_SUFFICIENT_BUFFER; \
}
#define COMM_BUF_SIZE 255
#define CARD_BUF_LEN 32

#define CHECKSUM_IS_ERROR 0
#define TRY_SALVAGE_COMM_SYNC 1

static HANDLE hSerial = NULL;
static HANDLE hThread = NULL;
static bool is_polling = false;
static char last_card_id[CARD_BUF_LEN];
static uint8_t last_card_type = 0;
static uint8_t last_card_len = 0;
static uint8_t aime_seq = 0;
static bool aime_use_led_flash = false;
static bool aime_led_flash = false;
static bool is_polling_cancel_request = false;

HRESULT aime_connect(uint32_t port, int baud, bool use_custom_led_flash) {

    if (hSerial != NULL){
        return S_FALSE;
    }

    aime_use_led_flash = use_custom_led_flash;

    dprintf(NAME ": Connect COM%d@%d\n", port, baud);

    char portname[16];
    sprintf(portname, "COM%d", port);

    hSerial = CreateFile(
            portname,
            GENERIC_READ | GENERIC_WRITE,
            0,
            0,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            0);
    if (hSerial == INVALID_HANDLE_VALUE) {
        uint32_t er = GetLastError();
        dprintf(NAME ": Failed to connect: %u\n", er);
        return HRESULT_FROM_WIN32(er);
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        uint32_t er = GetLastError();
        dprintf(NAME ": Failed to get port parameters: %u\n", er);
        hSerial = INVALID_HANDLE_VALUE;
        return HRESULT_FROM_WIN32(er);
    }
    dcbSerialParams.BaudRate = baud;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.fDtrControl = DTR_CONTROL_DISABLE;
    dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        uint32_t er = GetLastError();
        dprintf(NAME ": Failed to set port parameters: %u\n", er);
        hSerial = INVALID_HANDLE_VALUE;
        return HRESULT_FROM_WIN32(er);
    }

    COMMTIMEOUTS cto;
    GetCommTimeouts(hSerial,&cto);
    cto.ReadIntervalTimeout = 0;
    cto.ReadTotalTimeoutConstant = 1000;
    cto.ReadTotalTimeoutMultiplier = 0;
    cto.WriteTotalTimeoutConstant = 1000;
    cto.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(hSerial,&cto);

    dprintf(NAME ": Successfully opened port\n");

    aime_seq = 0;

    return aime_reset();
}

HRESULT aime_close(){

    is_polling = false;

    aime_set_polling(false);
    aime_led_set(0, 0, 0);

    CloseHandle(hSerial);
    hSerial = NULL;
    memset(last_card_id, 0, CARD_BUF_LEN);
    last_card_type = CARD_TYPE_NONE;
    last_card_len = 0;

    return S_OK;
}

HRESULT aime_encode(const uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t *outlen){
    if (in == NULL || out == NULL || outlen == NULL){
        return E_HANDLE;
    }
    if (inlen < 2){
        return E_INVALIDARG;
    }
    if (*outlen < inlen + 2){
        return E_NOT_SUFFICIENT_BUFFER;
    }

    uint8_t checksum = 0;
    uint32_t offset = 0;

    out[offset++] = 0xE0;
    for (int i = 0; i < inlen; i++){

        uint8_t byte = in[i];

        if (byte == 0xE0 || byte == 0xD0) {
            CHECK_OFFSET_BOUNDARY(offset+2, *outlen)
            out[offset++] = 0xD0;
            out[offset++] = byte - 1;
        } else {
            CHECK_OFFSET_BOUNDARY(offset+1, *outlen)
            out[offset++] = byte;
        }

        checksum += byte;
    }
    CHECK_OFFSET_BOUNDARY(offset+1, *outlen)
    out[offset++] = checksum;
    *outlen = offset;

    return S_OK;
}

HRESULT serial_read_single_byte(HANDLE handle, uint8_t* ptr){
    DWORD read = 0;
    if (!ReadFile(handle, ptr, 1, &read, NULL)){
        uint32_t er = GetLastError();
        dprintf(NAME ": Failed to read from serial port: %u\n", er);
        return HRESULT_FROM_WIN32(er);
    }
    if (read == 0){
        dprintf(NAME ": Stream was empty\n");
        return HRESULT_FROM_WIN32(E_FAIL);
    }
    return S_OK;
}

HRESULT aime_decoding_read(HANDLE handle, uint8_t *out, uint32_t *outlen){
    if (handle == NULL || out == NULL || outlen == NULL){
        return E_HANDLE;
    }

    const uint32_t len_byte_offset = 1;
    uint8_t checksum = 0;
    uint32_t offset = 0;
    int bytes_left = COMM_BUF_SIZE;
    HRESULT hr;
    bool escape_flag = false;

    do {
        hr = serial_read_single_byte(handle, out + offset);
        if (FAILED(hr)){
            return hr;
        }

        uint8_t byte = *(out + offset);

        if (offset == len_byte_offset){
            bytes_left = byte - 1;
        }

        if (offset == 0){
            if (byte != 0xE0){
#if TRY_SALVAGE_COMM_SYNC
                dprintf(NAME ": WARNING! Garbage on line: %x\n", byte);
                continue;
#else
                dprintf(NAME ": Failed to read from serial port: aime decode failed: Sync failure: %x\n", byte);
                return HRESULT_FROM_WIN32(ERROR_DATA_CHECKSUM_ERROR);
#endif
            }
            offset++;
        } else if (byte == 0xD0){
            escape_flag = true;
        } else {
            if (escape_flag) {
                byte += 1;
                escape_flag = false;
                bytes_left++;
            } else if (byte == 0xE0){
                dprintf(NAME ": Failed to read from serial port: aime decode failed: Found unexpected sync byte in stream at pos %d\n", offset);
                return HRESULT_FROM_WIN32(ERROR_DATA_CHECKSUM_ERROR);
            }
            checksum += byte;
            offset++;
        }
    } while (bytes_left-- > 0);

    hr = serial_read_single_byte(handle, out + offset);
    if (FAILED(hr)){
        return hr;
    }

    uint8_t schecksum = *(out + offset);

    if (checksum != schecksum){
#if CHECKSUM_IS_ERROR
        dprintf(NAME ": Failed to read from serial port: aime decode failed: Checksum failed: expected %d, got %d\n", checksum, schecksum);
        return HRESULT_FROM_WIN32(ERROR_DATA_CHECKSUM_ERROR);
#else
        dprintf(NAME ": Decode: WARNING! Checksum mismatch: expected %d, got %d\n", checksum, schecksum);
#endif
    }

#if SUPER_VERBOSE
    dprintf(NAME": Data received from serial (%d):\n", offset);
    dump(out, offset);
#endif

    // strip sync byte from response
    *outlen = offset - 1;
    memcpy(out, out + 1, *outlen);

    return S_OK;
}

HRESULT aime_transact(uint16_t packet, const uint8_t *payload, uint32_t payload_len, uint8_t *response, uint32_t *response_len) {

    dprintf(NAME ": Transact: %d (len=%d)\n", packet, payload_len);

    if (hSerial == INVALID_HANDLE_VALUE){
        return E_HANDLE;
    }

    uint32_t comm_buf_size;
    HRESULT hr;
    int attempt = 1;

    do {
        if (attempt > 1){
            dprintf(NAME ": RETRY! #%d\n", attempt);
        }
        if (payload != NULL && payload_len > 0) {
#if SUPER_VERBOSE
            dprintf("SEND (%d):\n", payload_len);
            dump(payload, payload_len);
#endif

            comm_buf_size = COMM_BUF_SIZE;
            uint8_t comm_out[COMM_BUF_SIZE];

            hr = aime_encode(payload, payload_len, comm_out, &comm_buf_size);
            if (FAILED(hr)) {
                dprintf(NAME ": Failed to encode packet: %ld\n", hr);
                return hr;
            }

            DWORD send_written = 0;
            if (!WriteFile(hSerial, comm_out, comm_buf_size, &send_written, NULL)) {
                uint32_t er = GetLastError();
                dprintf(NAME ": Failed to write to serial port: %u\n", er);
                return HRESULT_FROM_WIN32(er);
            }

#if SUPER_VERBOSE
            dprintf(NAME ": Send OK (%ld/%d)\n", send_written, comm_buf_size);
#endif
        }

        if (response != NULL && response_len != NULL) {
            hr = aime_decoding_read(hSerial, response, response_len);
        } else {
            hr = S_OK;
        }
    } while (!SUCCEEDED(hr) && attempt++ < 5);

    if (response != NULL && response_len != NULL) {
#if SUPER_VERBOSE
        dprintf("RECV (%d):\n", *response_len);
        dump(response, *response_len);
#endif
    }

    return hr;
}

HRESULT aime_transact_packet(struct aime_req_any* req, struct aime_resp_any* resp) {
    if (hSerial == INVALID_HANDLE_VALUE){
        return E_HANDLE;
    }

    HRESULT hr;

    uint32_t resp_len = sizeof(struct aime_resp_any);

    if (req->packet_len == 0){
        req->packet_len = req->len + 5;
    }
    req->seq = aime_seq++;

    hr = aime_transact(req->cmd, (uint8_t*) req, req->packet_len, (uint8_t *) resp, &resp_len);

    if (FAILED(hr)){
        dprintf(NAME ": transact_packet failed, transact failed: %ld\n", hr);
        return hr;
    }

    return hr;
}

HRESULT aime_reset(){
    dprintf(NAME ": Reset\n");

    struct aime_req_any req = {0};
    struct aime_resp_any resp = {0};
    req.cmd = AIME_CMD_RESET;
    req.len = 0;

    HRESULT hr = aime_transact_packet(&req, &resp);

    dprintf(NAME ": Reset: %d\n", resp.status);

    return hr;
}

HRESULT aime_led_reset(){
    dprintf(NAME ": LED Reset\n");

    struct aime_req_any req = {0};
    struct aime_resp_any resp = {0};
    req.cmd = AIME_CMD_LED_RESET;
    req.len = 0;

    HRESULT hr = aime_transact_packet(&req, &resp);

    dprintf(NAME ": LED Reset: %d\n", resp.status);

    return hr;
}

HRESULT aime_get_string_packet(int packet, char* out, uint32_t* len){

    struct aime_req_any req = {0};
    struct aime_resp_any resp = {0};
    req.cmd = packet;
    req.len = 0;

    HRESULT hr = aime_transact_packet(&req, &resp);

    if (resp.status == 0 && resp.len > 0){

        if (resp.len > *len){
            return E_NOT_SUFFICIENT_BUFFER;
        }

        *len = resp.len;

        memcpy(out, resp.payload, resp.len);

    }

    return hr;
}

HRESULT aime_get_fw_version(char* out, uint32_t* len){
    return aime_get_string_packet(AIME_CMD_GET_FW_VERSION, out, len);
}

HRESULT aime_get_hw_version(char* out, uint32_t* len){
    return aime_get_string_packet(AIME_CMD_GET_HW_VERSION, out, len);
}

HRESULT aime_get_led_hw_version(char* out, uint32_t* len){
    return aime_get_string_packet(AIME_CMD_LED_HW_VERSION, out, len);
}

HRESULT aime_get_led_info(char* out, uint32_t* len){
    return aime_get_string_packet(AIME_CMD_LED_GET_INFO, out, len);
}

DWORD WINAPI polling_thread(void* data) {
    dprintf(NAME ": Card Polling Thread started\n");
    while (is_polling && !is_polling_cancel_request){

        if (FAILED(aime_poll())){
            dprintf(NAME ": ERROR: Card polling failed!\n");
            return 1;
        }

        if (last_card_type != CARD_TYPE_NONE){

            if (last_card_type == CARD_TYPE_MIFARE){
                api_send(PACKET_26_CARD_AIME, aime_get_card_len(), (uint8_t*)aime_get_card_id());
            } else if (last_card_type == CARD_TYPE_FELICA){
                api_send(PACKET_25_CARD_FELICA, aime_get_card_len(), (uint8_t*)aime_get_card_id());
            }

            if (aime_use_led_flash){
                if (last_card_type != CARD_TYPE_ILLEGAL) {
                    aime_led_set(0, 0, 255);
                } else {
                    aime_led_set(255, 0, 0);
                }
                Sleep(1000);
                aime_led_set(0, 0, 0);
                aime_led_flash = false;
            }
            break;
        }

        if (aime_use_led_flash){
            aime_led_flash = !aime_led_flash;
            if (aime_led_flash) {
                aime_led_set(255, 255, 255);
            } else {
                aime_led_set(0, 0, 0);
            }
        }
        Sleep(1000);
    }

    dprintf(NAME ": Card Polling Thread stopped\n");
    is_polling_cancel_request = false;
    hThread = INVALID_HANDLE_VALUE;
    return 0;
}

HRESULT aime_set_polling(bool on){

    dprintf(NAME ": Set Polling (%d)\n", on);

    if (!on && hThread != INVALID_HANDLE_VALUE){
        dprintf(NAME ": Waiting for thread termination\n");
        is_polling_cancel_request = true;
        WaitForSingleObject(hThread, INFINITE);
        dprintf(NAME ": Thread terminated\n");

        hThread = INVALID_HANDLE_VALUE;
    }

    struct aime_req_any req = {0};
    struct aime_resp_any resp = {0};
    req.cmd = on ? AIME_CMD_RADIO_ON : AIME_CMD_RADIO_OFF;
    req.len = 1;
    req.payload[0] = POLLING_MODE_BOTH;

    HRESULT hr = aime_transact_packet(&req, &resp);

    if (!SUCCEEDED(hr)){
        return hr;
    }

    dprintf(NAME ": Set Radio: %d\n", resp.status);

    is_polling = on;
    last_card_type = CARD_TYPE_NONE;
    last_card_len = 0;

    if (is_polling && hThread == INVALID_HANDLE_VALUE){
        hThread = CreateThread(NULL, 0, polling_thread, NULL, 0, NULL);

        if (hThread == INVALID_HANDLE_VALUE){
            DWORD er = GetLastError();
            dprintf(NAME ": Thread failed: %lu\n", er);
            return HRESULT_FROM_WIN32(er);
        }
    }

    return S_OK;
}

HRESULT aime_poll(){

    struct aime_req_any req = {0};
    struct aime_resp_any resp = {0};
    req.cmd = AIME_CMD_POLL;
    req.len = 0;

    HRESULT hr = aime_transact_packet(&req, &resp);

    if (!SUCCEEDED(hr)){
        return hr;
    }

    if (resp.len > 0){
        int offset = 0;
        uint8_t* data = resp.payload;

        uint8_t count = data[offset++];
        for (int i = 0; i < count; i++) {
            uint8_t type = data[offset++];
            uint8_t size = data[offset++];

            memset(last_card_id, 0, CARD_BUF_LEN);

            if (type == CARD_TYPE_MIFARE && size == 0x04) { // MIFARE
                char uid[size];
                memcpy(uid, data + offset, size);
                offset += size;
                dprintf(NAME ": Found a MIFARE card (%d)\n", size);
                dump(uid, size);

                uint32_t uidi = 0;
                memcpy(&uidi, uid, size);

                hr = aime_mifare_select(uidi);
                if (!SUCCEEDED(hr)){
                    last_card_type = CARD_TYPE_ILLEGAL;
                    last_card_len = 0;
                    return S_FALSE;
                }

                hr = aime_mifare_authenticate(uidi, 0x03);
                if (!SUCCEEDED(hr)){
                    last_card_type = CARD_TYPE_ILLEGAL;
                    last_card_len = 0;
                    return S_FALSE;
                }

                uint8_t len = CARD_BUF_LEN;
                uint8_t block[CARD_BUF_LEN];
                hr = aime_mifare_read_block(uidi, 2, block, &len);
                if (!SUCCEEDED(hr)){
                    last_card_type = CARD_TYPE_ILLEGAL;
                    last_card_len = 0;
                    return S_FALSE;
                }

                memcpy(&last_card_id, block + 6, 10);

                last_card_type = CARD_TYPE_MIFARE;
                last_card_len = 10;

            } else if (type == CARD_TYPE_FELICA) { // FeliCa
                if (size == 0x10) {
                    memcpy(last_card_id, data + offset, size);
                    offset += size;
                    last_card_type = CARD_TYPE_FELICA;
                    last_card_len = size;
                    dprintf(NAME ": Found a FeliCa card (%d)\n", size);
                    dump(last_card_id, size);
                } else {
                    last_card_type = CARD_TYPE_ILLEGAL;
                    last_card_len = 0;
                    return S_FALSE;
                }
            } else {
                last_card_type = CARD_TYPE_ILLEGAL;
                last_card_len = 0;
                return S_FALSE;
            }
        }
    }

    return S_OK;
}

const char* aime_get_card_id(){
    return last_card_id;
}

uint8_t aime_get_card_len(){
    return last_card_len;
}

uint8_t aime_get_card_type(){
    return last_card_type;
}

HRESULT aime_led_set(uint8_t r, uint8_t g, uint8_t b){

    dprintf(NAME ": Set LEDs (%d, %d, %d)\n", r, g, b);

    struct aime_req_any req = {0};
    req.cmd = AIME_CMD_LED_SET_COLOR;
    req.len = 3;
    req.payload[0] = r;
    req.payload[1] = g;
    req.payload[2] = b;

    HRESULT hr = aime_transact_packet(&req, NULL);

    if (!SUCCEEDED(hr)){
        return hr;
    }

    dprintf(NAME ": Set LEDs\n");

    return hr;
}

HRESULT aime_set_mifare_key_sega(const uint8_t* key, uint8_t len){
    dprintf(NAME ": Set MIFARE Key 1\n");

    struct aime_req_any req = {0};
    struct aime_resp_any resp = {0};
    req.cmd = AIME_CMD_MIFARE_SET_KEY_SEGA;
    req.len = len;
    memcpy(req.payload, key, len);

    HRESULT hr = aime_transact_packet(&req, &resp);

    dprintf(NAME ": Set MIFARE Key 1: %d\n", resp.status);

    return hr;
}
HRESULT aime_set_mifare_key_namco(const uint8_t* key, uint8_t len){
    dprintf(NAME ": Set MIFARE Key 2\n");

    struct aime_req_any req = {0};
    struct aime_resp_any resp = {0};
    req.cmd = AIME_CMD_MIFARE_SET_KEY_NAMCO;
    req.len = len;
    memcpy(req.payload, key, len);

    HRESULT hr = aime_transact_packet(&req, &resp);

    dprintf(NAME ": Set MIFARE Key 2: %d\n", resp.status);

    return hr;
}

HRESULT aime_mifare_select(uint32_t uid){
    dprintf(NAME ": Select Mifare: %d\n", uid);

    struct aime_req_any req = {0};
    struct aime_resp_any resp = {0};
    req.cmd = AIME_CMD_MIFARE_SELECT;
    req.len = 4;
    memcpy(req.payload, &uid, 4);

    HRESULT hr = aime_transact_packet(&req, &resp);

    dprintf(NAME ": Select Mifare: %d\n", resp.status);

    return hr;
}
HRESULT aime_mifare_authenticate(uint32_t uid, uint8_t unknown){
    dprintf(NAME ": Authenticate MIFARE: %d, %d\n", uid, unknown);

    struct aime_req_any req = {0};
    struct aime_resp_any resp = {0};
    req.cmd = AIME_CMD_MIFARE_AUTHENTICATE;
    req.len = 5;
    memcpy(req.payload, &uid, 4);
    req.payload[4] = unknown;

    HRESULT hr = aime_transact_packet(&req, &resp);

    dprintf(NAME ": Authenticate MIFARE: %d\n", resp.status);

    return hr;
}
HRESULT aime_mifare_read_block(uint32_t uid, uint8_t block, uint8_t* block_contents, uint8_t* block_len){

    if (block_contents == NULL || block_len == NULL){
        return E_HANDLE;
    }

    dprintf(NAME ": Read Block: %d, %d\n", uid, block);

    struct aime_req_any req = {0};
    struct aime_resp_any resp = {0};
    req.cmd = AIME_CMD_MIFARE_READ_BLOCK;
    req.len = 5;
    memcpy(req.payload, &uid, 4);
    req.payload[4] = block;

    HRESULT hr = aime_transact_packet(&req, &resp);
    if (!SUCCEEDED(hr)){
        return hr;
    }

    dprintf(NAME ": Read Block: %d\n", resp.status);
    dump(resp.payload, resp.len);

    if (resp.status != 0){
        return E_FAIL;
    }

    if (resp.len > *block_len){
        return E_NOT_SUFFICIENT_BUFFER;
    }

    memcpy(block_contents, resp.payload, resp.len);

    return hr;
}