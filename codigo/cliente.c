#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <time.h>

extern int errno;

// Defines cliente TCP
#define PUERTO 7527
#define TAM_BUFFER 516

// Defines cliente UDP
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
	int addrlen, i, j, errcode, n_retry, cc;
    int s;				
   	struct addrinfo hints, *res;
    long timevar;			
    struct sockaddr_in myaddr_in;	/* for local socket address */
    struct sockaddr_in servaddr_in;	/* for server socket address */
    struct sigaction vec;
    char hostname[MAXHOST] = "localhost";
	char buf[TAM_BUFFER];
    char request[TAM_BUFFER];
    char response[TAM_BUFFER];

    if (argc > 3) {
        fprintf(stderr, "Usage: %s TCP/UDP [usuario[@host]]\n", argv[0]);
        exit(1);
    }

    if (strcmp(argv[1], "TCP") == 0) {
        /* Create the socket. */
        s = socket (AF_INET, SOCK_STREAM, 0);
        if (s == -1) {
            perror(argv[0]);
            fprintf(stderr, "%s: unable to create socket\n", argv[0]);
            exit(1);
        }
        
        /* clear out address structures */
        memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
        memset ((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

        /* Set up the peer address to which we will connect. */
        servaddr_in.sin_family = AF_INET;

        /* Get the host information for the hostname that the
            user passed in. */
        memset (&hints, 0, sizeof (hints));
        hints.ai_family = AF_INET;

        /* esta funci�n es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta*/
        errcode = getaddrinfo (hostname, NULL, &hints, &res); 
        if (errcode != 0){
                /* Name was not found.  Return a
                * special value signifying the error. */
            fprintf(stderr, "%s: No es posible resolver la IP de %s\n",
                    argv[0], hostname);
            exit(1);
            }
        else {
            /* Copy address of host */
            servaddr_in.sin_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
            }
        freeaddrinfo(res);

        /* puerto del servidor en orden de red*/
        servaddr_in.sin_port = htons(PUERTO);

        /* Try to connect to the remote server at the address
        * which was just built into peeraddr.
        */
        if (connect(s, (const struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) {
            perror(argv[0]);
            fprintf(stderr, "%s: unable to connect to remote\n", argv[0]);
            exit(1);
        }
            /* Since the connect call assigns a free address
            * to the local end of this connection, let's use
            * getsockname to see what it assigned.  Note that
            * addrlen needs to be passed in as a pointer,
            * because getsockname returns the actual length
            * of the address.
            */
        addrlen = sizeof(struct sockaddr_in);
        if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1) {
            perror(argv[0]);
            fprintf(stderr, "%s: unable to read socket address\n", argv[0]);
            exit(1);
        }

        /* Print out a startup message for the user. */
        time(&timevar);
        /* The port number must be converted first to host byte
        * order before printing.  On most hosts, this is not
        * necessary, but the ntohs() call is included here so
        * that this program could easily be ported to a host
        * that does require it.
        */
        printf("Connected to %s on port %u at %s",
                hostname, ntohs(myaddr_in.sin_port), (char *) ctime(&timevar));

        /* Build the request based on input arguments */
        if (argc == 2) {
            snprintf(request, TAM_BUFFER, "\r\n");
        } else {
            snprintf(request, TAM_BUFFER, "%s\r\n", argv[2]);
        }

        /* Send the request to the server */
        if (send(s, request, strlen(request), 0) != strlen(request)) {
            fprintf(stderr, "%s: Connection aborted on error ", argv[0]);
            exit(1);
        }

        /* Now, start receiving all of the replys from the server.
        * This loop will terminate when the recv returns zero,
        * which is an end-of-file condition.  This will happen
        * after the server has sent all of its replies, and closed
        * its end of the connection.
        */
        while (i = recv(s, buf, TAM_BUFFER, 0)) {
            if (i == -1) {
                perror(argv[0]);
                fprintf(stderr, "%s: error reading result\n", argv[0]);
                exit(1);
            }
            buf[i] = '\0';
            printf("%s", buf);
        }

        /* Print message indicating completion of task. */
        time(&timevar);
        printf("All done at %s", (char *)ctime(&timevar));

    } else if (strcmp(argv[1], "UDP") == 0) {

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
        printf("Connected to %s on port %u at %s", hostname, ntohs(myaddr_in.sin_port), (char *)ctime(&timevar));

        servaddr_in.sin_family = AF_INET;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;

        errcode = getaddrinfo(hostname, NULL, &hints, &res);
        if (errcode != 0) {
            fprintf(stderr, "%s: Unable to resolve IP of %s\n", argv[0], hostname);
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

            if ((cc = recvfrom(s, response, BUFFERSIZE - 1, 0, (struct sockaddr *)&servaddr_in, &addrlen)) == -1) {
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
                response[cc] = '\0';
                printf("Server response: %s\n", response);
                break;
            }
        }

        if (n_retry == 0) {
            printf("Unable to get response from %s after %d attempts.\n", hostname, RETRIES);
        }

    } else {
        fprintf(stderr, "Error de comparación");
        fprintf(stderr, "Usage: %s TCP/UDP [usuario[@host]]\n", argv[0]);
        exit(1);
    }

    return 0;
}
