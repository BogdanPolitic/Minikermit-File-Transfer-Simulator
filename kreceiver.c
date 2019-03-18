#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"
#include <limits.h>

#define HOST "127.0.0.1"
#define PORT 10001

// [global variables]
char EXCEED, seqNo;
// end of [global variables]

//[functions]
char* outputFileName(char* fileName, char* str) {				// genereaza numele fisierului de output, dupa numele celui de input
	memcpy(str, "recv_", 5);
	memcpy(str + 5, fileName, strlen(fileName));
	return str;
}

char notEOT(miniKermit p, char wrongCRC) {					// returneaza true cat timp nu s-a ajuns la sfarsitul intregului transfer
	if (!wrongCRC)								// se verifica si crc-ul deoarece caracterul 'B' poate fi corupt si pachetul sa nu fie tip B
		if (p.TYPE == 'B')
			return 0;
	return 1;
}

msg* receive_conf(char isSendInit) {						// functioneaza ca functia receive_message_timeout, cu timpul de asteptare adaptat conditiei
	if (isSendInit) {							// isSendInit (1 daca pachetul este de tip 'S', 0 altfel), la fel si cu numarul de timpi de
		EXCEED = 1;							// retransmitere a mesajului, in caz de timeout
		return receive_message_timeout(CONST_TIME * 1000 * 3);
	} else {
		EXCEED = 4;
		return receive_message_timeout(CONST_TIME * 1000);
	}
}

miniKermit defaultLOST() {							//returneaza un pachet de tip L (protocol pentru cazul pierderii din transfer a unui fisier)
	char buffer[CONST_MAXL + 4];
	miniKermit L;
	L.SOH = 1;
	L.LEN = 5;
	L.SEQ = seqNo - 1;
	L.TYPE = 'L';
	memset(L.DATA, 0, CONST_MAXL);
	memcpy(buffer, &L, CONST_MAXL + 4);
	L.CHECK = crc16_ccitt(buffer, CONST_MAXL + 4);
	L.MARK = 13;
	return L;
}

miniKermit defaultACK() {							// returneaza un pachet de tip Y (acknoledgment)
	miniKermit a;
	char buffer[CONST_MAXL + 4];
	a.SOH = 1;
	a.SEQ = seqNo - 1;
	a.TYPE = 'Y';
	sprintf(a.DATA, "ACK[%d]", a.SEQ);
	a.LEN = (5 + !(!(a.SEQ / 10)) + 1) + 5;
	memset(a.DATA, 0, CONST_MAXL - (uc) a.LEN + 5);
	memcpy(buffer, &a, CONST_MAXL + 4);
	a.CHECK = crc16_ccitt(buffer, CONST_MAXL + 4);
	a.MARK = 13;
	return a;
}

miniKermit defaultNAK() {							// returneaza un pachet de tip N (no acknoledgement)
	miniKermit n;
	char buffer[CONST_MAXL + 4];
	n.SOH = 1;
	n.LEN = 5;
	n.SEQ = seqNo - 1;
	n.TYPE = 'N';
	memset(n.DATA, 0, CONST_MAXL);
	memcpy(buffer, &n, CONST_MAXL + 4);
	n.CHECK = crc16_ccitt(buffer, CONST_MAXL + 4);
	n.MARK = 13;
	return n;
}

char badSequence(char seqno) {							// informeaza cu privire la secventa. Daca este gresita, inseamna ca un mesaj mai vechi,
	return seqno != seqNo - 1;						// intarziat, a ajuns inaintea celui corect
}

void sendPackage(miniKermit p, msg* t) {					// trimite pachetul transmis ca parametru la sender
	t->len = CONST_MAXL + 7;
	memcpy(t->payload, &p, t->len);
	send_message(t);
}
// end of [functions]

int main(int argc, char** argv) {
	init(HOST, PORT);

	// [variables]
	int outputFD;
	char isSendInit;
	miniKermit pkg;
	msg t;
	msg* y;
	char timesLost;
	char wrongCRC;
	char isAck;
	char streamWriter[CONST_MAXL];
	// end of [variables]

	isSendInit = 1;
	pkg.TYPE = ' ';
	wrongCRC = 1;
	seqNo = 1;

	t.len = CONST_MAXL + 7;

	while (notEOT(pkg, wrongCRC)) {
		if (pkg.TYPE == 'H' && !wrongCRC && !badSequence((pkg.SEQ + 2) % 64)) { // + 2 deoarece este pkg-ul precedent, format in bucla anterioara
			outputFD = open(outputFileName(pkg.DATA, (char*)malloc(50)), O_WRONLY | O_CREAT | O_TRUNC, -1);
		}
		isAck = 0;
		timesLost = 0;

		while (!isAck) {
			y = receive_conf(isSendInit);
			while (y == NULL) {
				if (++timesLost >= EXCEED) {
					close(outputFD);
					exit(0);
				}
				sendPackage(defaultLOST(), &t);
				y = receive_message_timeout(CONST_TIME * 5);
			}
			memcpy(&pkg, y->payload, CONST_MAXL + 7);
			if (crc16_ccitt(y->payload, y->len - 3) == pkg.CHECK) {
				if (badSequence(pkg.SEQ)) { 				// + 0 deoarece este pkg-ul format in bucla curenta, deci corespunde cu seqNo curent
					pkg.TYPE = ' ';
					continue;
				}
				else {
					sendPackage(defaultACK(), &t);
					seqNo += 2;
					seqNo %= 64;
					wrongCRC = 0;
					isAck = 1;
					if (!(pkg.TYPE == 'S' || pkg.TYPE == 'H')) {
						memcpy(streamWriter, pkg.DATA, (uc) pkg.LEN - 5);
						write(outputFD, streamWriter, (uc) pkg.LEN - 5);
					}
					isSendInit = 0;
				}
			} else {
				sendPackage(defaultNAK(), &t);
				seqNo += 2;
				seqNo %= 64;
				wrongCRC = 1;
			}
		}
	}

	close(outputFD);

	return 0;
}
