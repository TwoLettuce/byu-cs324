// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1916400641

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "sockhelper.h"

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	for (int i = 0; i < argc; i++){
		printf("argv[%d] = %s\n", i, argv[i]);
	}

	char zero_byte = 0x0;
	char level = atoi(argv[3]);
	unsigned int userID = htonl(USERID);
	unsigned short seed = htons(atoi(argv[4]));
	unsigned char request[8];
	printf("zero_byte = %d\n", zero_byte);
	printf("USERID = %08x\n", USERID);
	printf("level = %02x\n", level);
	// printf("\nExample: %08x\n", 123456789);
	printf("seed = %04x\n", atoi(argv[4]));

	bzero(request, 8);
	memcpy(&request[0], &zero_byte, sizeof(char));
	memcpy(&request[1], &level, sizeof(char));
	memcpy(&request[2], &userID, sizeof(unsigned int));
	memcpy(&request[6], &seed, sizeof(unsigned short));
	print_bytes(request, 8);
}


void print_bytes(unsigned char *bytes, int byteslen) {
	int i, j, byteslen_adjusted;

	if (byteslen % 8) {
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	} else {
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++) {
		if (!(i % 8)) {
			if (i > 0) {
				for (j = i - 8; j < i; j++) {
					if (j >= byteslen_adjusted) {
						printf("  ");
					} else if (j >= byteslen) {
						printf("  ");
					} else if (bytes[j] >= '!' && bytes[j] <= '~') {
						printf(" %c", bytes[j]);
					} else {
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted) {
				printf("\n%02X: ", i);
			}
		} else if (!(i % 4)) {
			printf(" ");
		}
		if (i >= byteslen_adjusted) {
			continue;
		} else if (i >= byteslen) {
			printf("   ");
		} else {
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
	fflush(stdout);
}
