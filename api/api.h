#define PACKET_20_PING 20
#define PACKET_21_ACK 21
#define PACKET_22_TEST 22
#define PACKET_23_SERVICE 23
#define PACKET_24_CREDIT 24
#define PACKET_25_CARD_FELICA 25
#define PACKET_26_CARD_AIME 26
#define PACKET_27_SEARCH 27
#define PACKET_28_SEQUENCE 28
#define PACKET_29_VFD 29
#define PACKET_30_VFD_SHIFTJIS 30
#define PACKET_31_SET_CARD_READING_STATE 31
#define PACKET_32_BLOCK_CARD_READER 32
#define PACKET_33_AIME_RGB 33

#define PACKET_HEADER_LEN 4
#define PACKET_HEADER_FIELD_ID 0
#define PACKET_HEADER_FIELD_GROUPID 1
#define PACKET_HEADER_FIELD_MACHINEID 2
#define PACKET_HEADER_FIELD_LEN 3

#define PACKET_CONTENT_MAX_SIZE 128
#define PACKET_MAX_SIZE PACKET_CONTENT_MAX_SIZE + PACKET_HEADER_LEN

#define CARD_LEN_FELICA 16
#define CARD_LEN_AIME 20

#define API_DISABLED 0
#define API_COMMAND_OK 1
#define API_IS_ERROR(x) x < API_DISABLED
#define API_PACKET_TOO_LONG -1
#define API_STATE_ERROR -2
#define API_PACKET_ID_UNKNOWN -3
#define API_SOCKET_OPERATION_FAIL -4

HRESULT api_init(const char* config_filename);
void api_stop();
int api_parse(int id, int len, const char* data);
int api_send(int id, int len, const char* data);
bool api_get_card_switch_state();
bool api_get_card_reading_state_and_clear_switch_state();
void api_block_card_reader(bool b);
uint8_t* api_get_aime_rgb_and_clear();
