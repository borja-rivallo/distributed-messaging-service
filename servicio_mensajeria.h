#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include "list.h"

int registrar_usuario(char *nombre, char *ip_registro);

int dar_de_baja_usuario(char *nombre);

int conectar_usuario(char *nombre, char *ip, int puerto);

int extraer_mensaje_pendiente(char *nombre_destino, unsigned int *id, char *remitente, char *contenido, char *fichero);

int desconectar_usuario(char *nombre, char *ip_solicitante);

int almacenar_mensaje(char *remitente, char *destinatario, char *contenido,
                      unsigned int *id_asignado, int *destinatario_conectado,
                      char *ip_dest, int *puerto_dest);

int almacenar_mensaje_attach(char *remitente, char *destinatario, char *contenido, char *fichero,
                             unsigned int *id_asignado, int *destinatario_conectado,
                             char *ip_dest, int *puerto_dest);

void borrar_mensaje_entregado(char *destinatario, unsigned int id_msg);

int obtener_ip_puerto_usuario(char *nombre, char *ip, int *puerto);

int obtener_usuarios_conectados(char *solicitante, char informacion_conectados[][256], int *num_conectados);
