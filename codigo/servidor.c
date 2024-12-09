/*
** Fichero: servidor.c
** Autores:
** Diego Borrallo Herrero DNI 49367527M
** Jaime Castellanos Rubio DNI 71047797S
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
#define ADDRNOTFOUND 0xffffffff /* return address for unfound host */
#define TAM_BUFFER 516
#define MAXHOST 128
#define TIMEOUT 60

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
void serverUDP(int s, char *buffer, struct sockaddr_in clientaddr_in);
void errout(char *); /* declare error out routine */
void procesar_peticion_TCP(int s, char *buf);
void procesar_peticion_UDP(int s, char *buf, struct sockaddr_in clientaddr_in, int addrlen);

int FIN = 0; /* Para el cierre ordenado */
void finalizar() { FIN = 1; }
void morir() { FIN = 1; }

// Devuelve la informacion formateada del usuario pasado por parametro
char *devuelveinf(char *user)
{
	char login[TAM_BUFFER];
	char name[TAM_BUFFER];
	char directory[TAM_BUFFER];
	char shell[TAM_BUFFER];
	char tty[TAM_BUFFER];
	char ip[TAM_BUFFER];
	char date[TAM_BUFFER];
	char time[TAM_BUFFER];
	char mail[TAM_BUFFER] = "No mail.";
	char plan[TAM_BUFFER] = "No plan.";
	char infoConexion[TAM_BUFFER];
	char idleTime[TAM_BUFFER];
	// Obtener información del usuario.
	char comando[TAM_BUFFER];
	char salida[TAM_BUFFER];
	char linea[TAM_BUFFER];
	char *respuesta = (char *)malloc(TAM_BUFFER);

	FILE *fp;
	memset(comando, 0, TAM_BUFFER);
	snprintf(comando, TAM_BUFFER, "getent passwd %s", user);
	if ((fp = popen(comando, "r")) == NULL)
	{
		printf("Error al ejecutar el comando getent.\n");
		return NULL;
	}
	// Leer la salida del comando
	if (fgets(linea, TAM_BUFFER, fp) != NULL)
	{ // Usuario encontrado
		// Copiar el contenido de la linea para no modificar la original
		char contenido[TAM_BUFFER];
		memset(contenido, 0, TAM_BUFFER);
		strncpy(contenido, linea, TAM_BUFFER);

		// Eliminar el salto de línea al final de la línea
		contenido[strcspn(contenido, "\n")] = '\0'; // Eliminar el '\n'

		// Obtener el primer campo de la línea (usuario) hasta el primer ':'
		char *usuario_linea = strtok(contenido, ":");

		if (strcmp(usuario_linea, user) == 0)
		{
			memset(salida, 0, TAM_BUFFER);
			strncpy(salida, linea, TAM_BUFFER);
		}
	}
	else
	{ // No se encuentra el usuario
		pclose(fp);
		memset(respuesta, 0, TAM_BUFFER);
		snprintf(respuesta, TAM_BUFFER, "%s: no such user\n", user);
		return respuesta;
	}

	pclose(fp);
	// Obtener los campos de la salida:
	char *separador;
	if ((separador = strtok(salida, ":")) != NULL)
	{
		memset(login, 0, TAM_BUFFER);
		strncpy(login, separador, TAM_BUFFER);
	}
	else
	{
		printf("Error al obtener el login del usuario.\n");
		return NULL;
	}
	// Saltar los campos que no nos interesan
	for (int i = 0; i < 4; i++)
		separador = strtok(NULL, ":");

	// Quinto campo: nombre
	if (separador != NULL)
	{
		memset(name, 0, TAM_BUFFER);
		strncpy(name, separador, TAM_BUFFER);
	}
	else
	{
		printf("Error al obtener el nombre del usuario.\n");
		return NULL;
	}
	// Sexto campo: directorio;
	if ((separador = strtok(NULL, ":")) != NULL)
	{
		memset(directory, 0, TAM_BUFFER);
		strncpy(directory, separador, TAM_BUFFER);
	}
	else
	{
		printf("Error al obtener el directorio del usuario.\n");
		return NULL;
	}
	// Séptimo campo: shell;
	if ((separador = strtok(NULL, ":")) != NULL)
	{
		memset(shell, 0, TAM_BUFFER);
		strncpy(shell, separador, TAM_BUFFER);
	}
	else
	{
		printf("Error al obtener el shell del usuario.\n");
		return NULL;
	}
	// Obtener la información de la conexión del usuario
	memset(comando, 0, TAM_BUFFER);
	snprintf(comando, TAM_BUFFER, "lastlog -u %s", user);
	if ((fp = popen(comando, "r")) == NULL)
	{
		printf("Error al ejecutar el comando lastlog.\n");
		return NULL;
	}
	// Ignorar la primera línea (encabezado)
	memset(salida, 0, TAM_BUFFER);
	fgets(salida, TAM_BUFFER, fp);
	// Leer la salida del comando
	if (fgets(salida, TAM_BUFFER, fp) == NULL)
	{
		printf("Error al leer la salida de lastlog.\n");
		pclose(fp);
		return NULL;
	}
	pclose(fp);
	// Parsear la línea obtenida
	separador = strtok(salida, " \t"); // Ignorar el campo del Login
	// Segundo campo (TTY)
	if ((separador = strtok(NULL, " \t")) != NULL)
	{
		memset(tty, 0, TAM_BUFFER);
		strncpy(tty, separador, TAM_BUFFER);
	}
	else
	{
		printf("Error al obtener el TTY del usuario.\n");
		return NULL;
	}
	// Tercer campo (IP)
	if ((separador = strtok(NULL, " \t")) != NULL)
	{
		memset(ip, 0, TAM_BUFFER);
		strncpy(ip, separador, TAM_BUFFER);
	}
	else
	{
		printf("Error al obtener la IP del usuario.\n");
		return NULL;
	}
	// Cuarto campo (Time)
	if ((separador = strtok(NULL, "\n")) != NULL)
	{
		memset(date, 0, TAM_BUFFER);
		strncpy(date, separador, TAM_BUFFER);
	}
	else
	{
		printf("Error al obtener la fecha del usuario.\n");
		return NULL;
	}
	// Formatear la fecha para que solo incluya hasta los minutos (HH:MM)
	int longitudFecha = strcspn(date, ":") + 3;
	strncpy(time, date, longitudFecha);
	time[longitudFecha] = '\0'; // Asegurar que termina en null

	memset(comando, 0, TAM_BUFFER);
	snprintf(comando, TAM_BUFFER, "w | grep %s", user);
	if ((fp = popen(comando, "r")) == NULL)
	{
		printf("Error al ejecutar el comando w\n");
		return NULL;
	}
	memset(salida, 0, TAM_BUFFER);
	// Leer la salida del comando
	if (fgets(salida, TAM_BUFFER, fp) == NULL)
	{ // El usuario no esta conectado
		pclose(fp);
		memset(idleTime, 0, TAM_BUFFER);
		snprintf(idleTime, TAM_BUFFER, "0:00");
	}
	else
	{ // El usuario esta conectado
		pclose(fp);
		// Extraer el valor de IDLE
		separador = strtok(salida, " ");
		int i = 0;
		while (separador != NULL)
		{
			i++;
			if (i == 5)
			{ // La columna IDLE está en la quinta posición
				memset(idleTime, 0, TAM_BUFFER);
				strncpy(idleTime, separador, TAM_BUFFER - 1);
				idleTime[TAM_BUFFER - 1] = '\0'; // Asegurar que termina en null
				break;
			}
			separador = strtok(NULL, " ");
		}
	}

	// Construir la respuesta.
	memset(infoConexion, 0, TAM_BUFFER);
	snprintf(infoConexion, TAM_BUFFER, "On since %s on %s from %s", time, tty, ip);
	memset(respuesta, 0, TAM_BUFFER);
	snprintf(respuesta, TAM_BUFFER, "\nLogin: %s\t\t\t\tName: %s\nDirectory: %s\t\tShell: %s\nOffice: -\t\t\t\tHome Phone: -\n %s\n idle %s\n %s\n %s\r\n",
			 login, name, directory, shell,
			 infoConexion, idleTime, mail, plan);
	return respuesta;
}

