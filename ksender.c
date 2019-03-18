#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

// [structures]
typedef struct {
	char prev;
	char curr;
} stats;
// end of [structures]

// [global variables]
char EXCEED, seqNo = 0;
// end of [global variables]

// [aux functions]
int min (int no1, int no2) {							// calculeaza minimul dintre 2 int-uri
	if (no1 < no2) {
		return no1;
	}
	return no2;
}

char* outputFileName(char* fileName, char* str) {				// genereaza numele fisierului de output, dupa numele celui de input
	memcpy(str, "log_", 4);
	memcpy(str + 4, fileName, strlen(fileName));
	return str;
}

long int fileSize(char* name) {							// intoarce dimensiunea fisierului al carui nume e dat ca parametru
	long int size;
	FILE* f = fopen(name, "r+");
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);
	return size;
}

void fillSendInitData(miniKermit* first) {					// umple campul DATA al pachetului de tip 'S' (Send-Init)
	sendInitData s;
	s.MAXL = CONST_MAXL;
	s.TIME = CONST_TIME;
	s.NPAD = 0;
	s.PADC = 0;
	s.QCTL = s.QBIN = s.CHKT = s.REPT = s.CAPA = s.R = 0;
	s.EOL = 0x0D;
	memcpy(&first->DATA, &s, 11);
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

char lostOrTimeout(msg* msg) {							// informeaza sender-ul daca fisierul a fost pierdut sau nu a venit in timp util
	if (msg == NULL) {
		return 1;
	}
	if (msg->payload[3] == 'L') {
		return 1;
	}
	return 0;
}

char badSequence(char seqno) {							// informeaza cu privire la secventa. Daca este gresita, inseamna ca un mesaj mai vechi,
	return seqno != seqNo;							// intarziat, a ajuns inaintea celui corect
}

char nextStep(miniKermit pkg, int fileNo, int total) {				// returneaza true, cat timp nu s-a ajuns la sfarsitul transferului unui singur fisier
	return !((pkg.TYPE == 'B' && fileNo == total - 1) || (pkg.TYPE == 'Z' && fileNo < total - 1));
}

char stoppedReading(int inputFileSize, int dataSizeRead) {			// returneaza true, cat timp mai este de citit (si de transferat) din fisierul respectiv
	return inputFileSize - dataSizeRead == 0;
}

char nak(miniKermit n) {							// informeaza daca a pachetul trimis a fost acceptat (false) sau nu (true) de receiver
	return n.TYPE == 'N';
}
// end of [aux functions]

int main(int argc, char** argv) {
	init(HOST, PORT);

	// [variables]
	int fileNo;
	int inputFD, outputFD, inputFileSize;
	stats first, header;
	miniKermit firstPkg, pkg;
	char buffer[CONST_MAXL + 4];
	int dataSizeRead;
	char EOFl;
	char timesLost;
	msg t;
	msg* confY;
	miniKermit confP;
	char streamWriter[CONST_MAXL];
	// end of [variables]

	first.prev = first.curr = header.prev = header.curr = 0;
	first.curr = 1;

	for (fileNo = 1; fileNo < argc; fileNo++) {
		// [opening files]
		inputFD = open(argv[fileNo], O_RDONLY);
		inputFileSize = fileSize(argv[fileNo]);
		outputFD = open(outputFileName(argv[fileNo], (char*)malloc(50)), O_WRONLY | O_CREAT | O_TRUNC, -1);
		// end of [opening files]

		dataSizeRead = 0;
		EOFl = 0;

		pkg.SOH = firstPkg.SOH = 1;
		pkg.TYPE = ' ';
		pkg.MARK = firstPkg.MARK = 13;
		
		while(nextStep(pkg, fileNo, argc)) {
			pkg.SEQ = firstPkg.SEQ = seqNo;
			if (first.curr) {
				firstPkg.LEN = 16;
				firstPkg.TYPE = 'S';
				fillSendInitData(&firstPkg);
				memset(firstPkg.DATA + 11, 0, CONST_MAXL - 11);
				memcpy(buffer, &firstPkg, CONST_MAXL + 4);
				firstPkg.CHECK = crc16_ccitt(buffer, CONST_MAXL + 4);
				first.curr = 0;
				first.prev = 1;
				header.curr = 1;
			} else if (header.curr == 1) {
				pkg.LEN = strlen(argv[fileNo]) + 5;
				pkg.TYPE = 'H';
				memcpy(pkg.DATA, argv[fileNo], (uc) pkg.LEN - 5);
				first.prev = 0;
				header.curr = 0;
				header.prev = 1;
			} else if (header.prev == 1 || !stoppedReading(inputFileSize, dataSizeRead)) {
				pkg.LEN = min(inputFileSize - dataSizeRead, CONST_MAXL) + 5;
				pkg.TYPE = 'D';
				read(inputFD, pkg.DATA, (uc) pkg.LEN - 5);
				dataSizeRead += (uc) pkg.LEN - 5;
				EOFl = !(inputFileSize - dataSizeRead);
				header.prev = 0;
			} else if (stoppedReading(inputFileSize, dataSizeRead) && EOFl) {
				pkg.LEN = 5;
				pkg.TYPE = 'Z';
				if (fileNo != argc - 1) {
					header.curr = 1;
				}
				EOFl = 0;
			} else if (!EOFl && fileNo == argc - 1) {
				pkg.LEN = 5;
				pkg.TYPE = 'B';
			}

			if (!first.prev) {
				memset(pkg.DATA + (uc) pkg.LEN - 5, 0, CONST_MAXL - (uc) pkg.LEN + 5);
				memcpy(buffer, &pkg, CONST_MAXL + 4);
				pkg.CHECK = crc16_ccitt(buffer, CONST_MAXL + 4);
			}

			if (first.prev) {
				memcpy(&pkg, &firstPkg, CONST_MAXL + 7);
			}

			t.len = CONST_MAXL + 7;
			memcpy(t.payload, &pkg, t.len);
			timesLost = 0;
			confY = NULL;

			while (lostOrTimeout(confY) || badSequence((confP.SEQ + 2) % 64) || nak(confP)) { // + 2 deoarece am crescut deja seqNo cu 2 (la inceput oricum se 
				if (confY != NULL) {							  // indeplineste prima conditie)
					if (badSequence(confY->payload[2] + 2)) {			  // + 2 deoarece am crescut deja seqNo cu 2 (la inceput oricum nu 
						timesLost = 0;						  // are cum sa se ajunga aici)
					} else {
						pkg.SEQ = seqNo;
						memcpy(buffer, &pkg, CONST_MAXL + 4);
						pkg.CHECK = crc16_ccitt(buffer, CONST_MAXL + 4);
						memcpy(t.payload, &pkg, CONST_MAXL + 7);
					}
				}
				send_message(&t);
				confY = receive_conf(pkg.TYPE == 'S');
				if (lostOrTimeout(confY)) {
					if (++timesLost > EXCEED) {
						close(inputFD);
						close(outputFD);
						exit(0);
					}
					continue;
				}
				memcpy(&confP, confY->payload, confY->len);
				if (badSequence(confP.SEQ)) {
					continue;
				}

				if (confP.TYPE == 'N') {
					sprintf(streamWriter, "NAK[%d]\n", confP.SEQ);
				}
				if (confP.TYPE == 'Y') {
					sprintf(streamWriter, "ACK[%d]\n", confP.SEQ);
				}
				if (confP.TYPE == 'N' || confP.TYPE == 'Y') {
					seqNo += 2;
					seqNo %= 64;
					write(outputFD, streamWriter, 5 + !(!(confP.SEQ / 10)) + 1 + 1);
				}
			}
		}
	}

	close(outputFD);
	close(inputFD);

	return 0;
}
