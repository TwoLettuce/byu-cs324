// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1916400641

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "sockhelper.h"

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	if (strcmp(argv[argc-1], "-v") == 0)
		verbose = 1;

	if (verbose){
		for (int i = 0; i < argc; i++){
			printf("argv[%d] = %s\n", i, argv[i]);
		}
	}

	char zero_byte = 0x0;
	char level = atoi(argv[3]);
	unsigned int userID = htonl(USERID);
	unsigned short seed = htons(atoi(argv[4]));
	unsigned char request[8];
	
	bzero(request, 8);
	memcpy(&request[0], &zero_byte, sizeof(char));
	memcpy(&request[1], &level, sizeof(char));
	memcpy(&request[2], &userID, sizeof(unsigned int));
	memcpy(&request[6], &seed, sizeof(unsigned short));
	if (verbose){
		printf("zero_byte = %d\n", zero_byte);
		printf("USERID = %08x\n", USERID);
		printf("level = %02x\n", level);
		printf("seed = %04x\n", atoi(argv[4]));
		print_bytes(request, 8);
	}


	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	struct addrinfo hints;
	// Initialize everything to 0
	memset(&hints, 0, sizeof(struct addrinfo));
	// Set the address family to either AF_INET (get only IPv4 address
	// information), AF_INET6 (get only IPv6 information), or AF_UNSPEC
	// (get both IPv4 and IPv6 information), depending on what was passed
	// in on the command line.
	hints.ai_family = AF_INET;
	// Use type SOCK_DGRAM (UDP)
	hints.ai_socktype = SOCK_DGRAM;

	struct addrinfo *result;

	char port[8];
	sprintf(port, argv[2]);
	getaddrinfo(argv[1], port, &hints, &result);

	struct sockaddr_storage remote_addr_ss;
	struct sockaddr* remote_addr = (struct sockaddr*) &remote_addr_ss;

	memcpy(remote_addr, result->ai_addr, sizeof(struct sockaddr_storage));

	char remote_ip[INET6_ADDRSTRLEN];
	unsigned short remote_port;
	parse_sockaddr(remote_addr, remote_ip, &remote_port);


	struct sockaddr_storage local_addr_ss;
	struct sockaddr* local_addr = (struct sockaddr*) &local_addr_ss;

	char local_ip[INET6_ADDRSTRLEN];
	unsigned short local_port;
	socklen_t addr_len = sizeof(struct sockaddr_storage);
	getsockname(sock, local_addr, &addr_len);
	parse_sockaddr(local_addr, local_ip, &local_port);
	if (verbose)
		printf("sending bytes\n");

	ssize_t nsent = sendto(sock, request, 8, 0, remote_addr, addr_len);

	unsigned char buf[256];
	unsigned char treasure[1024];
	unsigned short treasure_len = 0;
	int addr_fam = AF_INET;


	while (1) {	
		bzero(buf, 256);
		
		if (verbose){
			printf("receiving bytes\n");
		}

		ssize_t nreceived = recvfrom(sock, buf, 256, 0, remote_addr, &addr_len);

		if (verbose) {
			printf("Sent %ld bytes. Received %ld bytes.\n", nsent, nreceived);
			print_bytes(buf, nreceived);
		}

		unsigned char chunk_len = buf[0];
		if (chunk_len == 0){
			break;
		}

		unsigned char chunk[128];
		memcpy(chunk, &buf[1], chunk_len * sizeof(unsigned char));
		chunk[chunk_len] = '\0';

		unsigned char op_code = buf[chunk_len+1];

		unsigned short op_param_nw;
		memcpy(&op_param_nw, &buf[chunk_len+2], sizeof(unsigned short));
		unsigned short op_param = ntohs(op_param_nw);

		if(verbose) {
			printf("chunk length: %d\n", chunk_len);
			printf("chunk:\n%s\n", chunk);
			printf("Op Code: %d\n", op_code);
			printf("Op Param: %d\n", op_param);
		}

		memcpy(&treasure[treasure_len], chunk, chunk_len * sizeof(unsigned char));
		treasure_len += chunk_len;

		unsigned int nonce_nw;
		memcpy(&nonce_nw, &buf[chunk_len+4], sizeof(unsigned int));

		if(verbose)
			printf("nonce received: %08x\n", nonce_nw);

		unsigned int nonce = ntohl(nonce_nw);

		nonce++;
		if (verbose){
			printf("nonce host = %08x\n", nonce);
			printf("nonce response host = %08x\n", nonce);
		}

		nonce_nw = htonl(nonce);
		unsigned char nonce_str[4];
		memcpy(&nonce_str, &nonce_nw, sizeof(unsigned int));
		if (verbose) {
			printf("nonce sent: %08x\n", nonce_nw);
			print_bytes(nonce_str, 4);
			printf("\nTreasure: %s\n", treasure);
		}

		if (op_code == 1){
			unsigned short new_remote_port = op_param;
			populate_sockaddr(remote_addr, addr_fam, remote_ip, new_remote_port); 
		} else if (op_code == 2){
			close(sock);
			sock = socket(AF_INET, SOCK_DGRAM, 0);
			unsigned short new_local_port = op_param;
			populate_sockaddr(local_addr, addr_fam, NULL, new_local_port);
			if (bind(sock, local_addr, sizeof(struct sockaddr_storage)) < 0) {
				perror("bind()");
			}
		}

		nsent = sendto(sock, nonce_str, 4, 0, remote_addr, addr_len);
	}
	treasure[treasure_len] = '\0';
	printf("%s\n", treasure);

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
