#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>

int main(int argc, char *argv[]) {
	int pid;

	printf("Starting program; process has pid %d\n", getpid());
	
	FILE* outputFile = fopen("fork-output.txt", "w");
	int outputFileDescriptor = fileno(outputFile);
	fprintf(outputFile, "BEFORE FORK (%d)\n", outputFileDescriptor);
	fflush(outputFile);
	
	int pipefd[2];
	pipe(pipefd);
	
	if ((pid = fork()) < 0) {
		fprintf(stderr, "Could not fork()");
		exit(1);
	}

	/* BEGIN SECTION A */

	printf("Section A;  pid %d\n", getpid());
	//sleep(5);

	/* END SECTION A */
	if (pid == 0) {
		/* BEGIN SECTION B */
		printf("Section B\n");
	        sleep(10);
		write(pipefd[1], "hello from Section B\n", 21);
		sleep(10);
		close(pipefd[1]);
		//sleep(5);	
		fprintf(outputFile, "SECTION B (%d)\n", outputFileDescriptor);
		//sleep(30);
		//sleep(30);
		//printf("Section B done sleeping\n");
		char *newenviron[] = { NULL };

		printf("Program \"%s\" has pid %d. Sleeping.\n", argv[0], getpid());
		//sleep(30);

		if (argc <= 1) {
			printf("No program to exec.  Exiting...\n");
			exit(0);
		}
		
		printf("Running exec of \"%s\"\n", argv[1]);
		dup2(outputFileDescriptor, 1);
		execve(argv[1], &argv[1], newenviron);
		printf("End of program \"%s\".\n", argv[0]);
		exit(0);

		/* END SECTION B */
	} else {
		/* BEGIN SECTION C */

		printf("Section C\n");
		close(pipefd[1]);
		char buf[50]; 
		int charsRead = read(pipefd[0], buf, 21);
		buf[charsRead] = '\0';
		printf("Bytes read: %d\n", charsRead);
		printf(buf);
		char buf2[50];
		charsRead = read(pipefd[0], buf2, 21);
		printf("Bytes read; %d\n", charsRead);
		close(pipefd[0]);


		fprintf(outputFile, "SECTION C (%d)\n", outputFileDescriptor);
		fclose(outputFile);
		//sleep(5);
		
		//wait(NULL);
		//sleep(30);
		//printf("Section C done sleeping\n");

		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */

	printf("Section D\n");
	//sleep(30);

	/* END SECTION D */
}

