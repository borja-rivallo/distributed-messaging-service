#include <stdio.h>      
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include "servidor_registro.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "servicio_mensajeria.h"
#include <netdb.h>

// Mutex para proteger la estructura de la lista enlazada concurrente
pthread_mutex_t mutex_claves;

// Funciónn auxiliar para registrar la acción en el servidor RPC
void registrar_accion_rpc(char *nombre, char *accion, char *fichero) {
	CLIENT *clnt;
    enum clnt_stat retval;
    char *result;
    
    // Obtenemos la IP desde la variable de entorno
    char *rpc_ip = getenv("LOG_RPC_IP");
    if (rpc_ip == NULL) {
        printf("ERROR: LOG_RPC_IP no definida");
        return;
    }

    // Crear el cliente RPC usando la IP dinámica
    clnt = clnt_create(rpc_ip, PROGRAMA_REGISTRO, VERSION_REGISTRO, "tcp");
    if (clnt == NULL) {
        clnt_pcreateerror(rpc_ip);
        return; 
    }

    // Llamar a la función remota
    retval = registrar_accion_1(nombre, accion, fichero, (void *)&result, clnt);
    if (retval != RPC_SUCCESS) {
        clnt_perror(clnt, rpc_ip);
    }

    // Destruir el cliente RPC
    clnt_destroy(clnt);
}

void leer_cadena(int sc, char *buffer) {
    // Función auxiliar para la lectura de mensajes
    char c;
    int i = 0;
    while (read(sc, &c, 1) > 0) {
        buffer[i++] = c;
        if (c == '\0') {
            break;
        }
    }
}

void enviar_mensaje_cliente(const char *ip, int puerto, const char *remitente, 
                            unsigned int id, const char *contenido) {
    // Función auxiliar para enviar un mensaje sin fichero a un cliente

    // Configuramos la dirección del cliente
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(puerto);

    // Convertimos la dirección IP del cliente de formato string a formato de red
    inet_pton(AF_INET, ip, &client_addr.sin_addr);

    if (connect(sd, (struct sockaddr *)&client_addr, sizeof(client_addr)) == 0) {
        write(sd, "SEND MESSAGE\0", 13);
        write(sd, remitente, strlen(remitente) + 1);
        char id_str[16];
        sprintf(id_str, "%u", id);
        write(sd, id_str, strlen(id_str) + 1);
        write(sd, contenido, strlen(contenido) + 1);
    }
    close(sd);
}

void enviar_mensaje_attach_cliente(const char *ip, int puerto, const char *remitente, 
                                   unsigned int id, const char *contenido, const char *fichero) {
    // Función auxiliar para enviar un mensaje con fichero a un cliente

    // Configuramos la dirección del cliente
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(puerto);

    // Convertimos la dirección IP del cliente de formato string a formato de red
    inet_pton(AF_INET, ip, &client_addr.sin_addr);

    if (connect(sd, (struct sockaddr *)&client_addr, sizeof(client_addr)) == 0) {
        write(sd, "SEND MESSAGE_ATTACH\0", 20);
        write(sd, remitente, strlen(remitente) + 1);
        char id_str[16];
        sprintf(id_str, "%u", id);
        write(sd, id_str, strlen(id_str) + 1);
        write(sd, contenido, strlen(contenido) + 1);
        write(sd, fichero, strlen(fichero) + 1);
    }
    close(sd);
}

void notificar_ack_remitente(const char *remitente, unsigned int id) {
    // Función auxiliar para notificar al remitente que su mensaje ha sido entregado
    char ip_rem[16];
    int puerto_rem;
    
    // Obtenemos la IP y el puerto del remitente para enviarle el mensaje de ACK
    if (obtener_ip_puerto_usuario((char*)remitente, ip_rem, &puerto_rem)) {
        // Configuramos la dirección del remitente
        int sd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(puerto_rem);
        
        // Convertimos la dirección IP del remitente de formato string a formato de red
        inet_pton(AF_INET, ip_rem, &addr.sin_addr);

        // Intentamos conectar y enviar el mensaje de ACK
        if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            write(sd, "SEND MESS_ACK\0", 14);
            char id_str[16];
            sprintf(id_str, "%u", id);
            write(sd, id_str, strlen(id_str) + 1);
        }
        close(sd);
    }
}

