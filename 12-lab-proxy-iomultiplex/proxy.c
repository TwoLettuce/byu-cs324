#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <errno.h>
#include <fcntl.h>

#include "sockhelper.h"

/* Recommended max object size */
#define MAX_OBJECT_SIZE 102400
#define REQ_INFO_SIZE 16
#define READ_REQUEST 1
#define SEND_REQUEST 2
#define READ_RESPONSE 3
#define SEND_RESPONSE 4

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";
fd_set readfds;
fd_set writefds;

struct request_info {
	int csfd;
	int ssfd;
	char state;
	char buf[1024];
	int total_read_c;
	int bytes_to_write;
	int total_written_s;
	int total_read_s;
	int total_written_c;
};

int complete_request_received(char *);
void parse_request(char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(char *, int);
int open_sfd(char* host, char* port);
void handle_new_clients(int sfd);
void handle_client(struct request_info* info);



struct request_info info[REQ_INFO_SIZE];

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("usage: ./proxy <port>\n");
		return -1;
	}

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);

	int sfd = open_sfd("localhost", argv[1]);



	FD_SET(sfd, &readfds);
	fd_set readcpy;
	fd_set writecpy;
	fprintf(stderr, "listening socket: %d\n", sfd);
	while(1) {
		FD_ZERO(&readcpy);
		FD_ZERO(&writecpy);
		memcpy(&readcpy, &readfds, sizeof(fd_set));
		memcpy(&writecpy, &writefds, sizeof(fd_set));
		int nfds = select(1024, &readcpy, &writecpy, NULL, NULL); 
		if (nfds < 0) {
			perror("select");
			break;
		}

		for (int i = 0; i < REQ_INFO_SIZE; i++){
			if (FD_ISSET(i, &readcpy) || FD_ISSET(info[i].ssfd, &writecpy)
					|| FD_ISSET(i, &writecpy) || FD_ISSET(info[i].ssfd, &readcpy)) {
				nfds--;
				if (i == sfd){
					handle_new_clients(sfd);
				} else {
					handle_client(&info[i]);
				}
			} 
			if (nfds == 0){
				break;
			}
		}
	}



	printf("%s\n", user_agent_hdr);
	return 0;

}

int open_sfd(char* host, char* port){
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	int optval = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(strtol(port, NULL, 10));
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sfd, (struct sockaddr*)&addr, INET6_ADDRSTRLEN) < 0) {
		perror("bind failed");
	}

	if (listen(sfd, 10) < 0) {
		perror("listen failed");
	}

	if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		perror("nonblocking refused");
	}
	return sfd;
}

void handle_new_clients(int sfd) {


	while(1) {
		int csfd = accept(sfd, NULL, NULL);
		if (csfd < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			}
			perror("accept");
		}
		printf("Client accepted on FD: %d\n", csfd);

		if (fcntl(csfd, F_SETFL, fcntl(csfd, F_GETFL,0) | O_NONBLOCK) < 0) {
			perror("nonblocking refused");
		}

		struct request_info client;
		client.csfd = csfd;	
		client.ssfd = 0;
		client.state = READ_REQUEST;
		char buf[1024];
		memset(buf, 0, sizeof(buf));
		memcpy(client.buf, buf, sizeof(buf));
		client.total_read_c = 0;
		client.bytes_to_write = 0;
		client.total_written_s = 0;
		client.total_read_s = 0;
		client.total_written_c = 0;

		info[csfd] = client;
		FD_SET(csfd, &readfds);


	}

}

