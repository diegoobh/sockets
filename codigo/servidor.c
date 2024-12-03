/*
 *          		S E R V I D O R
 *
 *	This is an example program that demonstrates the use of
 *	sockets TCP and UDP as an IPC mechanism.  
 *
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>



#define PUERTO 7527
#define ADDRNOTFOUND	0xffffffff	/* return address for unfound host */
#define BUFFERSIZE	1024	/* maximum size of packets to be received */
#define TAM_BUFFER 516
#define MAXHOST 128

extern int errno;

/*
 *			M A I N
 *
 *	This routine starts the server.  It forks, leaving the child
 *	to do all the work, so it does not have to be run in the
 *	background.  It sets up the sockets.  It
 *	will loop forever, until killed by a signal.
 *
 */
 
void serverTCP(int s, struct sockaddr_in peeraddr_in);
void serverUDP(int s, char * buffer, struct sockaddr_in clientaddr_in);
void errout(char *);		/* declare error out routine */
void procesar_peticion(const char *request, char *response);

int FIN = 0;             /* Para el cierre ordenado */
void finalizar(){ FIN = 1; }

void procesar_peticion(const char *request, char *respuesta) {

    // Limpia los espacios al inicio y al final de la petición.
    char trimmed[TAM_BUFFER];
    strncpy(trimmed, request, TAM_BUFFER);
    char *start = trimmed;
    while (*start == ' ' || *start == '\t') 
		start++; // Ignorar espacios iniciales.
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) 
		end--;
    *(end + 1) = '\0';

    if (strlen(start) == 0) { // Petición vacía
		fprintf(stderr, "Petición vacía. Ejecutando finger en el equipo local.\n");
		FILE *fp = popen("finger", "r"); // Ejecutar finger general en el equipo local
		if (fp == NULL) {
			snprintf(respuesta, TAM_BUFFER, "Error ejecutando finger en el equipo local.\n");
		} else {
			size_t bytesLeidos;
			respuesta[0] = '\0';  // Asegura que la respuesta esté vacía inicialmente.
			while ((bytesLeidos = fread(respuesta + strlen(respuesta), 1, TAM_BUFFER - strlen(respuesta) - 1, fp)) > 0) {
				respuesta[strlen(respuesta) + bytesLeidos] = '\0';  // Termina la cadena.
			}
			pclose(fp);
		}
		if (strlen(respuesta) == 0) {
			snprintf(respuesta, TAM_BUFFER, "No se pudo procesar la petición.\n");
		}

	} else {
		fprintf(stderr, "Petición: %s\n", start);
        // Petición con contenido.
        char command[TAM_BUFFER] = {0};
        if (strchr(start, '@')) {
            // Petición con usuario@host.
            snprintf(command, TAM_BUFFER, "finger %s", start);
        } else {
            // Petición con un usuario local.
            snprintf(command, TAM_BUFFER, "finger %s", start);
        }
        FILE *fp = popen(command, "r"); // Ejecutar comando en terminal
        if (fp == NULL) {
            snprintf(respuesta, TAM_BUFFER, "Error ejecutando finger para: %s", start);
        } else {
            fread(respuesta, 1, TAM_BUFFER - 1, fp);
            pclose(fp);
        }
    }
}

