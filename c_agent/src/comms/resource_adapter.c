#include "../../include/resources/node-structures.h"
#include "../../include/comms/server_types.h"
#include "../../include/comms/event_handler.h"
#include <string.h>
#include <stdio.h>

#include <pthread.h>

pthread_mutex_t juani_mutex = PTHREAD_MUTEX_INITIALIZER;

void resource_adapter_patch(node_data_t NODE, char * SENDER_IP, unsigned SOCKET, const char * BUFFER, out_msg_t * outbox, int * outbox_count, JuaniAction action) {
    char juani_out[BUFFER_SIZE] = {0};
    if (outbox_count) *outbox_count = 0;

    // ----------------------------------------------------------------------
    // 1. TIMERS AND UDP DISCOVERY
    // ----------------------------------------------------------------------
    if (action == ACTION_GET_RESOURCES) {
        unsigned cpu, gpu, ram;
        // Get resources from the resource manager
        pthread_mutex_lock(&juani_mutex); 
        get_local_resources(NODE, &cpu, &gpu, &ram);
        pthread_mutex_unlock(&juani_mutex);
        // We get the port from unsigned  socket, juani do not manage socket 
        int tcp_public_port = SOCKET; // we call it socket but its not a socket je
        // "ANNOUNCE port cpu:X mem:Y gpu:Z"
        sprintf(outbox[0].message, "ANNOUNCE %d cpu:%u mem:%u gpu:%u\n", tcp_public_port, cpu, ram, gpu);
        *outbox_count = 1;
        return;
    }
    if (action == ACTION_CHECK_DEADNODES) {
        // limpiar nodos inactivos  JUANI SOLO DEVUELVE GRANTED

        char * ext_ips[50];
        resource_t ext_types[50];
        unsigned ext_amounts[50];
        unsigned target_socket; // CORRECCIÓN: Le quité el asterisco para que no sea un puntero no inicializado (peligro de crash)
        // FUNCION JUANI PENDIENTE

        return;
    }

    if (action == ACTION_NEW_NODE_DISCOVERED) {
        // Le pasamos el datagrama para que guarde el nuevo nodo en su Hash Table
        pthread_mutex_lock(&juani_mutex);
        master_function(NODE, SENDER_IP, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
        pthread_mutex_unlock(&juani_mutex);
        return;
    }

    if (action == ACTION_DISCONNECTED) {
        // Llamado a funcion juani que gestiona nodos muertos 
        // tiene que borrar todas los jobs que esten relacionados a este nodo
        // solo tengo como informacion el fd del mismo 
        return;
    }

    // ----------------------------------------------------------------------
    // 2. EVENTOS TCP Y PRE-PARSING DE MENSAJES (Erlang)
    // ----------------------------------------------------------------------
    if (action == ACTION_RESPOND && BUFFER != NULL) {
        
        char * ext_ips[50];
        resource_t ext_types[50];
        unsigned ext_amounts[50];

        if (strncmp(BUFFER, "GET_NODES", 9) == 0) {
            master_function(NODE, (char*)SENDER_IP, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
            if (strlen(juani_out) > 0) {
                strcpy(outbox[0].message, juani_out);
                outbox[0].target_fd = SOCKET;
                *outbox_count = 1;
            }
            return;
        }
        // --- CASO A: JOB_REQUEST  ---
        if (strncmp(BUFFER, "JOB_REQUEST", 11) == 0) {
            unsigned job_id;
            sscanf(BUFFER, "JOB_REQUEST %u", &job_id);
            
            // agruega a su tabla hash
            pthread_mutex_lock(&juani_mutex);
            master_function(NODE, (char*)SENDER_IP, SOCKET, BUFFER, juani_out, BUFFER_SIZE);

            // nos da la data
            unsigned count = get_job_data(NODE, job_id, ext_ips, ext_types, ext_amounts, 50);
            pthread_mutex_unlock(&juani_mutex);
            // hacemos mensaje y lo ponemos en el buzon
            for (unsigned i = 0; i < count; i++) {
                char res_str[4];
                if      (ext_types[i] == CPU) strcpy(res_str, "cpu");
                else if (ext_types[i] == GPU) strcpy(res_str, "gpu");
                else if (ext_types[i] == RAM) strcpy(res_str, "mem");

                sprintf(outbox[i].message, "RESERVE %u %s %u\n", job_id, res_str, ext_amounts[i]);
                strcpy(outbox[i].target_ip, ext_ips[i]);
                pthread_mutex_lock(&juani_mutex);
                unsigned node_port = get_node_port(NODE, ext_ips[i]);
                pthread_mutex_unlock(&juani_mutex);
                outbox[i].target_port = node_port;
                outbox[i].target_fd = -1; // Le decimos a la capa de red que lo enrute por IP
                // aunque ya le hayamos pasado el nodo capaz que ya tiene abierto un canal
            }
            *outbox_count = count;
        }
        
        // --- CASO B: JOB_RELEASE o DENIED  ---
        else if (strncmp(BUFFER, "JOB_RELEASE", 11) == 0 || strncmp(BUFFER, "DENIED", 6) == 0) {
            unsigned job_id;
            sscanf(BUFFER, "%*s %u", &job_id);
            
            // misma logica de arriba pero llamo a master cuando termino de extraer datos 
            pthread_mutex_lock(&juani_mutex);
            unsigned count = get_job_data(NODE, job_id, ext_ips, ext_types, ext_amounts, 50);
            pthread_mutex_unlock(&juani_mutex);
            for (unsigned i = 0; i < count; i++) {
                char res_str[4];
                if      (ext_types[i] == CPU) strcpy(res_str, "cpu");
                else if (ext_types[i] == GPU) strcpy(res_str, "gpu");
                else if (ext_types[i] == RAM) strcpy(res_str, "mem");

                if (strncmp(BUFFER, "JOB_RELEASE", 11)==0){
                    sprintf(outbox[i].message, "RELEASE %u %s %u\n", job_id, res_str, ext_amounts[i]);
                }else{
                    sprintf(outbox[i].message, "DENIED %u\n", job_id);
                }
                
                strcpy(outbox[i].target_ip, ext_ips[i]);
                pthread_mutex_lock(&juani_mutex);
                unsigned node_port = get_node_port(NODE, ext_ips[i]);
                pthread_mutex_unlock(&juani_mutex);
                outbox[i].target_port = node_port;
                outbox[i].target_fd = -1; 
            }
            *outbox_count = count;

            // en este caso si es denied juani devueleve el mensaje para el erlang, sino es 
            // job release solo borro datos
            pthread_mutex_lock(&juani_mutex); 
            master_function(NODE, (char*)SENDER_IP, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
            pthread_mutex_unlock(&juani_mutex);
            
            if(strncmp(BUFFER, "DENIED", 6) == 0){
                // ACA JAUNI DEBERIA DE RESPONDER JOB_DENIED
                // CORRECCIÓN: Lógica de incremento y asignación reparada
                strcpy(outbox[*outbox_count].message, juani_out);
                outbox[*outbox_count].target_fd = SOCKET; 
                (*outbox_count)++; 
            }   
        }
        // --- CASO D: COMANDOS SIMPLES (GRANTED, RESERVE, JOB_GRANTED, GET_NODES) ---
        else {
            // GET_NODES_RESPUESTA INMEDIATA
            // JOB_GRANTED SIN RESPUESTA
            // GRANTED POSIBLE RESPUESTA DE LONGITUD 1 (JOB_GRANTED)
            // RESERVE POSIBLE RESPUESTA DE LONGITUD 1 (GRANTED)
            pthread_mutex_lock(&juani_mutex);
            master_function(NODE, (char*)SENDER_IP, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
            pthread_mutex_unlock(&juani_mutex);
            // Si la función de Juani nos devolvio algo para responder
            if (strlen(juani_out) > 0) {
                strcpy(outbox[0].message, juani_out);
                outbox[0].target_fd = SOCKET; // Respuesta inmediata por el mismo FD
                *outbox_count = 1;
            }
        }
    }
}