// Se encarga de procesar las peticiones de procesos TCP
void procesar_peticion_TCP(int s, char *usuario)
{
	char *infoUsuario=NULL;
	char comando[TAM_BUFFER];
	char salida[TAM_BUFFER];
	int leido = 0;

	if (strcmp(usuario, "\r\n") != 0)
	{ // Petición no vacía
		
		// Eliminar los caracteres '\r\n' del final de la cadena 'usuario'
		size_t len = strlen(usuario);
		if (len > 0 && (usuario[len - 1] == '\n' || usuario[len - 1] == '\r'))
		{
			usuario[len - 1] = '\0'; // Eliminar el salto de línea '\n'
		}
		if (len > 1 && usuario[len - 2] == '\r')
		{
			usuario[len - 2] = '\0'; // Eliminar el retorno de carro '\r' si existe
		}
		
		// Verificar si es nombre o login
		FILE *fp;
		memset(comando, 0, TAM_BUFFER);
		snprintf(comando, TAM_BUFFER, "getent passwd | grep %s", usuario);
		if ((fp = popen(comando, "r")) == NULL)
		{
			printf("Error al ejecutar el comando getent.\n");
			return;
		}
		while (!leido)
		{
			if (fgets(salida, TAM_BUFFER, fp) != NULL)
			{
				// Eliminar el salto de línea al final de la línea
				salida[strcspn(salida, "\n")] = '\0'; // Eliminar el '\n'

				// Obtener el primer campo de la línea (usuario) hasta el primer ':'
				char *usuario_linea = strtok(salida, ":");

				if (strcmp(usuario_linea, usuario) == 0)
				{
					leido = 1;
					infoUsuario = devuelveinf(usuario);

					if (send(s, infoUsuario, strlen(infoUsuario), 0) != strlen(infoUsuario))
					{
						fprintf(stderr, "Servidor: Error al enviar respuesta al cliente\n");
					}
				}
				else
				{
					infoUsuario = devuelveinf(usuario_linea);

					if (send(s, infoUsuario, strlen(infoUsuario), 0) != strlen(infoUsuario))
					{
						fprintf(stderr, "Servidor: Error al enviar respuesta al cliente\n");
					}
				}
				if (feof(fp))
				{
					leido = 1;
				}
			}
			else
			{	
				infoUsuario = strcat(usuario, ": no such user or no more users to find.");
				if (send(s, infoUsuario, strlen(infoUsuario), 0) != strlen(infoUsuario))
				{
					fprintf(stderr, "Servidor: Error al enviar respuesta al cliente\n");
				}

				leido = 1;
			}
		}
	}
	else
	{ // Petición vacía
		char linea[TAM_BUFFER];

		// // Obtener información de todos los usuarios.
		FILE *fp;
		if ((fp = popen("who", "r")) == NULL)
		{
			printf("Error al ejecutar el comando who.\n");
			return;
		}

		// Leer la salida del comando y coger el primer campo (login)
		memset(linea, 0, TAM_BUFFER);
		while (fgets(linea, TAM_BUFFER, fp) != NULL)
		{
			// Extraer el primer campo (login)
			char *user = strtok(linea, " ");
			if (user != NULL)
			{
				// Obtener datos del usuario
				infoUsuario = devuelveinf(user);

				if (send(s, infoUsuario, strlen(infoUsuario), 0) != strlen(infoUsuario))
				{
					fprintf(stderr, "Servidor: Error al enviar respuesta al cliente\n");
				}
			}
		}
	}

	// Enviar mensaje de fin de conexion
	sprintf(infoUsuario, "\r\n");
	if (send(s, infoUsuario, strlen(infoUsuario), 0) != strlen(infoUsuario))
	{
		fprintf(stderr, "Servidor: Error al enviar respuesta al cliente\n");
	}
}

