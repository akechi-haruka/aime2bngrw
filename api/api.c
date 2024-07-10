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

#define DEFAULT_BUFLEN 512

static struct api_config api_cfg;
static unsigned int __stdcall api_socket_thread_proc(void *ctx);
static HANDLE api_socket_thread;
static SOCKET ListenSocket = INVALID_SOCKET;
static SOCKET SendSocket = INVALID_SOCKET;
static struct sockaddr_in SendAddr;
static struct sockaddr_in RecvAddr;
static struct sockaddr_in SendAddrBind;
static bool threadExitFlag = false;
static bool send_initialized = false;

static bool api_card_state_switch = false;
static bool api_card_reading_state = false;

HRESULT api_init(){

    WSADATA wsa;

	if (api_socket_thread != NULL) {
		dprintf("API: already running\n");
        return E_FAIL;
    }

    api_config_load(&api_cfg, L".\\segatools.ini");

	if (!api_cfg.enable){
        dprintf("API: disabled\n");
		return S_OK;
	}
	dprintf("API: Initializing using port %d\n", api_cfg.port);

    int err = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (err != 0) {
		dprintf("API: Failed to initialize, error %d\n", err);
        return E_FAIL;
    }

    // Create the UDP status socket
    ListenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ListenSocket == SOCKET_ERROR) {
        dprintf("API: Failed to open listen socket: %d\n", WSAGetLastError());
        return E_FAIL;
    }

	const char opt = 1;
	setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	//setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
	setsockopt(ListenSocket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

	RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(api_cfg.port);
    RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ListenSocket, (SOCKADDR *) & RecvAddr, sizeof (RecvAddr))) {
        dprintf("API: bind (recv) failed with error %d\n", WSAGetLastError());
        return E_FAIL;
    }

	api_initialize_send();

	threadExitFlag = false;
	api_socket_thread = (HANDLE) _beginthreadex(NULL, 0, api_socket_thread_proc, NULL, 0, NULL);

	return S_OK;
}

static unsigned int __stdcall api_socket_thread_proc(void *ctx)
{
	struct sockaddr_in SenderAddr;
    int SenderAddrSize = sizeof(SenderAddr);

    int err = SOCKET_ERROR;
    char ucBuffer[PACKET_MAX_SIZE];

    // Loop while the thread is not aborting
    while (!threadExitFlag) {
        err = recvfrom(ListenSocket, ucBuffer, PACKET_MAX_SIZE, 0, (SOCKADDR *) &SenderAddr, &SenderAddrSize);

        if (err != SOCKET_ERROR) {
            int id = ucBuffer[PACKET_HEADER_FIELD_ID];
			int len = ucBuffer[PACKET_HEADER_FIELD_LEN];
			int target = ucBuffer[PACKET_HEADER_FIELD_MACHINEID];
			if (target != api_cfg.networkId){
				dprintf("API: Received packet designated for machine %d, but we're %d\n", target, api_cfg.networkId);
			}
			if (len > PACKET_CONTENT_MAX_SIZE){
				len = PACKET_CONTENT_MAX_SIZE;
			}
			char data[PACKET_CONTENT_MAX_SIZE];
			for (int i = 0; i < len - PACKET_HEADER_LEN; i++){
				data[i] = ucBuffer[PACKET_HEADER_LEN + i];
			}
			dprintf("API: Received Packet: %d\n", id);
			api_parse(id, len, data);
        }
        else if (err != WSAEINTR) {
            err = WSAGetLastError();
            dprintf("API: Receive error: %d\n", err);
			threadExitFlag = true;
        }
    }

    dprintf("API: Exiting\n");
	threadExitFlag = true;
	send_initialized = false;

	closesocket(ListenSocket);
	closesocket(SendSocket);
    WSACleanup();

    return 0;
}

int api_parse(int id, int len, const char * data){

	char blank_data[0];
	switch (id){
		case PACKET_00_PING:
			api_send(PACKET_01_ACK, 0, blank_data);
			break;
		case PACKET_15_SET_CARD_READING_STATE:
            dprintf("API: Set card read state: %d\n", data[0]);
			api_card_state_switch = true;
			api_card_reading_state = data[0];
			api_send(PACKET_01_ACK, 0, blank_data);
			break;
		default:
			return API_PACKET_ID_UNKNOWN;
	}

	return API_COMMAND_OK;
}

// HACK: I have no idea why this needs to exist in the first place
// somehow there are different instances of SendSocket depending on where they're called from??
HRESULT api_initialize_send(){
	dprintf("API: Initializing send\n");

	const char opt = 1;
	SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == SOCKET_ERROR) {
        dprintf("API: Failed to open send socket: %d\n", WSAGetLastError());
        return E_FAIL;
    }
	setsockopt(SendSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	//setsockopt(ListenSocket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
	setsockopt(SendSocket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
	SendAddrBind.sin_family = AF_INET;
    SendAddrBind.sin_port = htons(api_cfg.port);
    SendAddrBind.sin_addr.s_addr = htonl(INADDR_ANY);
	SendAddr.sin_family = AF_INET;
    SendAddr.sin_port = htons(api_cfg.port);
    SendAddr.sin_addr.s_addr = inet_addr(api_cfg.bindAddr);
    if (bind(SendSocket, (SOCKADDR *) &SendAddrBind, sizeof (SendAddrBind))) {
        dprintf("API: bind (send) failed with error %d\n", WSAGetLastError());
        return E_FAIL;
    }
	dprintf("API: Send initialized on port %d\n", api_cfg.port+1);
	send_initialized = true;
	return S_OK;
}

int api_send(int id, int len, const char * data){

    if (!api_cfg.enable){
        return API_DISABLED;
    }
	if (threadExitFlag){
		return API_STATE_ERROR;
	}
	if (len > PACKET_CONTENT_MAX_SIZE){
		return API_PACKET_TOO_LONG;
	}
	if (!send_initialized){
		if (FAILED(api_init())){
			threadExitFlag = true;
			return API_STATE_ERROR;
		}
	}
	dprintf("API: Sending Packet: %d\n", id);
	int packetLen = PACKET_HEADER_LEN + len;
	char packet[packetLen];
	packet[PACKET_HEADER_FIELD_ID] = id;
	packet[PACKET_HEADER_FIELD_LEN] = len;
	packet[PACKET_HEADER_FIELD_MACHINEID] = api_cfg.networkId;
	for (int i = 0; i < len; i++){
		packet[PACKET_HEADER_LEN + i] = data[i];
	}

    if (sendto(SendSocket, packet, packetLen, 0, (SOCKADDR *) &SendAddr, sizeof(SendAddr)) == SOCKET_ERROR) {
        dprintf("API: sendto failed with error: %d\n", WSAGetLastError());
        return API_SOCKET_OPERATION_FAIL;
    }

	return API_COMMAND_OK;
}

void api_stop(){
	dprintf("API: shutdown\n");
	threadExitFlag = true;
	closesocket(ListenSocket);
	closesocket(SendSocket);
	WaitForSingleObject(api_socket_thread, INFINITE);
    CloseHandle(api_socket_thread);
    api_socket_thread = NULL;
}

bool api_get_card_switch_state(){
    return api_card_state_switch;
}

bool api_get_card_reading_state_and_clear_switch_state(){
    api_card_state_switch = false;
    return api_card_reading_state;
}

void api_block_card_reader(bool b){
    char data[1];
    data[0] = b;
    api_send(PACKET_16_BLOCK_CARD_READER, 1, data);
}