#ifndef LIB
#define LIB

#define CONST_TIME 5
#define CONST_MAXL 250

typedef unsigned char uc;

typedef struct {
	int len;
	char payload[1400];
} __attribute__((__packed__)) msg;

typedef struct {
	char MAXL;
	char TIME;
	char NPAD;
	char PADC;
	char EOL;
	char QCTL;
	char QBIN;
	char CHKT;
	char REPT;
	char CAPA;
	char R;
} __attribute__((__packed__)) sendInitData;

typedef struct {
	char SOH;
	char LEN;
	char SEQ;
	char TYPE;
	char DATA[CONST_MAXL];
	unsigned short CHECK;
	char MARK;
} __attribute__((__packed__)) miniKermit;

void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);

#endif

