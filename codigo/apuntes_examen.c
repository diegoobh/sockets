//APUNTES DE SOCKETS

//                              Cliente                 Servidor
//                         ---------------------   -----------------------
//                          TCP     |   UDP          TCP    |   UDP
// 1. Crear socket         socket()   socket()     socket()    socket()
// 2. Asociar socket       -          bind()       bind()      bind()
// 3. Conectar socket      connect()  -            listen()    -
// 4. Escuchar socket      -          -            accept()    -
// 5. Enviar datos         send()     sendto()     send()      sendto()
// 6. Recibir datos        recv()     recvfrom()   recv()      recvfrom()
// 7. Cerrar socket        close()    close()      close()     close()


// SINTAXIS DEL CLIENTE 
buff[TAMBUFFER]; 
dir_local.familia = AF_INET // Para IPv4
dir_local.ip = INADDR_ANY // Cualquier ip 
dir_local.puerto = 0 // El sistema te da un puerto libre

socket = crearSocket() -> AF_INET, | TCP -> SOCK_STREAM
                                   | UDP -> SOCK_DGRAM

bind() -> asociar SOCKET UDP a dir_local

dir_servidor.familia = AF_INET
dir_servidor.ip = getaddrinfo()
dir_servidor.puerto = htons(PUERTO)

connect() -> conectar SOCKET TCP a dir_servidor

//                 UDP                   |                   TCP      
//----------------------------------------------------------------------------
Manejadora de SIGALARM                   | send(socket, buff)
n_retry = RETRIES                        | recv(socket, buff)
mientras n_retry > 0                     | 
    sendto(buff, &dir_servidor)          | Si es -1 
    alarm(TIMEOUT)                       |    mostrar ERROR al enviar
    recvfrom(buff, &dir_servidor)        | sino 
                                         |    mostrar RESPUESTA del Servidor
    Si hay error                         | Fin si
        Si salta alarma                  |
            n_retry--                    |
        sino                             |
            mostrar ERROR al recibir     |
        fin si                           |
    sino                                 |
        alarma(0)                        |
        mostrar RESPUESTA del Servidor   |
        break                            |
    Fin si                               |
Fin mientras                             |
                                         |
Si n_retry == 0                          |
    mostrar ERROR sin intentos           |
Fin si                                   |


// SINTAXIS DEL SERVIDOR
buff[TAMBUFFER];
dir_local.familia = AF_INET // Para IPv4
dir_local.ip = INADDR_ANY // Cualquier ip
dir_local.puerto = htons(PUERTO)

socket = crearSocket() -> AF_INET, | TCP -> SOCK_STREAM
                                   | UDP -> SOCK_DGRAM

bind(socket, &dir_local)
listen(socket, N_SOCKETS) // Solo para el TCP
struct fd_set // Definir el conjunto de sockets 
FD_SET(socket, &fd_set) // Añadir el socket al conjunto de sockets 

Bucle infinito 
    select() -> fd_set 
//                 UDP                          |         TCP      
//--------------------------------------------------------------------------------------------
recvfrom(socket, buff, &dir_cliente)            | socket = accept(ls, &dir_cliente)
                                                | //Si es con varios clientes, hacemos fork()
addrlen = sizeof(dir_cliente)                   | 
                                                | mientras recv(socket, buff)
getaddrinfo()                                   |   Si == -1
Si != 0                                         |       mostrar ERROR al recibir
    mostrar ADDRNOTFOUND                        |   Fin si
sino                                            | Fin mientras
    reqaddr = dir_cliente.ip                    | 
Fin si                                          | send(socket, buff) // Según lo que se necesite
                                                | // Este send puede ir dentro o fuera del bucle
sendto(socket, &reqaddr, &dir_cliente, addrlen) | 
                                                |
Fin bucle infinito                              |