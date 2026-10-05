#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include "servicio_mensajeria.h"
#include "list.h"

static struct Node *head = NULL;

int registrar_usuario(char *nombre, char *ip_registro) {
	// Insertar un nuevo usuario en la lista
    
    // Comprobamos que el usuario que queremos registrar no exista ya en la lista
    struct Node *curr = head;
    while (curr != NULL) {
        if (strcmp(curr->nombre, nombre) == 0) {
            // Si el usuario ya existe, se devuelve 1
			return 1;
		}
        curr = curr->next;
    }

    // Creamos el nodo para el usuario
    struct Node *ptr = (struct Node *) malloc(sizeof(struct Node));
    if (ptr == NULL) {
        // Se trata de un error de memoria por lo que debe devolver 2
		return 2;
	}

    // Rellenamos el nodo con los diferentes datos necesarios
	strncpy(ptr->nombre, nombre, sizeof(ptr->nombre));
    ptr->nombre[255] = '\0';
    strncpy(ptr->ip_registro, ip_registro, 15);
    ptr->ip_registro[15] = '\0';
    ptr->conectado = 0;
    ptr->ultimo_id_msg = 0;
    ptr->mensajes_pendientes = NULL;

	// Pasamos la cabeza de la lista enlazada al siguiente nodo
    ptr->next = head;
    head = ptr;
    return 0;
}

