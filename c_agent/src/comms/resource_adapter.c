#include "../../include/resources/node-structures.h"
#include "../../include/comms/server_types.h"
#include "../../include/comms/event_handler.h"
#include <string.h>
#include <stdio.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <pthread.h>

pthread_mutex_t juani_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned local_agent_port = 0;

static int is_local_ip(const char *ip) {
    if (ip == NULL) return 0;
    if (strcmp(ip, "127.0.0.1") == 0) return 1;

    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == -1) return 0;

    int found = 0;
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL && !found; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;

        char addr[INET_ADDRSTRLEN];
        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        if (inet_ntop(AF_INET, &sa->sin_addr, addr, sizeof(addr)) != NULL) {
            found = strcmp(ip, addr) == 0;
        }
    }

    freeifaddrs(ifaddr);
    return found;
}

static const char *resource_name(resource_t type) {
    if (type == CPU) return "cpu";
    if (type == GPU) return "gpu";
    return "mem";
}

static void append_next_reserve(node_data_t NODE, unsigned job_id, unsigned erlang_socket, out_msg_t *outbox, int *outbox_count) {
    while (*outbox_count < MAX_OUTBOX) {
        char *target_ip = NULL;
        unsigned target_port = 0;
        resource_t type;
        unsigned amount = 0;

        pthread_mutex_lock(&juani_mutex);
        unsigned has_next = get_next_job_data(NODE, job_id, &target_ip, &target_port, &type, &amount);
        pthread_mutex_unlock(&juani_mutex);

        if (!has_next) return;

        char reserve_msg[BUFFER_SIZE];
        sprintf(reserve_msg, "RESERVE %u %s %u\n", job_id, resource_name(type), amount);

        if (is_local_ip(target_ip) && target_port == local_agent_port) {
            char reserve_out[BUFFER_SIZE] = {0};
            char granted_out[BUFFER_SIZE] = {0};

            pthread_mutex_lock(&juani_mutex);
            master_function(NODE, target_ip, target_port, erlang_socket, reserve_msg, reserve_out, BUFFER_SIZE);
            if (strlen(reserve_out) > 0) {
                master_function(NODE, target_ip, target_port, erlang_socket, reserve_out, granted_out, BUFFER_SIZE);
            }
            pthread_mutex_unlock(&juani_mutex);

            if (strlen(granted_out) > 0) {
                strcpy(outbox[*outbox_count].message, granted_out);
                outbox[*outbox_count].target_fd = erlang_socket;
                (*outbox_count)++;
                return;
            }

            if (strlen(reserve_out) == 0) return;
            continue;
        }

        strcpy(outbox[*outbox_count].message, reserve_msg);
        strcpy(outbox[*outbox_count].target_ip, target_ip);
        outbox[*outbox_count].target_port = target_port;
        outbox[*outbox_count].target_fd = -1;
        (*outbox_count)++;
        return;
    }
}