int main(argc, argv)
int argc;
char *argv[];
{

    int s_TCP, s_UDP;		/* connected socket descriptor */
    int ls_TCP;				/* listen socket descriptor */
    
    int cc;				    /* contains the number of bytes read */
     
    struct sigaction sa = {.sa_handler = SIG_IGN}; /* used to ignore SIGCHLD */
    
    struct sockaddr_in myaddr_in;	/* for local socket address */
    struct sockaddr_in clientaddr_in;	/* for peer socket address */
	int addrlen;
	
    fd_set readmask;
    int numfds,s_mayor;
    
    char buffer[BUFFERSIZE];	/* buffer for packets to be read into */
    
    struct sigaction vec;

		/* Create the listen socket. */
	ls_TCP = socket (AF_INET, SOCK_STREAM, 0);
	if (ls_TCP == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to create socket TCP\n", argv[0]);
		exit(1);
	}
	/* clear out address structures */
	memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
   	memset ((char *)&clientaddr_in, 0, sizeof(struct sockaddr_in));

    addrlen = sizeof(struct sockaddr_in);

		/* Set up address structure for the listen socket. */
	myaddr_in.sin_family = AF_INET;
		/* The server should listen on the wildcard address,
		 * rather than its own internet address.  This is
		 * generally good practice for servers, because on
		 * systems which are connected to more than one
		 * network at once will be able to have one server
		 * listening on all networks at once.  Even when the
		 * host is connected to only one network, this is good
		 * practice, because it makes the server program more
		 * portable.
		 */
	myaddr_in.sin_addr.s_addr = INADDR_ANY;
	myaddr_in.sin_port = htons(PUERTO);

	/* Bind the listen address to the socket. */
	if (bind(ls_TCP, (const struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to bind address TCP\n", argv[0]);
		exit(1);
	}
		/* Initiate the listen on the socket so remote users
		 * can connect.  The listen backlog is set to 5, which
		 * is the largest currently supported.
		 */
	if (listen(ls_TCP, 5) == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to listen on socket\n", argv[0]);
		exit(1);
	}
	
	
	/* Create the socket UDP. */
	s_UDP = socket (AF_INET, SOCK_DGRAM, 0);
	if (s_UDP == -1) {
		perror(argv[0]);
		printf("%s: unable to create socket UDP\n", argv[0]);
		exit(1);
	   }
	/* Bind the server's address to the socket. */
	if (bind(s_UDP, (struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) {
		perror(argv[0]);
		printf("%s: unable to bind address UDP\n", argv[0]);
		exit(1);
	    }

		/* Now, all the initialization of the server is
		 * complete, and any user errors will have already
		 * been detected.  Now we can fork the daemon and
		 * return to the user.  We need to do a setpgrp
		 * so that the daemon will no longer be associated
		 * with the user's control terminal.  This is done
		 * before the fork, so that the child will not be
		 * a process group leader.  Otherwise, if the child
		 * were to open a terminal, it would become associated
		 * with that terminal as its control terminal.  It is
		 * always best for the parent to do the setpgrp.
		 */
	setpgrp();

	switch (fork()) {
	case -1:		/* Unable to fork, for some reason. */
		perror(argv[0]);
		fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
		exit(1);

	case 0:     /* The child process (daemon) comes here. */

			/* Close stdin and stderr so that they will not
			 * be kept open.  Stdout is assumed to have been
			 * redirected to some logging file, or /dev/null.
			 * From now on, the daemon will not report any
			 * error messages.  This daemon will loop forever,
			 * waiting for connections and forking a child
			 * server to handle each one.
			 */
		fclose(stdin);
		fclose(stderr);

			/* Set SIGCLD to SIG_IGN, in order to prevent
			 * the accumulation of zombies as each child
			 * terminates.  This means the daemon does not
			 * have to make wait calls to clean them up.
			 */
		if ( sigaction(SIGCHLD, &sa, NULL) == -1) {
            perror(" sigaction(SIGCHLD)");
            fprintf(stderr,"%s: unable to register the SIGCHLD signal\n", argv[0]);
            exit(1);
            }
            
		    /* Registrar SIGTERM para la finalizacion ordenada del programa servidor */
        vec.sa_handler = (void *) finalizar;
        vec.sa_flags = 0;
        if ( sigaction(SIGTERM, &vec, (struct sigaction *) 0) == -1) {
            perror(" sigaction(SIGTERM)");
            fprintf(stderr,"%s: unable to register the SIGTERM signal\n", argv[0]);
            exit(1);
            }
        
		while (!FIN) {
            /* Meter en el conjunto de sockets los sockets UDP y TCP */
            FD_ZERO(&readmask);
            FD_SET(ls_TCP, &readmask);
            FD_SET(s_UDP, &readmask);
            /* 
            Seleccionar el descriptor del socket que ha cambiado. Deja una marca en 
            el conjunto de sockets (readmask)
            */ 
    	    if (ls_TCP > s_UDP) s_mayor=ls_TCP;
    		else s_mayor=s_UDP;

            if ( (numfds = select(s_mayor+1, &readmask, (fd_set *)0, (fd_set *)0, NULL)) < 0) {
                if (errno == EINTR) {
                    FIN=1;
		            close (ls_TCP);
		            close (s_UDP);
                    perror("\nFinalizando el servidor. Se�al recibida en elect\n "); 
                }
            }
           else { 

                /* Comprobamos si el socket seleccionado es el socket TCP */
                if (FD_ISSET(ls_TCP, &readmask)) {
                    /* Note that addrlen is passed as a pointer
                     * so that the accept call can return the
                     * size of the returned address.
                     */
    				/* This call will block until a new
    				 * connection arrives.  Then, it will
    				 * return the address of the connecting
    				 * peer, and a new socket descriptor, s,
    				 * for that connection.
    				 */
    			s_TCP = accept(ls_TCP, (struct sockaddr *) &clientaddr_in, &addrlen);
    			if (s_TCP == -1) exit(1);
    			switch (fork()) {
        			case -1:	/* Can't fork, just exit. */
        				exit(1);
        			case 0:		/* Child process comes here. */
                    			close(ls_TCP); /* Close the listen socket inherited from the daemon. */
        				serverTCP(s_TCP, clientaddr_in);
        				exit(0);
        			default:	/* Daemon process comes here. */
        					/* The daemon needs to remember
        					 * to close the new accept socket
        					 * after forking the child.  This
        					 * prevents the daemon from running
        					 * out of file descriptor space.  It
        					 * also means that when the server
        					 * closes the socket, that it will
        					 * allow the socket to be destroyed
        					 * since it will be the last close.
        					 */
        				close(s_TCP);
        			}
             } /* De TCP*/
          /* Comprobamos si el socket seleccionado es el socket UDP */
          if (FD_ISSET(s_UDP, &readmask)) {
                /* This call will block until a new
                * request arrives.  Then, it will
                * return the address of the client,
                * and a buffer containing its request.
                * BUFFERSIZE - 1 bytes are read so that
                * room is left at the end of the buffer
                * for a null character.
                */
                cc = recvfrom(s_UDP, buffer, BUFFERSIZE - 1, 0,
                   (struct sockaddr *)&clientaddr_in, &addrlen);
                if ( cc == -1) {
                    perror(argv[0]);
                    printf("%s: recvfrom error\n", argv[0]);
                    exit (1);
                    }
                /* Make sure the message received is
                * null terminated.
                */
                buffer[cc]='\0';
                serverUDP (s_UDP, buffer, clientaddr_in);
                }
          }
		}   /* Fin del bucle infinito de atenci�n a clientes */
        /* Cerramos los sockets UDP y TCP */
        close(ls_TCP);
        close(s_UDP);
    
        printf("\nFin de programa servidor!\n");
        
	default:		/* Parent process comes here. */
		exit(0);
	}

}

/*
 *				S E R V E R T C P
 *
 *	This is the actual server routine that the daemon forks to
 *	handle each individual connection.  Its purpose is to receive
 *	the request packets from the remote client, process them,
 *	and return the results to the client.  It will also write some
 *	logging information to stdout.
 *
 */
void serverTCP(int s, struct sockaddr_in clientaddr_in)
{
	int reqcnt = 0;		/* keeps count of number of requests */
	char buf[TAM_BUFFER];		/* This example uses TAM_BUFFER byte messages. */
	char hostname[MAXHOST];		/* remote host's name string */

	int len, status;
    struct hostent *hp;		/* pointer to host info for remote host */
    long timevar;			/* contains time returned by time() */
    
    struct linger linger;		/* allow a lingering, graceful close; */
    				            /* used when setting SO_LINGER */

	char respuesta_TCP[TAM_BUFFER];
    				
	/* Look up the host information for the remote host
	 * that we have connected with.  Its internet address
	 * was returned by the accept call, in the main
	 * daemon loop above.
	 */
	 
     status = getnameinfo((struct sockaddr *)&clientaddr_in,sizeof(clientaddr_in),
                           hostname,MAXHOST,NULL,0,0);
     if(status){
           	/* The information is unavailable for the remote
			 * host.  Just format its internet address to be
			 * printed out in the logging information.  The
			 * address will be shown in "internet dot format".
			 */
			 /* inet_ntop para interoperatividad con IPv6 */
            if (inet_ntop(AF_INET, &(clientaddr_in.sin_addr), hostname, MAXHOST) == NULL)
            	perror(" inet_ntop \n");
             }
    /* Log a startup message. */
    time (&timevar);
		/* The port number must be converted first to host byte
		 * order before printing.  On most hosts, this is not
		 * necessary, but the ntohs() call is included here so
		 * that this program could easily be ported to a host
		 * that does require it.
		 */
	printf("Startup from %s port %u at %s",
		hostname, ntohs(clientaddr_in.sin_port), (char *) ctime(&timevar));

		/* Set the socket for a lingering, graceful close.
		 * This will cause a final close of this socket to wait until all of the
		 * data sent on it has been received by the remote host.
		 */
	linger.l_onoff  =1;
	linger.l_linger =1;
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &linger,
					sizeof(linger)) == -1) {
		errout(hostname);
	}

		/* Go into a loop, receiving requests from the remote
		 * client.  After the client has sent the last request,
		 * it will do a shutdown for sending, which will cause
		 * an end-of-file condition to appear on this end of the
		 * connection.  After all of the client's requests have
		 * been received, the next recv call will return zero
		 * bytes, signalling an end-of-file condition.  This is
		 * how the server will know that no more requests will
		 * follow, and the loop will be exited.
		 */
	while (len = recv(s, buf, TAM_BUFFER, 0)) {
        if (len == -1) errout(hostname);
        buf[len] = '\0'; // Asegurar terminación de la cadena.
        procesar_peticion(buf, respuesta_TCP);

		if (send(s, respuesta_TCP, strlen(respuesta_TCP), 0) != strlen(respuesta_TCP)) {
			fprintf(stderr, "Servidor: Error al enviar respuesta al cliente\n");
		}

		reqcnt++;
    }
    close(s);

		/* Log a finishing message. */
	time (&timevar);
		/* The port number must be converted first to host byte
		 * order before printing.  On most hosts, this is not
		 * necessary, but the ntohs() call is included here so
		 * that this program could easily be ported to a host
		 * that does require it.
		 */
	printf("Completed %s port %u, %d requests, at %s\n",
		hostname, ntohs(clientaddr_in.sin_port), reqcnt, (char *) ctime(&timevar));
}

/*
 *	This routine aborts the child process attending the client.
 */
void errout(char *hostname)
{
	printf("Connection with %s aborted on error\n", hostname);
	exit(1);     
}


/*
 *				S E R V E R U D P
 *
 *	This is the actual server routine that the daemon forks to
 *	handle each individual connection.  Its purpose is to receive
 *	the request packets from the remote client, process them,
 *	and return the results to the client.  It will also write some
 *	logging information to stdout.
 *
 */
void serverUDP(int s, char * buffer, struct sockaddr_in clientaddr_in)
{
    struct in_addr reqaddr;	/* for requested host's address */
    struct hostent *hp;		/* pointer to host info for requested host */
    int nc, errcode;

    struct addrinfo hints, *res;

	char respuesta_UDP[BUFFERSIZE];

	int addrlen;

   	addrlen = sizeof(struct sockaddr_in);

      memset (&hints, 0, sizeof (hints));
      hints.ai_family = AF_INET;

	procesar_peticion(buffer, respuesta_UDP);
    nc = sendto(s, respuesta_UDP, strlen(respuesta_UDP), 0, (struct sockaddr *)&clientaddr_in, addrlen);

	if ( nc == -1) {
         perror("serverUDP");
         printf("%s: sendto error\n", "serverUDP");
         return;
    }   
 }