void handle_client(struct request_info* info){
	fprintf(stderr, "csfd: %d\n", info->csfd);
	fprintf(stderr, "ssfd: %d\n", info->ssfd);
	fprintf(stderr, "state: %d\n", info->state);
	char buf[1024];
	switch(info->state){
		case READ_REQUEST:
			fprintf(stderr, "reading!\n");
			memset(buf, 0, sizeof(buf));
			memcpy(buf, info->buf, info->total_read_c);
			int bytes_recvd = -1;
			int total_recvd = info->total_read_c;
			write(2, buf, total_recvd);
			while(!complete_request_received(buf)){
				bytes_recvd = recv(info->csfd, buf+total_recvd, 1024, 0);
				if (bytes_recvd < 0) {
					if (errno == EAGAIN || errno == EWOULDBLOCK){
						info->total_read_c = total_recvd;
						return;
					} else {
						perror("recv");
						close(info->csfd);
						FD_CLR(info->csfd, &readfds);
						free(info);
						return;
					}	
				}
				total_recvd += bytes_recvd;
				buf[total_recvd] = '\0';
				print_bytes(buf, total_recvd);
				memcpy(info->buf, buf, strlen(buf));
				char method[16], hostname[64], port[8], path[128];
				parse_request(buf, method, hostname, port, path);
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
				memcpy(info->buf, req, strlen(req));
				info->bytes_to_write = strlen(req);
				print_bytes(req, strlen(req));

				struct addrinfo hints;
				memset(&hints, 0, sizeof(struct addrinfo));
				hints.ai_family = AF_INET;
				hints.ai_socktype = SOCK_STREAM;

				struct addrinfo *result;
				int s;
				s = getaddrinfo(hostname, port, &hints, &result);
				if (s < 0){
					perror("getaddrinfo");
				}

				int ssfd;
				int addr_fam;
				socklen_t addr_len;

				struct sockaddr_storage local_addr_ss;
				struct sockaddr* local_addr = (struct sockaddr*)&local_addr_ss;
				char local_ip[INET_ADDRSTRLEN];
				unsigned short local_port;

				struct sockaddr_storage remote_addr_ss;
				struct sockaddr* remote_addr = (struct sockaddr*)&remote_addr_ss;
				char remote_ip[INET_ADDRSTRLEN];
				unsigned short remote_port;


				struct addrinfo *rp;
				for (rp = result; rp != NULL; rp = rp->ai_next){
					ssfd = socket(rp->ai_family, rp->ai_socktype, 0);
					if (ssfd < 0){
						perror("socket error");
						continue;
					}

					addr_fam = rp->ai_family;
					addr_len = rp->ai_addrlen;

					memcpy(remote_addr, rp->ai_addr, sizeof(struct sockaddr_storage));

					parse_sockaddr(remote_addr, remote_ip, &remote_port);
					fprintf(stderr, "Connecting to %s:%d (addr family: %d)\n", remote_ip, remote_port, addr_fam);

					if (connect(ssfd, remote_addr, addr_len) >= 0){
						break;
					}
					close(ssfd);
				}
				if (rp == NULL) {
					fprintf(stderr, "ERROR CONNECTING TO SERVER\n");
					perror("server error");
					exit(EXIT_FAILURE);
				}
				freeaddrinfo(result);

				addr_len = sizeof(struct sockaddr_storage);
				s = getsockname(ssfd, local_addr, &addr_len);

				parse_sockaddr(local_addr, local_ip, &local_port);
				fprintf(stderr, "Local socket info: %s:%d (addr fam: %d)\n", local_ip, local_port, addr_fam);


				if (fcntl(ssfd, F_SETFL, fcntl(ssfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
					perror("nonblocking refused");
				}

				FD_CLR(info->csfd, &readfds);
				FD_SET(ssfd, &writefds);
				info->ssfd = ssfd;
				info->state = SEND_REQUEST;
				break;
			}
			break;
		case SEND_REQUEST:
			fprintf(stderr, "sending request!\n");
			memset(buf, 0, sizeof(buf));
			memcpy(buf, info->buf, info->total_read_c);
			int bytes_written = -1;
			int total_written = info->total_written_s;
			while(1) {
				bytes_written = send(info->ssfd, buf+total_written, info->bytes_to_write, 0);
				if (bytes_written < 0) {
					fprintf(stderr, "error occured with send!\n");
					if (errno == EAGAIN || errno == EWOULDBLOCK){
						info->total_written_s = total_written;
						return;
					} else {
						perror("send to server");
						FD_CLR(info->ssfd, &writefds);
						close(info->csfd);
						close(info->ssfd);
						free(info);
						return;
					}
				}
				info->bytes_to_write -= bytes_written;
				total_written += bytes_written;
				if (info->bytes_to_write == 0){
					fprintf(stderr, "done sending!\n");
					FD_CLR(info->ssfd, &writefds);
					FD_SET(info->ssfd, &readfds);
					info->state = READ_RESPONSE;
					break;
				}
			}
			break;
		case READ_RESPONSE:
			fprintf(stderr, "reading response!\n");
			FD_CLR(info->ssfd, &readfds);
			break;
	}
}


int complete_request_received(char *request) {
	char* str = strstr(request, "\r\n\r\n");
	return str != NULL;
}


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

void print_bytes(char *bytes, int byteslen) {
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