void notificar_ack_attach_remitente(const char *remitente, unsigned int id, const char *fichero) {
    // Función auxiliar para notificar al remitente que su mensaje ha sido entregado
    char ip_rem[16];
    int puerto_rem;
    
    // Obtenemos la IP y el puerto del remitente para enviarle el mensaje de ACK
    if (obtener_ip_puerto_usuario((char*)remitente, ip_rem, &puerto_rem)) {
        // Configuramos la dirección del remitente
        int sd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(puerto_rem);
        
        // Convertimos la dirección IP del remitente de formato string a formato de red
        inet_pton(AF_INET, ip_rem, &addr.sin_addr);

        // Intentamos conectar y enviar el mensaje de ACK
        if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            write(sd, "SEND_MESS_ATTACH_ACK\0", 21);
            char id_str[16];
            sprintf(id_str, "%u", id);
            write(sd, id_str, strlen(id_str) + 1);
            write(sd, fichero, strlen(fichero) + 1);
        }
        close(sd);
    }
}

// Hilo que tratará cada conexión individualmente
void *tratar_cliente(void *arg) {
    // Primero copiamos el descriptor para poder liberar la memoria del original
    int sc = *((int *)arg);
    free(arg);

    // La primera cadena es la operación y es lo primero que el servidor debe leer
    char op_str[256];
    leer_cadena(sc, op_str);

    // Respuesta inicializada en 2 por si acaso hay algún error general
    char res = 2;

    // La lógica a partir de aquí dependerá del código de operación comenzando la sección crítica
    pthread_mutex_lock(&mutex_claves);

    if (strcmp(op_str, "REGISTER") == 0) {
        // En el caso de la operación REGISTER esperamos leer el nombre a continuación
        char nombre[256];
        leer_cadena(sc, nombre);

        // Tenemos que obtener la IP del cliente para el registro y para ello comenzamos inicializando la estructura
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);

        // Obtenemos la dirección del cliente al otro lado de la conexión
        getpeername(sc, (struct sockaddr*)&peer, &peer_len);
        char ip_registro[16];
        strcpy(ip_registro, inet_ntoa(peer.sin_addr)); // Convertimos la dirección IP a formato string

        // Procesamos la operación y mandamos el resultado de vuelta
        res = (char)registrar_usuario(nombre, ip_registro);
        write(sc, &res, 1);

        // Mostramos lo ocurrido en el servidor
        if (res == 0) {
            printf("s> REGISTER %s OK\n", nombre);

            // Si el registro es correcto se registra la acción en el servidor RPC
            registrar_accion_rpc(nombre, "REGISTER", ""); 
        } else {
            printf("s> REGISTER %s FAIL\n", nombre);
        }

    } else if (strcmp(op_str, "UNREGISTER") == 0) {
        // En el caso de la operación UNREGISTER esperamos leer el nombre a continuación
        char nombre[256];
        leer_cadena(sc, nombre);

        // Procesamos la operación y mandamos el resultado de vuelta
        res = (char)dar_de_baja_usuario(nombre);
        write(sc, &res, 1);

        // Mostramos lo ocurrido en el servidor
        if (res == 0) {
            printf("s> UNREGISTER %s OK\n", nombre);
                
            // Si la baja es correcta se registra la acción en el servidor RPC
            registrar_accion_rpc(nombre, "UNREGISTER", "");
        } else {
            printf("s> UNREGISTER %s FAIL\n", nombre);
        }

    } else if (strcmp(op_str, "CONNECT") == 0) {
        // En el caso de la operación CONNECT esperamos leer el nombre y puerto
        char nombre[256];
        char puerto_str[16];
        leer_cadena(sc, nombre);
        leer_cadena(sc, puerto_str);
        int puerto = atoi(puerto_str); // Convertimos el puerto de string a entero

        // Tenemos que obtener la IP del cliente para la conexión y para ello comenzamos inicializando la estructura
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);

        // Obtenemos la dirección del cliente al otro lado de la conexión
        getpeername(sc, (struct sockaddr*)&peer, &peer_len);
        char ip[16];
        strcpy(ip, inet_ntoa(peer.sin_addr)); // Convertimos la dirección IP a formato string

        // Procesamos la operación
        res = (char)conectar_usuario(nombre, ip, puerto);
        write(sc, &res, 1);

        // Mostramos lo ocurrido en el servidor
        if (res == 0){
            printf("s> CONNECT %s OK\n", nombre);

            // Si la conexión es correcta se registra la acción en el servidor RPC
            registrar_accion_rpc(nombre, "CONNECT", "");
        } else {
            printf("s> CONNECT %s FAIL\n", nombre);
        }

        // Ahora enviamos los mensajes pendientes si hay éxito
        if (res == 0) {
            unsigned int id;
            char remitente[256];
            char contenido[256];
            char fichero[256];

            // Mientras haya mensajes pendiantes para el usuario se continua el bucle
            while (extraer_mensaje_pendiente(nombre, &id, remitente, contenido, fichero) == 1) {
                if (strlen(fichero) > 0) {
                    // Si el mensaje tiene un fichero adjunto se envía con la función correspondiente
                    enviar_mensaje_attach_cliente(ip, puerto, remitente, id, contenido, fichero);
                    notificar_ack_attach_remitente(remitente, id, fichero);
                } else {
                    // Si el mensaje no tiene un fichero adjunto se envía con la función correspondiente
                    enviar_mensaje_cliente(ip, puerto, remitente, id, contenido);
                    notificar_ack_remitente(remitente, id);
                }
                printf("s> SEND MESSAGE %u FROM %s TO %s\n", id, remitente, nombre);
            }
        }

    } else if (strcmp(op_str, "DISCONNECT") == 0) {
        // En el caso de la operación DISCONNECT esperamos leer el nombre a continuación
        char nombre[256];
        leer_cadena(sc, nombre);

        // Tenemos que obtener la IP del cliente para comprobar si se puede realizar la desconexión
        // Para ello comenzamos inicializando la estructura
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);

        // Obtenemos la dirección del cliente al otro lado de la conexión
        getpeername(sc, (struct sockaddr*)&peer, &peer_len);
        char ip_solicitante[16];
        strcpy(ip_solicitante, inet_ntoa(peer.sin_addr)); // Convertimos la dirección IP a formato string

        // Procesamos la operación y mandamos el resultado de vuelta
        res = (char)desconectar_usuario(nombre, ip_solicitante);
        write(sc, &res, 1);

        // Mostramos lo ocurrido en el servidor
        if (res == 0) {
            printf("s> DISCONNECT %s OK\n", nombre);

            // Si la desconexión es correcta se registra la acción en el servidor RPC
            registrar_accion_rpc(nombre, "DISCONNECT", "");
        } else {
            printf("s> DISCONNECT %s FAIL\n", nombre);
        }

    } else if (strcmp(op_str, "USERS") == 0) {
        // En el caso de la operación USERS esperamos leer el nombre a continuación
        char nombre[256];
        leer_cadena(sc, nombre);

        // Procesamos la operación y mandamos el resultado de vuelta
        char nombres_conectados[100][256];
        int num_conectados;
        res = (char)obtener_usuarios_conectados(nombre, nombres_conectados, &num_conectados);
        write(sc, &res, 1);

        // En este caso hay que enviar de vuelta el número de usuarios conectados y sus nombres
        if (res == 0) {
            // Primero se envía el número de usuarios conectados como string
            char num_str[16];
            sprintf(num_str, "%d", num_conectados);
            write(sc, num_str, strlen(num_str) + 1);

            // Después se envía la información de cada usuario conectado en una cadena
            for (int i = 0; i < num_conectados; i++) {
                write(sc, nombres_conectados[i], strlen(nombres_conectados[i]) + 1);
            }
        }

         // Mostramos lo ocurrido en el servidor
        if (res == 0) {
            printf("s> CONNECTEDUSERS OK\n");
            
            // Si la operación es correcta se registra la acción en el servidor RPC
            registrar_accion_rpc(nombre, "CONNECTEDUSERS", "");
        } else {
            printf("s> CONNECTEDUSERS FAIL\n");
        }

    } else if (strcmp(op_str, "SEND") == 0) {
        // En el caso de la operación SEND esperamos leer el nombre, destinatario y contenido del mensaje a continuación
        char remitente[256];
        char destinatario[256];
        char contenido[256];
        leer_cadena(sc, remitente);
        leer_cadena(sc, destinatario);
        leer_cadena(sc, contenido);

        unsigned int id_asignado = 0;
        int dest_conectado = 0;
        int puerto_dest = 0;
        char ip_dest[16] = "";

        // Procesamos la operación y mandamos el resultado de vuelta
        res = (char)almacenar_mensaje(remitente, destinatario, contenido, &id_asignado, &dest_conectado, ip_dest, &puerto_dest);
        write(sc, &res, 1);

         // Mostramos lo ocurrido en el servidor y si está conectado el usuario se hace el envío
        if (res == 0) {
            if (dest_conectado == 1) {
                // El destinatario está conectado por lo que el envío es directo
                printf("s> SEND MESSAGE %u FROM %s TO %s\n", id_asignado, remitente, destinatario);
                enviar_mensaje_cliente(ip_dest, puerto_dest, remitente, id_asignado, contenido);

                // Se borra de los mensajes pendiantes del destinatario
                borrar_mensaje_entregado(destinatario, id_asignado);

                // Se notifica al remitente como se ha entregado el mensaje
                notificar_ack_remitente(remitente, id_asignado);
            } else {
                // El destinatorio no está conectado por lo que se avisa de que el mensaje se ha almacenado
                printf("s> MESSAGE %u FROM %s TO %s STORED\n", id_asignado, remitente, destinatario);
            }
        } else {
            printf("s> SEND FAIL\n");
        }        

        if (res == 0) {
            // Si todo salió bien se envía el identificador de mensaje generado como string
            char id_str[16];
            sprintf(id_str, "%u", id_asignado);
            write(sc, id_str, strlen(id_str) + 1);

            // Y se registra el envío en el servidor RPC
            registrar_accion_rpc(remitente, "SEND", "");
        }

    } else if (strcmp(op_str, "SENDATTACH") == 0) {
        // En el caso de la operación SENDATTACH esperamos leer el nombre, destinatario, contenido y nombre del fichero del mensaje a continuación
        char remitente[256];
        char destinatario[256];
        char contenido[256];
        char fichero[256];
        
        leer_cadena(sc, remitente);
        leer_cadena(sc, destinatario);
        leer_cadena(sc, contenido);
        leer_cadena(sc, fichero);

        unsigned int id_asignado = 0;
        int dest_conectado = 0;
        int puerto_dest = 0;
        char ip_dest[16] = "";

        // Procesamos la operación y mandamos el resultado de vuelta
        res = (char)almacenar_mensaje_attach(remitente, destinatario, contenido, fichero, &id_asignado, &dest_conectado, ip_dest, &puerto_dest);
        write(sc, &res, 1);

        // Mostramos lo ocurrido en el servidor y si está conectado el usuario se hace el envío
        if (res == 0) {
            if (dest_conectado == 1) {
                // El destinatario está conectado por lo que el envío es directo
                printf("s> SEND MESSAGE %u FROM %s TO %s\n", id_asignado, remitente, destinatario);
                enviar_mensaje_attach_cliente(ip_dest, puerto_dest, remitente, id_asignado, contenido, fichero);

                // Se borra de los mensajes pendiantes del destinatario
                borrar_mensaje_entregado(destinatario, id_asignado);

                // Se notifica al remitente como se ha entregado el mensaje con fichero adjunto
                notificar_ack_attach_remitente(remitente, id_asignado, fichero);
            } else {
                // El destinatorio no está conectado por lo que se avisa de que el mensaje se ha almacenado
                printf("s> MESSAGE %u FROM %s TO %s STORED\n", id_asignado, remitente, destinatario);
            }
        } else {
            printf("s> SENDATTACH FAIL\n");
        }

        if (res == 0) {
            // Si todo salió bien se envía el identificador de mensaje generado como string
            char id_str[16];
            sprintf(id_str, "%u", id_asignado);
            write(sc, id_str, strlen(id_str) + 1);

            // Y se registra el envío en el servidor RPC
            registrar_accion_rpc(remitente, "SENDATTACH", fichero);
        }

    } else {
        // Si el código de operación no es ninguno de los anteriores se considera un error
    }

    // Fin de la sección crítica
    pthread_mutex_unlock(&mutex_claves);

    // Se cierra la conexión con el cliente y se termina el hilo
    close(sc);
    pthread_exit(NULL);
}