void resource_adapter_patch(node_data_t NODE, char * SENDER_IP, unsigned SOCKET, const char * BUFFER, out_msg_t * outbox, int * outbox_count, JuaniAction action) {
    char juani_out[BUFFER_SIZE] = {0};
    unsigned sender_port = get_connection_port(SOCKET);
    if (outbox_count) *outbox_count = 0;

    // ----------------------------------------------------------------------
    // 1. TIMERS AND UDP DISCOVERY
    // ----------------------------------------------------------------------
    if (action == ACTION_GET_RESOURCES) {
        local_agent_port = SOCKET;
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
        unsigned target_socket;
        while (1) {
            pthread_mutex_lock(&juani_mutex);
            int pending_job = chk_job_request(NODE, juani_out, BUFFER_SIZE, &target_socket);
            pthread_mutex_unlock(&juani_mutex);

            if (pending_job == -1 || *outbox_count >= MAX_OUTBOX) break;

            strcpy(outbox[*outbox_count].message, juani_out);
            outbox[*outbox_count].target_fd = target_socket;  // acá usás el socket
            (*outbox_count)++;
        }

        unsigned job_ids[50];
        unsigned owner_sockets[50];
        pthread_mutex_lock(&juani_mutex);
        unsigned timeout_count = collect_timed_out_jobs(NODE, job_ids, owner_sockets, 50);
        pthread_mutex_unlock(&juani_mutex);

        for (unsigned t = 0; t < timeout_count && *outbox_count < MAX_OUTBOX; t++) {
            unsigned job_id = job_ids[t];

            char *job_ips[50];
            unsigned job_ports[50];
            resource_t job_types[50];
            unsigned job_amounts[50];

            pthread_mutex_lock(&juani_mutex);
            unsigned count = get_job_data(NODE, job_id, job_ips, job_ports, job_types, job_amounts, 50);
            pthread_mutex_unlock(&juani_mutex);

            for (unsigned i = 0; i < count && *outbox_count < MAX_OUTBOX; i++) {
                const char *res_str = resource_name(job_types[i]);
                char release_msg[BUFFER_SIZE];
                sprintf(release_msg, "RELEASE %u %s %u\n", job_id, res_str, job_amounts[i]);

                if (is_local_ip(job_ips[i]) && job_ports[i] == local_agent_port) {
                    pthread_mutex_lock(&juani_mutex);
                    master_function(NODE, job_ips[i], job_ports[i], owner_sockets[t], release_msg, juani_out, BUFFER_SIZE);
                    pthread_mutex_unlock(&juani_mutex);
                    continue;
                }

                strcpy(outbox[*outbox_count].message, release_msg);
                strcpy(outbox[*outbox_count].target_ip, job_ips[i]);
                outbox[*outbox_count].target_port = job_ports[i];
                outbox[*outbox_count].target_fd = -1;
                (*outbox_count)++;
            }

            if (*outbox_count < MAX_OUTBOX) {
                sprintf(outbox[*outbox_count].message, "JOB_TIMEOUT %u\n", job_id);
                outbox[*outbox_count].target_fd = owner_sockets[t];
                (*outbox_count)++;
            }

            pthread_mutex_lock(&juani_mutex);
            remove_owned_job(NODE, job_id);
            pthread_mutex_unlock(&juani_mutex);
        }

        return;
    }

    if (action == ACTION_NEW_NODE_DISCOVERED) {
        // Le pasamos el datagrama para que guarde el nuevo nodo en su Hash Table
        pthread_mutex_lock(&juani_mutex);
        master_function(NODE, SENDER_IP, 0, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
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
        unsigned ext_ports[50];
        resource_t ext_types[50];
        unsigned ext_amounts[50];

        if (strncmp(BUFFER, "GET_NODES", 9) == 0) {
            master_function(NODE, (char*)SENDER_IP, sender_port, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
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
            master_function(NODE, (char*)SENDER_IP, sender_port, SOCKET, BUFFER, juani_out, BUFFER_SIZE);

            pthread_mutex_unlock(&juani_mutex);
            append_next_reserve(NODE, job_id, SOCKET, outbox, outbox_count);
        }
        
        // --- CASO B: JOB_RELEASE o DENIED  ---
        else if (strncmp(BUFFER, "JOB_RELEASE", 11) == 0 || strncmp(BUFFER, "DENIED", 6) == 0) {
            unsigned job_id;
            sscanf(BUFFER, "%*s %u", &job_id);
            
            // misma logica de arriba pero llamo a master cuando termino de extraer datos 
            pthread_mutex_lock(&juani_mutex);
            unsigned count = get_job_data(NODE, job_id, ext_ips, ext_ports, ext_types, ext_amounts, 50);
            pthread_mutex_unlock(&juani_mutex);
            int out_idx = 0;
            for (unsigned i = 0; i < count; i++) {
                char res_str[4];
                if      (ext_types[i] == CPU) strcpy(res_str, "cpu");
                else if (ext_types[i] == GPU) strcpy(res_str, "gpu");
                else if (ext_types[i] == RAM) strcpy(res_str, "mem");

                char directive_msg[BUFFER_SIZE];
                if (strncmp(BUFFER, "JOB_RELEASE", 11)==0){
                    sprintf(directive_msg, "RELEASE %u %s %u\n", job_id, res_str, ext_amounts[i]);
                }else{
                    sprintf(directive_msg, "DENIED %u\n", job_id);
                }

                if (is_local_ip(ext_ips[i]) && ext_ports[i] == local_agent_port) {
                    if (strncmp(BUFFER, "JOB_RELEASE", 11)==0) {
                        pthread_mutex_lock(&juani_mutex);
                        master_function(NODE, ext_ips[i], ext_ports[i], SOCKET, directive_msg, juani_out, BUFFER_SIZE);
                        pthread_mutex_unlock(&juani_mutex);
                    }
                    continue;
                }
                
                strcpy(outbox[out_idx].message, directive_msg);
                strcpy(outbox[out_idx].target_ip, ext_ips[i]);
                outbox[out_idx].target_port = ext_ports[i];
                outbox[out_idx].target_fd = -1; 
                out_idx++;
            }
            *outbox_count = out_idx;

            // en este caso si es denied juani devueleve el mensaje para el erlang, sino es 
            // job release solo borro datos
            pthread_mutex_lock(&juani_mutex); 
            master_function(NODE, (char*)SENDER_IP, sender_port, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
            pthread_mutex_unlock(&juani_mutex);
            
            if(strncmp(BUFFER, "DENIED", 6) == 0){
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
            unsigned granted_job_id = 0;
            int is_granted = (sscanf(BUFFER, "GRANTED %u", &granted_job_id) == 1);

            pthread_mutex_lock(&juani_mutex);
            master_function(NODE, (char*)SENDER_IP, sender_port, SOCKET, BUFFER, juani_out, BUFFER_SIZE);
            pthread_mutex_unlock(&juani_mutex);

            if (is_granted) {
                unsigned owner_socket = get_job_owner_socket(NODE, granted_job_id);
                if (strlen(juani_out) > 0) {
                    strcpy(outbox[0].message, juani_out);
                    outbox[0].target_fd = owner_socket;
                    *outbox_count = 1;
                } else {
                    append_next_reserve(NODE, granted_job_id, owner_socket, outbox, outbox_count);
                }
                return;
            }

            // Si la función de Juani nos devolvio algo para responder
            if (strlen(juani_out) > 0) {
                strcpy(outbox[0].message, juani_out);
                outbox[0].target_fd = SOCKET; // Respuesta inmediata por el mismo FD
                *outbox_count = 1;
            }
        }
    }
}
