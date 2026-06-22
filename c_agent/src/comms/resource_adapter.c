#include "../../include/resources/node-structures.h"
#include "../../include/comms/server_types.h"
#include "../../include/comms/event_handler.h"
#include <string.h>
#include <stdio.h>

#define DEFAULT_PORT 8080 // MI PORT

void resource_adapter_patch(node_data_t NODE, char * SENDER_IP, unsigned SOCKET, const char * BUFFER, out_msg_t * outbox, int * outbox_count, JuaniAction action) {
    char juani_out[BUFFER_SIZE] = {0};
    if (outbox_count) *outbox_count = 0;

    // ----------------------------------------------------------------------
    // 1. TIMERS AND UDP DISCOVERY
    // ----------------------------------------------------------------------
    if (action == ACTION_GET_RESOURCES) {
        unsigned cpu, gpu, ram;
        // Get resources from the resource manager 
        get_local_resources(NODE, &cpu, &gpu, &ram);
        // We get the port from unsigned  socket, juani do not manage socket 
        int tcp_public_port = SOCKET; // we call it socket but its not a socket je
        // "ANNOUNCE port cpu:X mem:Y gpu:Z"
        sprintf(outbox[0].message, "ANNOUNCE %d cpu:%u mem:%u gpu:%u\n", tcp_public_port, cpu, ram, gpu);
        *outbox_count = 1;
        return;
    }
    if (action == ACTION_CHECK_DEADNODES) {
        // limpiar nodos inactivos 
        known_nodes_del_inactive_nodes(NODE);
    
        char buffer_cola[BUFFER_SIZE];
        
        // hasta que devuelva -1 cola vacia o no se puede garatizar job
        while (chk_job_request(NODE, buffer_cola, BUFFER_SIZE) != -1) {
            
            // buffer_cola ahora tiene algo como "GRANTED 5\n"
            strcpy(outbox[*outbox_count].message, buffer_cola);
            
            // pero tendria que encontrar una forma de conseguir el socket 
            // aca no tengo el job id como en las job_request y eso 

            strcpy(outbox[*outbox_count].target_ip, "127.0.0.1"); 
            
            outbox[*outbox_count].target_port = DEFAULT_PORT; 
            
            (*outbox_count)++;
        }

        return;
    }

    if (action == ACTION_NEW_NODE_DISCOVERED) {
        // Le pasamos el datagrama para que guarde el nuevo nodo en su Hash Table
        master_function(NODE, SENDER_IP, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
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

        // --- CASO A: JOB_REQUEST  ---
        if (strncmp(BUFFER, "JOB_REQUEST", 11) == 0) {
            unsigned job_id;
            sscanf(BUFFER, "JOB_REQUEST %u", &job_id);
            
            // agruega a su tabla hash
            master_function(NODE, (char*)SENDER_IP, SOCKET, BUFFER, juani_out, BUFFER_SIZE);

            // nos da la data
            unsigned count = get_job_data(NODE, job_id, ext_ips, ext_types, ext_amounts, 50);

            // hacemos mensaje y lo ponemos en el buzon
            for (unsigned i = 0; i < count; i++) {
                char res_str[4];
                if      (ext_types[i] == CPU) strcpy(res_str, "cpu");
                else if (ext_types[i] == GPU) strcpy(res_str, "gpu");
                else if (ext_types[i] == RAM) strcpy(res_str, "mem");

                sprintf(outbox[i].message, "RESERVE %u %s %u\n", job_id, res_str, ext_amounts[i]);
                strcpy(outbox[i].target_ip, ext_ips[i]);
                unsigned node_port = get_node_port(NODE, ext_ips[i]);
                outbox[i].target_port = node_port;
                outbox[i].target_fd = -1; // Le decimos a la capa de red que lo enrute por IP
                // aunque ya le hayamos pasado el nodo capaz que ya tiene abierto un canal
            }
            *outbox_count = count;
        }
        
        // --- CASO B: JOB_RELEASE  ---
        else if (strncmp(BUFFER, "JOB_RELEASE", 11) == 0) {
            unsigned job_id;
            sscanf(BUFFER, "JOB_RELEASE %u", &job_id);
            
            // misma logica de arriba pero llamo a master cuando termino de extraer datos 

            unsigned count = get_job_data(NODE, job_id, ext_ips, ext_types, ext_amounts, 50);

            for (unsigned i = 0; i < count; i++) {
                char res_str[4];
                if      (ext_types[i] == CPU) strcpy(res_str, "cpu");
                else if (ext_types[i] == GPU) strcpy(res_str, "gpu");
                else if (ext_types[i] == RAM) strcpy(res_str, "mem");

                sprintf(outbox[i].message, "RELEASE %u %s %u\n", job_id, res_str, ext_amounts[i]);
                strcpy(outbox[i].target_ip, ext_ips[i]);
                unsigned node_port = get_node_port(NODE, ext_ips[i]);
                outbox[i].target_port = node_port;
                outbox[i].target_fd = -1; 
            }
            *outbox_count = count;

            master_function(NODE, (char*)SENDER_IP, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
        }
        
        // --- CASO C: COMANDOS SIMPLES (GRANTED, DENIED, RESERVE directo, etc) ---
        else {
            master_function(NODE, (char*)SENDER_IP, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
            
            // Si la función de Juani nos devolvió algo para responder
            if (strlen(juani_out) > 0) {
                strcpy(outbox[0].message, juani_out);
                outbox[0].target_fd = SOCKET; // Respuesta inmediata por el mismo FD
                *outbox_count = 1;
            }
        }
    }
}

// FALTA IMPLEMENTAR TIMEOUTS PARA TRABAJOS ONDA CUANTO TIMEPO PASO DESDE QUE SE HZO UN JOB REQUEST Y SI 
// PASA ESE TIEMPO LO ELIMINAMOS DE LA LISTA DE JOB ACTIVOS Y LA LISTA DE JOBS PENDIENTES QUE ESTEN ENCOLADOS
// TAMBIEN TIMEOUT DE RECURSOS PRESTADO, NO TODA LA VIDA PRESTAMOS RECURSOS