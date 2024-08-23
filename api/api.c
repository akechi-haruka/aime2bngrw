#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>

#include "api/api.h"
#include "api/config.h"

#include "../aime2bngrw/util/dprintf.h"

static struct api_config api_cfg;

static unsigned int __stdcall api_socket_thread_proc(void *ctx);

static HANDLE api_socket_thread;
static SOCKET listen_socket = INVALID_SOCKET;
static SOCKET send_socket = INVALID_SOCKET;
static struct sockaddr_in send_addr;
static struct sockaddr_in recv_addr;
static bool threadExitFlag = false;

static bool api_card_state_switch = false;
static bool api_card_reading_state = false;
static bool api_aime_rgb_set = false;
static uint8_t api_aime_rgb[3];

HRESULT api_init(const char* config_filename) {

    WSADATA wsa;

    if (api_socket_thread != NULL) {
        dprintf("API: already running\n");
        return E_FAIL;
    }

    api_config_load(&api_cfg, config_filename);

    if (!api_cfg.enable) {
        dprintf("API: disabled\n");
        return S_OK;
    }
    dprintf("API: Initializing using port %d, group %d, device %d\n", api_cfg.port, api_cfg.groupId, api_cfg.deviceId);

    int err = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (err != 0) {
        dprintf("API: Failed to initialize, error %d\n", err);
        return E_FAIL;
    }

    // Create the UDP status socket
    listen_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (listen_socket == SOCKET_ERROR) {
        dprintf("API: Failed to open listen socket: %d\n", WSAGetLastError());
        return E_FAIL;
    }

    const char opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(api_cfg.port);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_socket, (SOCKADDR *) &recv_addr, sizeof(recv_addr))) {
        dprintf("API: bind (recv) failed with error %d\n", WSAGetLastError());
        return E_FAIL;
    }

    send_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (send_socket == SOCKET_ERROR) {
        dprintf("API: Failed to open send socket: %d\n", WSAGetLastError());
        return E_FAIL;
    }
    setsockopt(send_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(send_socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(api_cfg.port);
    send_addr.sin_addr.s_addr = inet_addr(api_cfg.bindAddr);
    dprintf("API: Send initialized on port %d\n", api_cfg.port);

    threadExitFlag = false;
    api_socket_thread = (HANDLE) _beginthreadex(NULL, 0, api_socket_thread_proc, NULL, 0, NULL);

    return S_OK;
}

static unsigned int __stdcall api_socket_thread_proc(void *ctx) {
    struct sockaddr_in sender_addr;
    int sender_addr_size = sizeof(sender_addr);

    int err = SOCKET_ERROR;
    char buf[PACKET_MAX_SIZE];

    // Loop while the thread is not aborting
    while (!threadExitFlag) {
        err = recvfrom(listen_socket, buf, PACKET_MAX_SIZE, 0, (SOCKADDR *) &sender_addr, &sender_addr_size);

        if (err != SOCKET_ERROR) {
            int id = buf[PACKET_HEADER_FIELD_ID];
            int group = buf[PACKET_HEADER_FIELD_GROUPID];
            int device = buf[PACKET_HEADER_FIELD_MACHINEID];
            int len = buf[PACKET_HEADER_FIELD_LEN];
            if (group != api_cfg.groupId) {
                if (api_cfg.log) {
                    dprintf("API: Received packet designated for group %d, but we're %d\n", group, api_cfg.groupId);
                }
                continue;
            }
            if (device == api_cfg.deviceId) {
                if (api_cfg.log) {
                    dprintf("API: Received packet from ourselves\n");
                }
                continue;
            }
            if (len > PACKET_CONTENT_MAX_SIZE) {
                len = PACKET_CONTENT_MAX_SIZE;
            }
            char data[PACKET_CONTENT_MAX_SIZE];
            for (int i = 0; i < len - PACKET_HEADER_LEN; i++) {
                data[i] = buf[PACKET_HEADER_LEN + i];
            }
            if (api_cfg.log) {
                dprintf("API: Received Packet: %d\n", id);
            }
            api_parse(id, len, data);
        } else {
            err = WSAGetLastError();
            dprintf("API: Receive error: %d\n", err);
            threadExitFlag = true;
        }
    }

    dprintf("API: Exiting\n");
    threadExitFlag = true;

    closesocket(listen_socket);
    closesocket(send_socket);
    WSACleanup();

    return 0;
}

int api_parse(int id, int len, const char *data) {

    switch (id) {
        case PACKET_20_PING:
            api_send(PACKET_21_ACK, 0, NULL);
            break;
        case PACKET_27_SEARCH:
            api_send(PACKET_20_PING, 0, NULL);
            break;
        case PACKET_33_AIME_RGB:
            api_aime_rgb_set = true;
            api_aime_rgb[0] = data[0];
            api_aime_rgb[1] = data[1];
            api_aime_rgb[2] = data[2];
            api_send(PACKET_21_ACK, 0, NULL);
            break;
        case PACKET_31_SET_CARD_READING_STATE:
            dprintf("API: Set card read state: %d\n", data[0]);
            api_card_state_switch = true;
            api_card_reading_state = data[0];
            api_send(PACKET_21_ACK, 0, NULL);
            break;
        default:
            return API_PACKET_ID_UNKNOWN;
    }

    return API_COMMAND_OK;
}

int api_send(int id, int len, const char *data) {

    if (!api_cfg.enable) {
        return API_DISABLED;
    }
    if (threadExitFlag) {
        return API_STATE_ERROR;
    }
    if (len > PACKET_CONTENT_MAX_SIZE) {
        return API_PACKET_TOO_LONG;
    }
    if (api_cfg.log) {
        dprintf("API: Sending Packet: %d\n", id);
    }
    int packetLen = PACKET_HEADER_LEN + len;
    char packet[packetLen];
    packet[PACKET_HEADER_FIELD_ID] = id;
    packet[PACKET_HEADER_FIELD_GROUPID] = api_cfg.groupId;
    packet[PACKET_HEADER_FIELD_MACHINEID] = api_cfg.deviceId;
    packet[PACKET_HEADER_FIELD_LEN] = len;
    for (int i = 0; i < len; i++) {
        packet[PACKET_HEADER_LEN + i] = data[i];
    }

    if (sendto(send_socket, packet, packetLen, 0, (SOCKADDR *) &send_addr, sizeof(send_addr)) == SOCKET_ERROR) {
        dprintf("API: sendto failed with error: %d\n", WSAGetLastError());
        return API_SOCKET_OPERATION_FAIL;
    }

    return API_COMMAND_OK;
}

void api_stop() {
    dprintf("API: shutdown\n");
    threadExitFlag = true;
    closesocket(listen_socket);
    closesocket(send_socket);
    WaitForSingleObject(api_socket_thread, INFINITE);
    CloseHandle(api_socket_thread);
    api_socket_thread = NULL;
}

bool api_get_card_switch_state() {
    return api_card_state_switch;
}

bool api_get_card_reading_state_and_clear_switch_state() {
    api_card_state_switch = false;
    return api_card_reading_state;
}

uint8_t* api_get_aime_rgb_and_clear() {
    if (api_aime_rgb_set){
        api_aime_rgb_set = false;
        return api_aime_rgb;
    } else {
        return NULL;
    }
}

void api_block_card_reader(bool b) {
    char data[1];
    data[0] = b;
    api_send(PACKET_32_BLOCK_CARD_READER, 1, data);
}