// Se encarga de procesar las peticiones de procesos UDP
void procesar_peticion_UDP(int s, char *usuario, struct sockaddr_in clientaddr_in, int addrlen)
{
	char *infoUsuario = NULL;
	int nc;
	int leido = 0;
	char comando[TAM_BUFFER];
	char salida[TAM_BUFFER];
	
	if (strcmp(usuario, "\r\n") != 0)
	{ // Petición no vacía
		// Eliminar los caracteres '\r\n' del final de la cadena 'usuario'
		size_t len = strlen(usuario);
		if (len > 0 && (usuario[len - 1] == '\n' || usuario[len - 1] == '\r'))
		{
			usuario[len - 1] = '\0'; // Eliminar el salto de línea '\n'
		}
		if (len > 1 && usuario[len - 2] == '\r')
		{
			usuario[len - 2] = '\0'; // Eliminar el retorno de carro '\r' si existe
		}

		// Verificar si es nombre o login
		FILE *fp;
		memset(comando, 0, TAM_BUFFER);
		snprintf(comando, TAM_BUFFER, "getent passwd | grep %s", usuario);
		if ((fp = popen(comando, "r")) == NULL)
		{
			printf("Error al ejecutar el comando getent.\n");
			return;
		}
		while (!leido)
		{
			if (fgets(salida, TAM_BUFFER, fp) != NULL)
			{
				// Eliminar el salto de línea al final de la línea
				salida[strcspn(salida, "\n")] = '\0'; // Eliminar el '\n'

				// Obtener el primer campo de la línea (usuario) hasta el primer ':'
				char *usuario_linea = strtok(salida, ":");

				if (strcmp(usuario_linea, usuario) == 0)
				{
					leido = 1;
					infoUsuario = devuelveinf(usuario);

					nc = sendto(s, infoUsuario, strlen(infoUsuario), 0, (struct sockaddr *)&clientaddr_in, addrlen);
					if (nc == -1)
					{
						perror("serverUDP");
						printf("%s: sendto error\n", "serverUDP");
						return;
					}
				}
				else
				{
					infoUsuario = devuelveinf(usuario_linea);

					nc = sendto(s, infoUsuario, strlen(infoUsuario), 0, (struct sockaddr *)&clientaddr_in, addrlen);
					if (nc == -1)
					{
						perror("serverUDP");
						printf("%s: sendto error\n", "serverUDP");
						return;
					}
				}
				if (feof(fp))
				{
					leido = 1;
				}
			}
			else
			{
				infoUsuario = strcat(usuario, ": no such user or no more users to find.");
				nc = sendto(s, infoUsuario, strlen(infoUsuario), 0, (struct sockaddr *)&clientaddr_in, addrlen);
				if (nc == -1)
				{
					perror("serverUDP");
					printf("%s: sendto error\n", "serverUDP");
					return;
				}
				leido = 1;
			}
		}
	}
	else
	{ // Petición vacía
		char linea[TAM_BUFFER];

		// // Obtener información de todos los usuarios.
		FILE *fp;
		if ((fp = popen("who", "r")) == NULL)
		{
			printf("Error al ejecutar el comando who.\n");
			return;
		}

		// Leer la salida del comando y coger el primer campo (login)
		memset(linea, 0, TAM_BUFFER);
		while (fgets(linea, TAM_BUFFER, fp) != NULL)
		{
			// Extraer el primer campo (login)
			char *user = strtok(linea, " ");
			if (user != NULL)
			{
				// Obtener datos del usuario
				infoUsuario = devuelveinf(user);

				nc = sendto(s, infoUsuario, strlen(infoUsuario), 0, (struct sockaddr *)&clientaddr_in, addrlen);
				if (nc == -1)
				{
					perror("serverUDP");
					printf("%s: sendto error\n", "serverUDP");
					return;
				}
			}
			memset(linea, 0, TAM_BUFFER);
		}
	}

	// Enviar mensaje de fin de conexion
	sprintf(infoUsuario, "\r\n");
	nc = sendto(s, infoUsuario, strlen(infoUsuario), 0, (struct sockaddr *)&clientaddr_in, addrlen);
	if (nc == -1)
	{
		perror("serverUDP");
		printf("%s: sendto error\n", "serverUDP");
		return;
	}
}

