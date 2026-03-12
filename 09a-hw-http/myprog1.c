#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argv, char** argc){
	char* content_length = getenv("CONTENT_LENGTH");
	char* query_string = getenv("QUERY_STRING");
	int length;
	if (strcmp(content_length, "")){
		length = atoi(content_length);
	} else {
		fprintf(stderr, "NO CONTENT FOUND");
		exit(-1);
	}

	char buf[length];

	fgets(buf, sizeof(buf)+1, stdin);
	char response[length*2 + 150];
	sprintf(response, "Hello CS324\nQuery string: %s\nRequest body: %s\n", query_string, buf);
	printf("Content-Type: text/plain\r\n");
	printf("Content-Length: %d\r\n", strlen(response));
	printf("\r\n");
	printf(response);
	
}
