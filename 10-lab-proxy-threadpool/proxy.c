#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sockhelper.h"

/* Recommended max object size */
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

int complete_request_received(char *);
void parse_request(char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);
int open_sfd(char* host, char* port);
void handle_client(int csfd);

int main(int argc, char *argv[])
{
	printf("%s\n", user_agent_hdr);

	int sfd = open_sfd("localhost", argv[1]);
	int csfd;
	while(1) {
		csfd = accept(sfd, NULL, NULL);
		if (csfd < 0) {
			perror("accept refused");
		} else {
			fprintf(stderr, "accepted!\n");
			handle_client(csfd);
		}
	}


	return 0;
}

int complete_request_received(char *request) {
	char* str = strstr(request, "\r\n\r\n");
	return str != NULL;
}


int open_sfd(char* host, char* port) {
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	int optval = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(strtol(port, NULL, 10));
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sfd, (struct sockaddr*)&addr, INET6_ADDRSTRLEN) < 0){
		perror("bind failed");
	}

	if(listen(sfd, 10) < 0){

		perror("listen failed");
	}
	return sfd;
}

void handle_client(int sfd){

	unsigned char buf[1024];
	memset(buf, 0, sizeof(buf));
	int bytes_recvd = -1;
	int total_recvd = 0;
	while(!complete_request_received((char*)buf)){
		bytes_recvd = recv(sfd, buf+total_recvd, 64, 0);
		total_recvd += bytes_recvd;
		fprintf(stderr, "total: %d\n this: %d\n", total_recvd, bytes_recvd);
	}
	buf[total_recvd] = '\0';
	print_bytes(buf, total_recvd);

	char method[16], hostname[64], port[8], path[64];
	parse_request((char*) buf, method, hostname, port, path);
	printf("METHOD: %s\n", method);
	printf("HOSTNAME: %s\n", hostname);
	printf("PORT: %s\n", port);
	printf("PATH: %s\n", path);

	char req[1024];
	memset(req, 0, sizeof(req));
	sprintf(req, "%s %s HTTP/1.0\r\n", method, path);
	sprintf(req+strlen(req), "Host: %s", hostname);
	if (strcmp(port, "80") == 0){
		sprintf(req+strlen(req), "\r\n");
	} else {
		sprintf(req+strlen(req), ":%s\r\n", port);
	}
	sprintf(req+strlen(req), "User-Agent: %s\r\n", user_agent_hdr);
	sprintf(req+strlen(req), "Connection: close\r\n");
	sprintf(req+strlen(req), "Proxy-Connection: close\r\n");
	sprintf(req+strlen(req), "\r\n");
	req[strlen(req)] = '\0';		
	print_bytes((unsigned char*) req, strlen(req));
	close(sfd);
}

/*
 * WARNING: LLM GENERATED CODE
 * LLM used: ChatGPT
 * prompt: I am writing code for a proxy server in C as part of 
 * a lab assignment in my CS324 class. One of the functions I am
 * writing is called parse_request. 
 * Write the code for this function such that it: 
 * (Copied and pasted information from the spec)
 *
 * No editing was required to make this code work. 
 */
void parse_request(char *request, char *method,
		char *hostname, char *port, char *path) {
	char *space1, *space2;
	char url[2048];

	//---- Extract METHOD ----
	space1 = strchr(request, ' ');
	strncpy(method, request, space1 - request);
	method[space1 - request] = '\0';

	// ---- Extract URL ----
	space2 = strchr(space1 + 1, ' ');
	strncpy(url, space1 + 1, space2 - (space1 + 1));
	url[space2 - (space1 + 1)] = '\0';

	// ---- Skip protocol (http://) ----
	char *after_protocol = strstr(url, "://");
	if (after_protocol) {
		after_protocol += 3;
	} else {
		after_protocol = url;
	}

	char *colon = strchr(after_protocol, ':');
	char *slash = strchr(after_protocol, '/');

	if (colon && slash && colon < slash) {
		// ---- Port specified ----
		// hostname
		strncpy(hostname, after_protocol, colon - after_protocol);
		hostname[colon - after_protocol] = '\0';

		// port
		strncpy(port, colon + 1, slash - (colon + 1));
		port[slash - (colon + 1)] = '\0';

		// path
		strcpy(path, slash);
	} else {
		// ---- No port specified ----
		strcpy(port, "80");
		if (slash) {

			// hostname
			strncpy(hostname, after_protocol, slash - after_protocol);
			hostname[slash - after_protocol] = '\0';

			// path
			strcpy(path, slash);
		} else {
			// URL with no explicit path
			strcpy(hostname, after_protocol);
			strcpy(path, "/");
		}
	}
}

void test_parser() {
	int i;
	char method[16], hostname[64], port[8], path[64];

       	char *reqs[] = {
		"GET http://www.example.com/index.html HTTP/1.0\r\n"
		"Host: www.example.com\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html?foo=1&bar=2 HTTP/1.0\r\n"
		"Host: www.example.com:8080\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://localhost:1234/home.html HTTP/1.0\r\n"
		"Host: localhost:1234\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html HTTP/1.0\r\n",

		NULL
	};
	
	for (i = 0; reqs[i] != NULL; i++) {
		printf("Testing %s\n", reqs[i]);
		if (complete_request_received(reqs[i])) {
			printf("REQUEST COMPLETE\n");
			parse_request(reqs[i], method, hostname, port, path);
			printf("METHOD: %s\n", method);
			printf("HOSTNAME: %s\n", hostname);
			printf("PORT: %s\n", port);
			printf("PATH: %s\n", path);
		} else {
			printf("REQUEST INCOMPLETE\n");
		}
	}
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
