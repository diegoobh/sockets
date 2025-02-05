/*
** Fichero: cliente.c 
** Autores:
** Diego Borrallo Herrero
** Jaime Castellanos Rubio
*/

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
#include <sys/sem.h>

int sem_id;

extern int errno;

// Defines cliente TCP
#define PUERTO 7527
#define TAM_BUFFER 516

// Defines cliente UDP
#define ADDRNOTFOUND	0xffffffff
#define RETRIES	5
#define PUERTO 7527
#define TIMEOUT 30
#define MAXHOST 512

void handler() {
    printf("Alarma recibida \n");
}

void wait_sem(int sem_id) {
    struct sembuf sb = {0, -1, 0}; // Operación de resta
    if (semop(sem_id, &sb, 1) == -1) {
        perror("Error en wait_sem");
        exit(1);
    }
}

void signal_sem(int sem_id) {
    struct sembuf sb = {0, 1, 0}; // Operación de suma 
    if (semop(sem_id, &sb, 1) == -1) {
        perror("Error en signal_sem");
        exit(1);
    }
}

void escribir_log(char *mensaje, unsigned int puerto) {
    wait_sem(sem_id); // Bloquea el acceso al archivo

    char nombre_fichero[TAM_BUFFER];
    snprintf(nombre_fichero, TAM_BUFFER, "%u.txt", puerto);

    FILE *fichero = fopen(nombre_fichero, "a");
    if (fichero == NULL) {
        perror("Error abriendo archivo de log");
        signal_sem(sem_id); // Asegurar liberación del semáforo
        exit(1);
    }

    fprintf(fichero, "%s\n", mensaje);
    fclose(fichero);

    signal_sem(sem_id); // Libera el acceso al archivo
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
    char hostname[TAM_BUFFER];
	char buf[TAM_BUFFER];
    char peticion[TAM_BUFFER];
    char usuario[TAM_BUFFER];
    char respuesta_TCP[TAM_BUFFER];
    char respuesta_UDP[TAM_BUFFER];
    char log[TAM_BUFFER];

    // Estructura para semáforos
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    };

    // Inicialización del semáforo
    union semun sem_union;
    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666); // Crear el semáforo
    if (sem_id == -1) {
        perror("Error creando el semáforo");
        exit(1);
    }

    // Inicializar el semáforo a 1 (disponible)
    sem_union.val = 1;
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1) {
        perror("Error inicializando el semáforo");
        exit(1);
    }

    if (argc > 3) {
        fprintf(stderr, "Usage: %s TCP/UDP [usuario[@host]]\n", argv[0]);
        exit(1);
    }

    /* Build the request based on input arguments */
    if (argc == 2) {
        //Limpia peticion
        memset(peticion, 0, TAM_BUFFER);
        snprintf(peticion, TAM_BUFFER, "\r\n");
    } else {
        memset(peticion, 0, TAM_BUFFER);
        snprintf(peticion, TAM_BUFFER, "%s\r\n", argv[2]);
    }

    // Buscamos @ en la cadena
    char *posicion = strchr(peticion, '@');

    if(posicion != NULL) {

        if(peticion[0] == '@') {
            memset(usuario, 0, TAM_BUFFER);
            strcpy(usuario, "\r\n");
        } else {
            // Calcular la longitud del usuario
            int longitud_usuario = posicion - peticion;

            // Copiar el nombre de usuario
            strncpy(usuario, peticion, longitud_usuario);
            usuario[longitud_usuario] = '\0'; // Añadir el terminador nulo
        }

        // Obtener el host
        char *host = posicion + 1;
        memset(hostname, 0, TAM_BUFFER);
        strcpy(hostname, host);
        // Eliminar los caracteres '\r\n' del final de la cadena 'usuario'
		size_t len = strlen(hostname);
		if (len > 0 && (hostname[len - 1] == '\n' || hostname[len - 1] == '\r'))
		{
			hostname[len - 1] = '\0'; // Eliminar el salto de línea '\n'
		}
		if (len > 1 && hostname[len - 2] == '\r')
		{
			hostname[len - 2] = '\0'; // Eliminar el retorno de carro '\r' si existe
		}
        
    } else {
        // Si no hay @, hostname = localhost
        memset(hostname, 0, TAM_BUFFER);
        strcpy(hostname, "localhost");

        // Verificar la peticion es solo \r\n usuario vacío 
        if (strcmp(peticion, "\r\n") == 0) {
            memset(usuario, 0, TAM_BUFFER);
            strcpy(usuario, "\r\n");
        } else { // Si no, usuario = peticion
            memset(usuario, 0, TAM_BUFFER); 
            strcpy(usuario, peticion);
        }
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
        errcode = getaddrinfo(hostname, NULL, &hints, &res); 
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
        sprintf(log, "Connected to %s on port %u at %s", hostname, ntohs(myaddr_in.sin_port), (char *)ctime(&timevar));
        escribir_log(log, ntohs(myaddr_in.sin_port));

        /* Send the request to the server */
        if (send(s, usuario, strlen(usuario), 0) != strlen(usuario)) {
            sprintf(log, "%s: unable to send request\n", argv[0]);
            escribir_log(log, ntohs(myaddr_in.sin_port));
            exit(1);
        }

        /* Now, start receiving all of the replys from the server.
        * This loop will terminate when the recv returns zero,
        * which is an end-of-file condition.  This will happen
        * after the server has sent all of its replies, and closed
        * its end of the connection.
        */
        while (i = recv(s, respuesta_TCP, TAM_BUFFER, 0)) {
            if (i == -1) {
                perror(argv[0]);
                sprintf(log, "%s: error reading result\n", argv[0]);
                escribir_log(log, ntohs(myaddr_in.sin_port));
                exit(1);
            }
            if (strcmp(respuesta_TCP, "\r\n") == 0) {
                break;
            }
            respuesta_TCP[i] = '\0';
            sprintf(log, "%s\n", respuesta_TCP);
            escribir_log(log, ntohs(myaddr_in.sin_port));
        }
        // Cerrar el socket al finalizar la conexión 
        close(s);

        /* Print message indicating completion of task. */
        time(&timevar);
        sprintf(log, "All done at %s", (char *)ctime(&timevar));
        escribir_log(log, ntohs(myaddr_in.sin_port));

        // Eliminar el semáforo
        if (semctl(sem_id, 0, IPC_RMID, 0) == -1) {
            perror("Error eliminando el semáforo");
        }

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

        // Mandar msj vacio al servidor para que nos devuelva el puerto
        char msj_vacio[TAM_BUFFER];
        memset(msj_vacio, 0, TAM_BUFFER);
        snprintf(msj_vacio, TAM_BUFFER, " ");
        if (sendto(s, msj_vacio, strlen(msj_vacio), 0, (struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) {
            perror(argv[0]);
            fprintf(stderr, "%s: unable to send request\n", argv[0]);
            exit(1);
        }
        // Recibirpuerto por el que se va a comunicar
        if ((cc = recvfrom(s, buf, TAM_BUFFER-1, 0, (struct sockaddr *)&servaddr_in, &addrlen)) == -1) {
            perror(argv[0]);
            fprintf(stderr, "%s: unable to get response\n", argv[0]);
            exit(1);
        }
        // Actualizar el puerto 
        int nuevoPuerto = atoi(buf);
        servaddr_in.sin_port = htons(nuevoPuerto);

        while (n_retry > 0) {
      
            if (sendto(s, usuario, strlen(usuario), 0, (struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) {
                perror(argv[0]);
                sprintf(log, "%s: unable to send request\n", argv[0]);
                escribir_log(log, ntohs(servaddr_in.sin_port));
                exit(1);
            }

            alarm(TIMEOUT);

            while (1 && n_retry > 0) {
                if ((cc = recvfrom(s, respuesta_UDP, TAM_BUFFER-1, 0, (struct sockaddr *)&servaddr_in, &addrlen)) == -1) {
                    if (errno == EINTR) {
                        sprintf(log, "Attempt %d (retries %d).\n", RETRIES - n_retry + 1, RETRIES);
                        escribir_log(log, ntohs(servaddr_in.sin_port));
                        n_retry--;
                    } else {
                        perror(argv[0]);
                        sprintf(log, "%s: unable to get response\n", argv[0]);
                        escribir_log(log, ntohs(servaddr_in.sin_port));
                        exit(1);
                    }
                } else {
                    alarm(0);
                    respuesta_UDP[cc] = '\0';
                    sprintf(log, "%s\n", respuesta_UDP);
                    escribir_log(log, ntohs(servaddr_in.sin_port));
                    if(strcmp(respuesta_UDP, "\r\n") == 0){
                        break;
                    }
                }
            }
            break;
        }

        if (n_retry == 0) {
            sprintf(log, "Unable to get response from %s after %d attempts.\n", hostname, RETRIES);
            escribir_log(log, ntohs(servaddr_in.sin_port));
        }
        // Cerrar el socket al finalizar la conexión
        close(s);

        // Eliminar el semáforo
        if (semctl(sem_id, 0, IPC_RMID, 0) == -1) {
            perror("Error eliminando el semáforo");
        }

    } else {
        fprintf(stderr, "Error de comparación");
        fprintf(stderr, "Usage: %s TCP/UDP [usuario[@host]]\n", argv[0]);
        exit(1);
    }

    return 0;
}
