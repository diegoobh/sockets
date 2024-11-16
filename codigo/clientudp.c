/*
 *			C L I E N T U D P
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern int errno;

#define ADDRNOTFOUND	0xffffffff
#define RETRIES	5
#define BUFFERSIZE	1024
#define PUERTO 7527
#define TIMEOUT 6
#define MAXHOST 512

void handler() {
    printf("Alarma recibida \n");
}

int main(argc, argv)
int argc;
char *argv[];
{
    int errcode, n_retry;
    int s;                
    long timevar;         
    struct sockaddr_in myaddr_in;
    struct sockaddr_in servaddr_in;
    int addrlen;
    struct sigaction vec;
    char hostname[MAXHOST];
    char request[BUFFERSIZE];
    char response[BUFFERSIZE];
    struct addrinfo hints, *res;

    if (argc > 3) {
		fprintf(stderr, "Usage:  %s <remote host> [usuario[@host]]\n", argv[0]);
		exit(1);
	}

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == -1) {
        perror(argv[0]);
        fprintf(stderr, "%s: unable to create socket\n", argv[0]);
        exit(1);
    }

    memset((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
    memset((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

    myaddr_in.sin_family = AF_INET;
    myaddr_in.sin_port = 0;
    myaddr_in.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (const struct sockaddr *)&myaddr_in, sizeof(struct sockaddr_in)) == -1) {
        perror(argv[0]);
        fprintf(stderr, "%s: unable to bind socket\n", argv[0]);
        exit(1);
    }

    addrlen = sizeof(struct sockaddr_in);
    if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1) {
        perror(argv[0]);
        fprintf(stderr, "%s: unable to read socket address\n", argv[0]);
        exit(1);
    }

    time(&timevar);
    printf("Connected to %s on port %u at %s", argv[1], ntohs(myaddr_in.sin_port), (char *)ctime(&timevar));

    servaddr_in.sin_family = AF_INET;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    errcode = getaddrinfo(argv[1], NULL, &hints, &res);
    if (errcode != 0) {
        fprintf(stderr, "%s: Unable to resolve IP of %s\n", argv[0], argv[1]);
        exit(1);
    } else {
        servaddr_in.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    }
    freeaddrinfo(res);

    servaddr_in.sin_port = htons(PUERTO);

    vec.sa_handler = (void *)handler;
    vec.sa_flags = 0;
    if (sigaction(SIGALRM, &vec, (struct sigaction *)0) == -1) {
        perror("sigaction(SIGALRM)");
        fprintf(stderr, "%s: unable to register the SIGALRM signal\n", argv[0]);
        exit(1);
    }

    n_retry = RETRIES;

    while (n_retry > 0) {
		/* Build the request based on input arguments */
			if (argc == 2) {
				snprintf(request, BUFFERSIZE, "\r\n");
			} else {
				snprintf(request, BUFFERSIZE, "%s\r\n", argv[2]);
			}        
			if (sendto(s, request, strlen(request), 0, (struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) {
            perror(argv[0]);
            fprintf(stderr, "%s: unable to send request\n", argv[0]);
            exit(1);
	}

        alarm(TIMEOUT);

        if (recvfrom(s, response, BUFFERSIZE, 0, (struct sockaddr *)&servaddr_in, &addrlen) == -1) {
            if (errno == EINTR) {
                printf("Attempt %d (retries %d).\n", RETRIES - n_retry + 1, RETRIES);
                n_retry--;
            } else {
                perror(argv[0]);
                fprintf(stderr, "%s: unable to get response\n", argv[0]);
                exit(1);
            }
        } else {
            alarm(0);
            response[BUFFERSIZE - 1] = '\0';
            printf("Server response: %s\n", response);
            break;
        }
    }

    if (n_retry == 0) {
        printf("Unable to get response from %s after %d attempts.\n", argv[1], RETRIES);
    }

    close(s);
    return 0;
}
