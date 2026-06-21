#ifndef MOCK_RESOURCE_MANAGER_POOL_H
#define MOCK_RESOURCE_MANAGER_POOL_H
#include "network_core.h"
typedef enum {
    ACTION_RESPOND,           // Enviar un mensaje a un fd ya conectado
    ACTION_CHECK_DEADNODES, // Conectarse a un nuevo nodo y enviarle un mensaje
    ACTION_GET_RESOURCES,    // Solicitar recursos a un nodo (puede implicar conectarse)
    ACTION_NEW_NODE_DISCOVERED, // Se descubrió un nuevo nodo (UDP) → conectar y enviar
    ACTION_DISCONNECTED,
    ACTION_NONE               // No hacer nada
} JuaniAction;

typedef struct{
    void* nada;
}node_data_t_;

typedef node_data_t_ * node_data_t;

void master_function(
    node_data_t NODE, 
    const char * SENDER_IP, 
    unsigned SOCKET, 
    const char * BUFFER, 
    out_msg_t * outbox,       
    int * outbox_count,
    JuaniAction action
);

#endif