void print_server_ip(char* puerto_str) {
    char hostname[256];
    struct hostent *hp;
    struct in_addr in;

    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("gethostname");
        return;
    }

    hp = gethostbyname(hostname);
    if (hp == NULL) {
        // Si falla por nombre, imprimos un error
        fprintf(stderr, "Error: No se pudo determinar la IP del host %s\n", hostname);
        return;
    }

    // Copiamos la dirección de red
    memcpy(&in.s_addr, *(hp->h_addr_list), sizeof(in.s_addr));

    // Imprimimos la IP y el puerto del servidor
    printf("s> init server %s:%s\n", inet_ntoa(in), puerto_str);
    printf("s>\n");
}

int main(int argc, char *argv[]) {
    // Comprobamos que el puerto se ha pasado de forma correcta como argumento
    if (argc != 3) {
        printf("Uso: ./server -p <PUERTO>\n");
        return -1;
    }
    // Obtenemos el puerto de los argumentos
    int puerto = atoi(argv[2]);

    // Definimos las estructuras que tendrán las direcciones del servidor y del cliente
    struct sockaddr_in server_addr, client_addr;
    socklen_t size = sizeof(client_addr);

    // Definimos el descriptor del socket de escucha
    int sd; 

    // Creamos el socket TCP (AF_INET = IPv4, SOCK_STREAM = TCP) y comprobamos que se ha creado correctamente
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("SERVER: Error en el socket");
        return -1;
    }

    int val = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &val, sizeof(int));

    // Configuramos la estructura de la dirección del servidor
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;           // IPv4                 
    server_addr.sin_addr.s_addr = INADDR_ANY;   // Para aceptar conexiones desde cualquier interfaz
    server_addr.sin_port = htons(puerto);       // Puerto del servidor traducida a formato de red

    // Asigna la dirección y el puerto al propio socket del servidor y comprobamos que se ha hecho correctamente
    if (bind(sd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("SERVER: Error en bind");
        return -1;
    }

    // Iniciamos la escucha por el socket
    listen(sd, SOMAXCONN);
    print_server_ip(argv[argc-1]);

    // Inicializamos el mutex para proteger la estructura de la lista enlazada concurrente
    pthread_mutex_init(&mutex_claves, NULL);

    // Bucle principal: Servidor concurrente mediante hilos 
    while (1) {
        int *sc = malloc(sizeof(int));
        // Aceptamos una conexión entrante y comprobamos que se he hecho correctamente
        if ((*sc = accept(sd, (struct sockaddr *)&client_addr, (socklen_t *)&size)) < 0) {
            // En caso de error liberamos la memoria del descriptor
            free(sc);
            continue;
        }

        // Creamos un hilo independiente para tratar la conexión entrante
        pthread_t thid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        // El hilo tratará de la conexión mediante la función tratar_cliente y el descriptor del cliente
        pthread_create(&thid, &attr, tratar_cliente, sc);
    }

    // Aunque el servidor no llega a este punto se cierra el socket y se destruye el mutex
    pthread_mutex_destroy(&mutex_claves);
    close(sd);
    return 0;
}