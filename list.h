#ifndef _LISTA_H
#define _LISTA_H        1
#include <servicio_mensajeria.h>

struct Mensaje {
	unsigned int id;						// ID del mensaje
	char remitente[256];					// Nombre del remitente
	char contenido[256];					// Contenido del mensaje
	char fichero[256];                      // Nombre del fichero adjunto (si lo hay)
	struct Mensaje *next;
};

struct Node{ 
	char 	nombre[256];					// Nombre del usuario
	int 	conectado;						// 1 si el usuario está conectado, 0 si no lo está
	char    ip_registro[16];				// IP registrada del usuario para evitar desconexiones desde otras IPs
	char 	ip[16];
	int 	puerto;
	unsigned int ultimo_id_msg;				// Último ID de mensaje recibido por el usuario
	struct Mensaje *mensajes_pendientes;	// Lista de mensajes pendientes para el usuario
	struct 	Node *next; 
};

typedef struct Node * List;

#endif