int main(argc, argv)
int argc;
char *argv[];
{

	int s_TCP, s_UDP; /* connected socket descriptor */
	int ls_TCP;		  /* listen socket descriptor */

	int s_UDP_NUEVO; /* connected socket descriptor */
	struct sockaddr_in sUDPnuevo;

	int cc; /* contains the number of bytes read */

	struct sigaction sa = {.sa_handler = SIG_IGN}; /* used to ignore SIGCHLD */

	struct sockaddr_in myaddr_in;	  /* for local socket address */
	struct sockaddr_in clientaddr_in; /* for peer socket address */
	int addrlen;

	fd_set readmask;
	int numfds, s_mayor;

	char buffer[TAM_BUFFER]; /* buffer for packets to be read into */

	struct sigaction vec;
	struct sigaction vec2;

	/* Create the listen socket. */
	ls_TCP = socket(AF_INET, SOCK_STREAM, 0);
	if (ls_TCP == -1)
	{
		perror(argv[0]);
		fprintf(stderr, "%s: unable to create socket TCP\n", argv[0]);
		exit(1);
	}
	/* clear out address structures */
	memset((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
	memset((char *)&clientaddr_in, 0, sizeof(struct sockaddr_in));

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
	if (bind(ls_TCP, (const struct sockaddr *)&myaddr_in, sizeof(struct sockaddr_in)) == -1)
	{
		perror(argv[0]);
		fprintf(stderr, "%s: unable to bind address TCP\n", argv[0]);
		exit(1);
	}
	/* Initiate the listen on the socket so remote users
	 * can connect.  The listen backlog is set to 5, which
	 * is the largest currently supported.
	 */
	if (listen(ls_TCP, 5) == -1)
	{
		perror(argv[0]);
		fprintf(stderr, "%s: unable to listen on socket\n", argv[0]);
		exit(1);
	}

	/* Create the socket UDP. */
	s_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	if (s_UDP == -1)
	{
		perror(argv[0]);
		printf("%s: unable to create socket UDP\n", argv[0]);
		exit(1);
	}
	/* Bind the server's address to the socket. */
	if (bind(s_UDP, (struct sockaddr *)&myaddr_in, sizeof(struct sockaddr_in)) == -1)
	{
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

	switch (fork())
	{
	case -1: /* Unable to fork, for some reason. */
		perror(argv[0]);
		fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
		exit(1);

	case 0: /* The child process (daemon) comes here. */

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
		if (sigaction(SIGCHLD, &sa, NULL) == -1)
		{
			perror(" sigaction(SIGCHLD)");
			fprintf(stderr, "%s: unable to register the SIGCHLD signal\n", argv[0]);
			exit(1);
		}

		/* Registrar SIGTERM para la finalizacion ordenada del programa servidor */
		vec.sa_handler = (void *)finalizar;
		vec.sa_flags = 0;
		if (sigaction(SIGTERM, &vec, (struct sigaction *)0) == -1)
		{
			perror(" sigaction(SIGTERM)");
			fprintf(stderr, "%s: unable to register the SIGTERM signal\n", argv[0]);
			exit(1);
		}

		// Registrar alarma cont timeout de 60 segundos para que finalice si no se ejecuta un cliente 
		vec.sa_handler = (void *)morir;
		vec.sa_flags = 0;
		if (sigaction(SIGALRM, &vec, (struct sigaction *)0) == -1)
		{
			perror(" sigaction(SIGALRM)");
			fprintf(stderr, "%s: unable to register the SIGALRM signal\n", argv[0]);
			exit(1);
		}

		while (!FIN)
		{
			alarm(TIMEOUT); // Iniciar el timeout
			/* Meter en el conjunto de sockets los sockets UDP y TCP */
			FD_ZERO(&readmask);
			FD_SET(ls_TCP, &readmask);
			FD_SET(s_UDP, &readmask);
			/*
			Seleccionar el descriptor del socket que ha cambiado. Deja una marca en
			el conjunto de sockets (readmask)
			*/
			if (ls_TCP > s_UDP)
				s_mayor = ls_TCP;
			else
				s_mayor = s_UDP;

			if ((numfds = select(s_mayor + 1, &readmask, (fd_set *)0, (fd_set *)0, NULL)) < 0)
			{
				if (errno == EINTR)
				{
					FIN = 1;
					close(ls_TCP);
					close(s_UDP);
					perror("\nFinalizando el servidor. Se�al recibida en elect\n ");
				}
			}
			else
			{

				/* Comprobamos si el socket seleccionado es el socket TCP */
				if (FD_ISSET(ls_TCP, &readmask))
				{
					alarm(0); // Reiniciar el timeout
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
					s_TCP = accept(ls_TCP, (struct sockaddr *)&clientaddr_in, &addrlen);
					if (s_TCP == -1)
						exit(1);
					switch (fork())
					{
					case -1: /* Can't fork, just exit. */
						exit(1);
					case 0:			   /* Child process comes here. */
						close(ls_TCP); /* Close the listen socket inherited from the daemon. */
						serverTCP(s_TCP, clientaddr_in);
						exit(0);
					default: /* Daemon process comes here. */
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
				if (FD_ISSET(s_UDP, &readmask))
				{
					alarm(0); // Reiniciar el timeout 
					/* This call will block until a new
					 * request arrives.  Then, it will
					 * return the address of the client,
					 * and a buffer containing its request.
					 * TAM_BUFFER - 1 bytes are read so that
					 * room is left at the end of the buffer
					 * for a null character.
					 */
					cc = recvfrom(s_UDP, buffer, TAM_BUFFER - 1, 0, (struct sockaddr *)&clientaddr_in, &addrlen);
					if (cc == -1)
					{
						perror(argv[0]);
						printf("%s: recvfrom error\n", argv[0]);
						exit(1);
					}

					s_UDP_NUEVO = socket(AF_INET, SOCK_DGRAM, 0);
					if (s_UDP_NUEVO == -1)
					{
						printf("Error creando socket UDP\n");
						exit(1);
					}

					sUDPnuevo = myaddr_in;
					sUDPnuevo.sin_port = 0;
					if (bind(s_UDP_NUEVO, (struct sockaddr *)&sUDPnuevo, sizeof(struct sockaddr_in)) == -1)
					{
						printf("Error en bind\n");
						exit(1);
					}

					addrlen = sizeof(struct sockaddr_in);
					if (getsockname(s_UDP_NUEVO, (struct sockaddr *)&sUDPnuevo, &addrlen) == -1)
					{
						printf("Error en getsockname\n");
						exit(1);
					}

					switch (fork())
					{
					case -1: /* Can't fork, just exit. */
						exit(1);
					case 0: /* Child process comes here. */
						sprintf(buffer, "%d", ntohs(sUDPnuevo.sin_port));
						if (sendto(s_UDP, buffer, TAM_BUFFER, 0, (struct sockaddr *)&clientaddr_in, addrlen) == -1)
						{
							printf("Error en sendto\n");
							perror("sendto");
							exit(1);
						}
						close(s_UDP); /* Close the listen socket inherited from the daemon. */
						serverUDP(s_UDP_NUEVO, buffer, clientaddr_in);
						exit(0);
					default: /* Daemon process comes here. */
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
						close(s_UDP_NUEVO);
					}
				} /* UDP */
			}
		} /* Fin del bucle infinito de atenci�n a clientes */
		/* Cerramos los sockets UDP y TCP */
		close(ls_TCP);
		close(s_UDP);

		printf("\nFin de programa servidor!\n");

	default: /* Parent process comes here. */
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
	int reqcnt = 0;			/* keeps count of number of requests */
	char buf[TAM_BUFFER];	/* This example uses TAM_BUFFER byte messages. */
	char hostname[MAXHOST]; /* remote host's name string */

	int len, status;
	struct hostent *hp; /* pointer to host info for remote host */
	long timevar;		/* contains time returned by time() */

	struct linger linger; /* allow a lingering, graceful close; */
						  /* used when setting SO_LINGER */

	/* Look up the host information for the remote host
	 * that we have connected with.  Its internet address
	 * was returned by the accept call, in the main
	 * daemon loop above.
	 */

	status = getnameinfo((struct sockaddr *)&clientaddr_in, sizeof(clientaddr_in),
						 hostname, MAXHOST, NULL, 0, 0);
	if (status)
	{
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
	time(&timevar);
	/* The port number must be converted first to host byte
	 * order before printing.  On most hosts, this is not
	 * necessary, but the ntohs() call is included here so
	 * that this program could easily be ported to a host
	 * that does require it.
	 */
	printf("Startup from %s port %u at %s",
		   hostname, ntohs(clientaddr_in.sin_port), (char *)ctime(&timevar));

	/* Set the socket for a lingering, graceful close.
	 * This will cause a final close of this socket to wait until all of the
	 * data sent on it has been received by the remote host.
	 */
	linger.l_onoff = 1;
	linger.l_linger = 1;
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &linger,
				   sizeof(linger)) == -1)
	{
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
	memset(buf, 0, TAM_BUFFER);
	while (len = recv(s, buf, TAM_BUFFER, 0))
	{
		// if (len == -1)
		// 	errout(hostname);//(Quitamos el error) para que no salga por pantalla

		procesar_peticion_TCP(s, buf);

		close(s);

		reqcnt++;
	}

	/* Log a finishing message. */
	time(&timevar);
	/* The port number must be converted first to host byte
	 * order before printing.  On most hosts, this is not
	 * necessary, but the ntohs() call is included here so
	 * that this program could easily be ported to a host
	 * that does require it.
	 */
	printf("Completed %s port %u, %d requests, at %s\n",
		   hostname, ntohs(clientaddr_in.sin_port), reqcnt, (char *)ctime(&timevar));
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
void serverUDP(int s, char *buffer, struct sockaddr_in clientaddr_in)
{
	struct in_addr reqaddr; /* for requested host's address */
	struct hostent *hp;		/* pointer to host info for requested host */

	struct addrinfo hints, *res;

	int addrlen;

	addrlen = sizeof(struct sockaddr_in);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;

	// Limpiar el buffer
	memset(buffer, 0, TAM_BUFFER);

	// Recibir el mensaje del cliente
	int nc = recvfrom(s, buffer, TAM_BUFFER, 0, (struct sockaddr *)&clientaddr_in, &addrlen);
	if (nc == -1)
	{
		perror("serverUDP");
		printf("%s: recvfrom error\n", "serverUDP");
		return;
	}

	// Copiar el buffer en una variable local
	char usuario[TAM_BUFFER];
	memset(usuario, 0, TAM_BUFFER);
	strncpy(usuario, buffer, TAM_BUFFER);

	procesar_peticion_UDP(s, usuario, clientaddr_in, addrlen);

	close(s);
}