int dar_de_baja_usuario(char *nombre) {
    // Eliminar un usuario de la lista
    struct Node *curr = head;
    struct Node *prev = NULL;

    while (curr != NULL) {
        if (strcmp(curr->nombre, nombre) == 0) {
            // Si el usuario se encuentra en la lista, lo eliminamos junto a sus mensajes pendientes
            struct Mensaje *msg_curr = curr->mensajes_pendientes;
            while (msg_curr != NULL) {
                struct Mensaje *msg_next = msg_curr->next;
                free(msg_curr);
                msg_curr = msg_next;
            }
            if (prev == NULL) {
                head = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    // Si el usuario no se encuentra en la lista, se devuelve 1
    return 1;
}

int conectar_usuario(char *nombre, char *ip, int puerto) {
    // Conectar un usuario a la lista
    struct Node *curr = head;
    while (curr != NULL) {
        if (strcmp(curr->nombre, nombre) == 0) {
            // Si el usuario se encuentra en la lista y esta conectado, se devuelve 2
            if (curr->conectado) {
                return 2;
            }
            // Si no esta conectado lo conectamos
            curr->conectado = 1;
            strncpy(curr->ip, ip, sizeof(curr->ip));
            curr->ip[15] = '\0';
            curr->puerto = puerto;
            return 0;
        }
        curr = curr->next;
    }
    // Si el usuario no se encuentra en la lista, se devuelve 1
    return 1;
}

int extraer_mensaje_pendiente(char *nombre_destino, unsigned int *id, char *remitente, char *contenido, char *fichero) {
    // Función auxiliar para extraer un mensaje pendiente para un usuario
    struct Node *curr = head;

    while (curr != NULL) {
        if (strcmp(curr->nombre, nombre_destino) == 0) {
            // Comprobamos que tenga mensajes pendientes
            if (curr->mensajes_pendientes != NULL) {
                struct Mensaje *msg = curr->mensajes_pendientes;

                // Rellenamos los datos del mensaje
                *id = msg->id;
                strcpy(remitente, msg->remitente);
                strcpy(contenido, msg->contenido);
                strcpy(fichero, msg->fichero);

                // Eliminamos el mensaje de la lista de mensajes pendientes
                curr->mensajes_pendientes = msg->next;
                free(msg);

                // Si sale bien devuelve 1
                return 1;
            }
            // Si no tiene mensajes pendientes devuelve 0
            return 0;
        }
        curr = curr->next;
    }
    // Si el usuario no se encuentra en la lista, se devuelve 0
    return 0;
}

int desconectar_usuario(char *nombre, char *ip_solicitante) {
    // Desconectar un usuario de la lista

    struct Node *curr = head;

    while (curr != NULL) {
        if (strcmp(curr->nombre, nombre) == 0) {
            // Si el usuario se encuentra en la lista y esta desconectado, se devuelve 2
            if (!curr->conectado) {
                return 2;
            }
            // Si la IP del solicitante no coincide con la IP registrada para el usuario, se devuelve 2
            if (strcmp(curr->ip_registro, ip_solicitante) != 0) {
                return 3;
            }
            // Si no esta desconectado lo desconectamos
            curr->conectado = 0;
            strcpy(curr->ip, "");
            curr->puerto = 0;
            return 0;
        }
        curr = curr->next;
    }
    // Si el usuario no se encuentra en la lista, se devuelve 1
    return 1;
}

int almacenar_mensaje(char *remitente, char *destinatario, char *contenido,
                      unsigned int *id_asignado, int *destinatario_conectado,
                      char *ip_dest, int *puerto_dest) {
    // Función auxiliar para almacenar un mensaje sin fichero en la lista de mensajes pendientes del destinatario
    
    struct Node *node_remitente = NULL;
    struct Node *node_destinatario = NULL;
    struct Node *curr = head;

    // Buscamos el nodo del remitente y del destinatario
    while (curr != NULL) {
        if (strcmp(curr->nombre, remitente) == 0) {
            node_remitente = curr;
        }
        if (strcmp(curr->nombre, destinatario) == 0) {
            node_destinatario = curr;
        }
        curr = curr->next;
    }

    if (node_remitente == NULL || node_destinatario == NULL) {
        // Si el remitente o el destinatario no se encuentran en la lista, se devuelve 1
        return 1;
    }

    // Asignamos un ID único al mensaje
    node_remitente->ultimo_id_msg++;
    if (node_remitente->ultimo_id_msg == 0) {
        node_remitente->ultimo_id_msg = 1; // Evitamos que el ID sea 0 al desbordar
    }
    *id_asignado = node_remitente->ultimo_id_msg;

    // Creamos el mensaje
    struct Mensaje *nuevo_msg = (struct Mensaje *) malloc(sizeof(struct Mensaje));
    if (nuevo_msg == NULL) {
        // Se trata de un error de memoria por lo que debe devolver 2
        return 2;
    }

    nuevo_msg->id = *id_asignado;
    strcpy(nuevo_msg->remitente, remitente);
    strcpy(nuevo_msg->contenido, contenido);
    strcpy(nuevo_msg->fichero, ""); // No hay fichero adjunto en esta función
    nuevo_msg->next = NULL;

    // Almacenamos el mensaje en la lista de mensajes pendientes del destinatario
    if (node_destinatario->mensajes_pendientes == NULL) {
        node_destinatario->mensajes_pendientes = nuevo_msg;
    } else {
        struct Mensaje *msg_aux = node_destinatario->mensajes_pendientes;
        while (msg_aux->next != NULL) {
            msg_aux = msg_aux->next;
        }
        msg_aux->next = nuevo_msg;
    }

    // Rellenamos los datos de salida
    *destinatario_conectado = node_destinatario->conectado;
    if (node_destinatario->conectado == 1) {
        strcpy(ip_dest, node_destinatario->ip);
        *puerto_dest = node_destinatario->puerto;
    }

    return 0;
}

int almacenar_mensaje_attach(char *remitente, char *destinatario, char *contenido, char *fichero,
                            unsigned int *id_asignado, int *destinatario_conectado,
                            char *ip_dest, int *puerto_dest) {
    // Función auxiliar para almacenar un mensaje con fichero en la lista de mensajes pendientes del destinatario
    
    struct Node *node_remitente = NULL;
    struct Node *node_destinatario = NULL;
    struct Node *curr = head;

    // Buscamos el nodo del remitente y del destinatario
    while (curr != NULL) {
        if (strcmp(curr->nombre, remitente) == 0) {
            node_remitente = curr;
        }
        if (strcmp(curr->nombre, destinatario) == 0) {
            node_destinatario = curr;
        }
        curr = curr->next;
    }

    if (node_remitente == NULL || node_destinatario == NULL) {
        // Si el remitente o el destinatario no se encuentran en la lista, se devuelve 1
        return 1;
    }

    // Asignamos un ID único al mensaje
    node_remitente->ultimo_id_msg++;
    if (node_remitente->ultimo_id_msg == 0) {
        node_remitente->ultimo_id_msg = 1; // Evitamos que el ID sea 0 al desbordar
    }
    *id_asignado = node_remitente->ultimo_id_msg;

    // Creamos el mensaje
    struct Mensaje *nuevo_msg = (struct Mensaje *) malloc(sizeof(struct Mensaje));
    if (nuevo_msg == NULL) {
        // Se trata de un error de memoria por lo que debe devolver 2
        return 2;
    }

    nuevo_msg->id = *id_asignado;
    strcpy(nuevo_msg->remitente, remitente);
    strcpy(nuevo_msg->contenido, contenido);
    strcpy(nuevo_msg->fichero, fichero); // Se copia el nombre del fichero adjunto
    nuevo_msg->next = NULL;

    // Almacenamos el mensaje en la lista de mensajes pendientes del destinatario
    if (node_destinatario->mensajes_pendientes == NULL) {
        node_destinatario->mensajes_pendientes = nuevo_msg;
    } else {
        struct Mensaje *msg_aux = node_destinatario->mensajes_pendientes;
        while (msg_aux->next != NULL) {
            msg_aux = msg_aux->next;
        }
        msg_aux->next = nuevo_msg;
    }

    // Rellenamos los datos de salida
    *destinatario_conectado = node_destinatario->conectado;
    if (node_destinatario->conectado == 1) {
        strcpy(ip_dest, node_destinatario->ip);
        *puerto_dest = node_destinatario->puerto;
    }

    return 0;
}

void borrar_mensaje_entregado(char *destinatario, unsigned int id_msg) {
    // Función auxiliar para borrar un mensaje entregado de la lista de mensajes pendientes del destinatario
    struct Node *curr = head;

    while (curr != NULL) {
        if (strcmp(curr->nombre, destinatario) == 0) {
            struct Mensaje *msg_curr = curr->mensajes_pendientes;
            struct Mensaje *msg_prev = NULL;

            while (msg_curr != NULL) {
                if (msg_curr->id == id_msg) {
                    // Si el mensaje se encuentra en la lista, lo eliminamos
                    if (msg_prev == NULL) {
                        curr->mensajes_pendientes = msg_curr->next;
                    } else {
                        msg_prev->next = msg_curr->next;
                    }
                    free(msg_curr);
                    return;
                }
                msg_prev = msg_curr;
                msg_curr = msg_curr->next;
            }
            return;
        }
        curr = curr->next;
    }
}

int obtener_ip_puerto_usuario(char *nombre, char *ip, int *puerto) {
    // Función auxiliar para obtener la IP y el puerto de un usuario
    struct Node *curr = head;
    while (curr != NULL) {
        if (strcmp(curr->nombre, nombre) == 0 && curr->conectado) {
            strcpy(ip, curr->ip);
            *puerto = curr->puerto;
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

int obtener_usuarios_conectados(char *solicitante, char informacion_conectados[][256], int *num_conectados) {
    struct Node *curr = head;
    int solicitante_existe = 0;
    int solicitante_conectado = 0;

    while (curr != NULL) {
        if (strcmp(curr->nombre, solicitante) == 0) {
            solicitante_existe = 1;
            if (curr->conectado == 1) {
                solicitante_conectado = 1;
            }
            break;
        }
        curr = curr->next;
    }

    if (solicitante_existe == 0) {
        // Si el solicitante no se encuentra en la lista, se devuelve 2
        return 2;
    }
    if (solicitante_conectado == 0) {
        // Si el solicitante no está conectado, se devuelve 1
        return 1;
    }

    // Recorremos la lista para obtener los usuarios conectados
    *num_conectados = 0;
    curr = head;
    char aux[512];

    while (curr != NULL) {
        if (curr->conectado == 1) {
            sprintf(aux, "%s :: %s :: %d", curr->nombre, curr->ip, curr->puerto);
            strcpy(informacion_conectados[*num_conectados], aux);
            (*num_conectados)++;
        }
        curr = curr->next;
    }

    return 0;
}