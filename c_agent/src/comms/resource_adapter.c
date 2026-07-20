#include "../../include/comms/resource_adapter.h"
#include "../../include/resources/node_structures.h"
#include "../../include/comms/server_types.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/* ========================================================================= */
/* INTERNAL HELPERS                                                          */
/* ========================================================================= */

/**
 * @brief Safely formats and appends a new message to the outgoing message queue.
 * @param outbox The array representing the outgoing message queue.
 * @param count Pointer to the current number of messages in the outbox.
 * @param msg The raw string message to send.
 * @param target_fd The target file descriptor (-1 if routing by IP/Port).
 * @param ip The target IP address (NULL if routing by FD).
 * @param port The target TCP port (0 if routing by FD).
 */
static void add_to_outbox(out_msg_t *outbox, int *count, const char *msg, int target_fd, const char *ip, unsigned port) {
    if (*count >= MAX_OUTBOX) return; // Prevent buffer overflow
    
    snprintf(outbox[*count].message, sizeof(outbox[*count].message), "%s", msg);
    outbox[*count].target_fd = target_fd;
    outbox[*count].target_port = port;

    if (ip != NULL) {
        strncpy(outbox[*count].target_ip, ip, sizeof(outbox[*count].target_ip) - 1);
        outbox[*count].target_ip[sizeof(outbox[*count].target_ip) - 1] = '\0';
    } else {
        outbox[*count].target_ip[0] = '\0';
    }

    (*count)++;
}


/* ========================================================================= */
/* HANDLERS - MESSAGING LOGIC (ERLANG & REMOTE NODES)                        */
/* ========================================================================= */

static void handle_get_nodes_response(ServerContext *ctx, unsigned socket, out_msg_t *outbox, int *count) {
    node_data_t NODE = (node_data_t)ctx->mynode;
    char msg[BUFFER_SIZE] = {0};

    pthread_mutex_lock(&NODE->lock_known);
    get_known_nodes_payload(NODE->known_nodes, msg, sizeof(msg));
    pthread_mutex_unlock(&NODE->lock_known);

    char final_msg[BUFFER_SIZE + 1];
    snprintf(final_msg, sizeof(final_msg), "%s", msg);
    
    add_to_outbox(outbox, count, final_msg, socket, NULL, 0);
}

/**
 * @brief Processes the initial JOB_REQUEST from the local Erlang scheduler.
 */
static void handle_job_request(ServerContext *ctx, unsigned socket, const char *buffer, out_msg_t *outbox, int *count) {
    node_data_t NODE = (node_data_t)ctx->mynode;
    unsigned job_id;
    
    // Safety check: Ensure the protocol format is correct
    if (sscanf(buffer, "JOB_REQUEST %u", &job_id) != 1) return;
    
    char *dup = strdup(buffer);
    if (!dup) return; // Safety check: memory allocation failure
    
    pthread_mutex_lock(&NODE->lock_owned);

    // 1. Register the base Job
    add_new_owned_job(NODE->owned_jobs, job_id, socket);
    
    // 2. Parse resource petitions
    char *token = strtok(dup, " "); // skip "JOB_REQUEST"
    token = strtok(NULL, " ");      // skip "job_id"
    token = strtok(NULL, " ");      // first "@ip:port:res:qty"
    
    while (token != NULL) {
        char ip[16], res[10]; unsigned port, cant;
        if (sscanf(token, "@%15[^:]:%u:%9[^:]:%u", ip, &port, res, &cant) == 4) {
            resource_t type = (strcmp(res, "cpu") == 0) ? CPU : (strcmp(res, "mem") == 0) ? RAM : GPU;
            append_petition_to_job(NODE->owned_jobs, job_id, ip, port, type, cant);
        }
        token = strtok(NULL, " ");
    }
    pthread_mutex_unlock(&NODE->lock_owned);
    free(dup);
    
    // 3. Dispatch the first RESERVE message of the chain
    char *ip_next; unsigned port_next, cant_next; resource_t type_next;
    if (get_next_reserve(NODE->owned_jobs, job_id, &ip_next, &port_next, &type_next, &cant_next)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "RESERVE %u %s %u\n", job_id, (type_next==CPU)?"cpu":(type_next==RAM)?"mem":"gpu", cant_next);
        add_to_outbox(outbox, count, msg, -1, ip_next, port_next);
    }
}

/**
 * @brief Handles incoming RESERVE messages from remote nodes requesting local resources.
 */
static void handle_reserve(ServerContext *ctx, unsigned socket, const char *buffer, out_msg_t *outbox, int *count) {
    node_data_t NODE = (node_data_t)ctx->mynode;
    unsigned job_id, cant; char res[10];
    
    if (sscanf(buffer, "RESERVE %u %9s %u", &job_id, res, &cant) != 3) return;
    resource_t type = (strcmp(res, "cpu") == 0) ? CPU : (strcmp(res, "mem") == 0) ? RAM : GPU;
    
    pthread_mutex_lock(&NODE->lock_local);

    int result = new_job_request(NODE->resources, NODE->active_jobs, job_id, socket, cant, type);
    
    if (result == 1) { // Resources Granted Immediately
        char msg[64];
        snprintf(msg, sizeof(msg), "GRANTED %u\n", job_id);
        add_to_outbox(outbox, count, msg, socket, NULL, 0);
    } else if (result == -1) {
        char msg[64];
        snprintf(msg, sizeof(msg), "DENIED %u\n", job_id);
        add_to_outbox(outbox, count, msg, socket, NULL, 0);
    }
    // Note: If result == 0 (WAIT), we do nothing. The request is safely queued.
    pthread_mutex_unlock(&NODE->lock_local);
}

/**
 * @brief Processes GRANTED messages from remote nodes. 
 * Marks the petition as done and either notifies Erlang or triggers the next RESERVE.
 */
static void handle_granted(ServerContext *ctx, const char *sender_ip, unsigned sender_port, unsigned socket, const char *buffer, out_msg_t *outbox, int *count) {
    socket = socket; // Prevent warning for unused variable
    node_data_t NODE = (node_data_t)ctx->mynode;
    unsigned job_id;
    
    if (sscanf(buffer, "GRANTED %u", &job_id) != 1) return;
    if (sender_port == 0) return; // Prevent matching against invalid port
    
    pthread_mutex_lock(&NODE->lock_owned);

    // 1. Mark petition as granted
    int grant_result = mark_petition_as_granted(NODE->owned_jobs, job_id, sender_ip, sender_port);
    if (grant_result == 1) {
        // Job is 100% Complete: Notify Erlang
        char msg[64];
        unsigned owner_socket = get_job_owner_socket(NODE->owned_jobs, job_id);
        snprintf(msg, sizeof(msg), "JOB_GRANTED %u\n", job_id);
        add_to_outbox(outbox, count, msg, owner_socket, NULL, 0);
    } else if (grant_result == 0) {
        // Job incomplete: Dispatch the next RESERVE in the queue
        char *ip_next; unsigned port_next, cant_next; resource_t type_next;
        if (get_next_reserve(NODE->owned_jobs, job_id, &ip_next, &port_next, &type_next, &cant_next)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "RESERVE %u %s %u\n", job_id, (type_next==CPU)?"cpu":(type_next==RAM)?"mem":"gpu", cant_next);
            add_to_outbox(outbox, count, msg, -1, ip_next, port_next);
        }
    }

    pthread_mutex_unlock(&NODE->lock_owned);
}

/**
 * @brief Processes DENIED messages. Performs a rollback by releasing all previously 
 * granted resources on other nodes and notifying Erlang of the failure.
 */
static void handle_denied(ServerContext *ctx, const char *buffer, out_msg_t *outbox, int *count) {
    node_data_t NODE = (node_data_t)ctx->mynode;
    unsigned job_id;
    
    if (sscanf(buffer, "DENIED %u", &job_id) != 1) return;

    pthread_mutex_lock(&NODE->lock_owned);
    
    // 1. Extract successfully granted resources for rollback
    char *ips[MAX_JOB_RESOURCES]; unsigned ports[MAX_JOB_RESOURCES], cants[MAX_JOB_RESOURCES];
    resource_t types[MAX_JOB_RESOURCES];
    unsigned n = get_granted_resources(NODE->owned_jobs, job_id, ips, ports, types, cants, MAX_JOB_RESOURCES);
    unsigned owner_socket = get_job_owner_socket(NODE->owned_jobs, job_id);
    
    // 2. Dispatch RELEASE to all surviving nodes
    for(unsigned i = 0; i < n; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "RELEASE %u %s %u\n", job_id, (types[i]==CPU)?"cpu":(types[i]==RAM)?"mem":"gpu", cants[i]);
        add_to_outbox(outbox, count, msg, -1, ips[i], ports[i]);
    }
    
    // 3. Notify Erlang of total failure and clean up memory
    char msg_deny[64];
    snprintf(msg_deny, sizeof(msg_deny), "JOB_DENIED %u\n", job_id);
    add_to_outbox(outbox, count, msg_deny, owner_socket, NULL, 0);
    remove_owned_job(NODE->owned_jobs, job_id);

    pthread_mutex_unlock(&NODE->lock_owned);
}

/**
 * @brief Handles RELEASE messages from remote nodes, freeing local resources 
 * and potentially unlocking pending requests in the queue.
 */
static void handle_release(ServerContext *ctx, unsigned socket, const char *buffer, out_msg_t *outbox, int *count) {
    node_data_t NODE = (node_data_t)ctx->mynode;
    unsigned job_id, cant; char res[10];
    
    if (sscanf(buffer, "RELEASE %u %9s %u", &job_id, res, &cant) != 3) return;

    pthread_mutex_lock(&NODE->lock_local);
    
    // 1. Reclaim local resources
    del_active_job(NODE->resources, NODE->active_jobs, job_id, socket);
    
    // 2. Wake up queued requests if resources are now available
    char msg[64];
    unsigned queued_socket = 0;
    
    while (chk_job_request(NODE->resources, NODE->active_jobs, msg, sizeof(msg), &queued_socket) != -1) {
        add_to_outbox(outbox, count, msg, queued_socket, NULL, 0);
    }

    pthread_mutex_unlock(&NODE->lock_local);
}

/**
 * @brief Handles JOB_RELEASE from local Erlang, signaling the successful termination 
 * of a job and triggering the release of remote resources.
 */
static void handle_job_release(ServerContext *ctx, const char *buffer, out_msg_t *outbox, int *count) {
    node_data_t NODE = (node_data_t)ctx->mynode;
    unsigned job_id;
    
    if (sscanf(buffer, "JOB_RELEASE %u", &job_id) != 1) return;
    
    pthread_mutex_lock(&NODE->lock_owned);

    // 1. Extract all remote nodes involved in this job
    char *ips[MAX_JOB_RESOURCES]; unsigned ports[MAX_JOB_RESOURCES], cants[MAX_JOB_RESOURCES];
    resource_t types[MAX_JOB_RESOURCES];
    unsigned n = get_granted_resources(NODE->owned_jobs, job_id, ips, ports, types, cants, MAX_JOB_RESOURCES);
    
    // 2. Dispatch RELEASE messages to free them
    for(unsigned i = 0; i < n; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "RELEASE %u %s %u\n", job_id, (types[i]==CPU)?"cpu":(types[i]==RAM)?"mem":"gpu", cants[i]);
        add_to_outbox(outbox, count, msg, -1, ips[i], ports[i]);
    }
    
    remove_owned_job(NODE->owned_jobs, job_id);

    pthread_mutex_unlock(&NODE->lock_owned);
}


/* ========================================================================= */
/* HANDLERS - EVENTS & MAINTENANCE                                           */
/* ========================================================================= */

/**
 * @brief Garbage Collector: Removes inactive nodes from 'known_nodes' and 
 * performs rollbacks on any jobs affected by those dead nodes.
 */
static void handle_check_deadnodes(ServerContext *ctx, out_msg_t *outbox, int *count) {
    node_data_t NODE = (node_data_t)ctx->mynode;
    char *dead_ips[10]; unsigned dead_ports[10];
    
    pthread_mutex_lock(&NODE->lock_known);
    unsigned dead_count = remove_inactive_nodes(NODE->known_nodes, dead_ips, dead_ports, 10);
    pthread_mutex_unlock(&NODE->lock_known);

    for (unsigned i = 0; i < dead_count; i++) {

        int zombie_fd = find_fd_by_ip_port(dead_ips[i], dead_ports[i]);
        
        // Free given local resources 
        if (zombie_fd != -1) {
            pthread_mutex_lock(&NODE->lock_local);
            free_all_resources_from_socket(NODE->resources, NODE->active_jobs, zombie_fd);
            pthread_mutex_unlock(&NODE->lock_local);
        }
    
        pthread_mutex_lock(&NODE->lock_owned);
        unsigned affected_jobs[20];
        unsigned affected_count = get_jobs_affected_by_dead_node(NODE->owned_jobs, dead_ips[i], dead_ports[i], affected_jobs, 20);
        
        for (unsigned j = 0; j < affected_count; j++) {
            unsigned job_id = affected_jobs[j];
            
            // ROLLBACK: Release resources from surviving nodes
            char *ips[MAX_JOB_RESOURCES]; unsigned ports[MAX_JOB_RESOURCES], cants[MAX_JOB_RESOURCES];
            resource_t types[MAX_JOB_RESOURCES];
            unsigned granted = get_granted_resources(NODE->owned_jobs, job_id, ips, ports, types, cants, MAX_JOB_RESOURCES);
            unsigned owner_socket = get_job_owner_socket(NODE->owned_jobs, job_id);
            
            for(unsigned k=0; k<granted; k++) {
                char msg[128];
                snprintf(msg, sizeof(msg), "RELEASE %u %s %u\n", job_id, (types[k]==CPU)?"cpu":(types[k]==RAM)?"mem":"gpu", cants[k]);
                add_to_outbox(outbox, count, msg, -1, ips[k], ports[k]);
            }
            
            char msg_deny[64];
            snprintf(msg_deny, sizeof(msg_deny), "JOB_DENIED %u\n", job_id);
            add_to_outbox(outbox, count, msg_deny, owner_socket, NULL, 0);
            
            remove_owned_job(NODE->owned_jobs, job_id);
        }
        free(dead_ips[i]); // Clean up the memory allocated by the Yellow Pages GC

        pthread_mutex_unlock(&NODE->lock_owned);
    }

    unsigned timed_out_jobs[20];
    pthread_mutex_lock(&NODE->lock_owned);
    unsigned timeout_count = collect_timed_out_jobs(NODE->owned_jobs, timed_out_jobs, 20);

    for (unsigned i = 0; i < timeout_count; i++) {
        unsigned job_id = timed_out_jobs[i];
        char *ips[MAX_JOB_RESOURCES]; unsigned ports[MAX_JOB_RESOURCES], cants[MAX_JOB_RESOURCES];
        resource_t types[MAX_JOB_RESOURCES];
        unsigned granted = get_granted_resources(NODE->owned_jobs, job_id, ips, ports, types, cants, MAX_JOB_RESOURCES);
        unsigned owner_socket = get_job_owner_socket(NODE->owned_jobs, job_id);

        if (*count + (int)granted + 1 > MAX_OUTBOX) break;

        for (unsigned j = 0; j < granted; j++) {
            char msg[128];
            snprintf(msg, sizeof(msg), "RELEASE %u %s %u\n", job_id, (types[j]==CPU)?"cpu":(types[j]==RAM)?"mem":"gpu", cants[j]);
            add_to_outbox(outbox, count, msg, -1, ips[j], ports[j]);
        }

        char msg_timeout[64];
        snprintf(msg_timeout, sizeof(msg_timeout), "JOB_TIMEOUT %u\n", job_id);
        add_to_outbox(outbox, count, msg_timeout, owner_socket, NULL, 0);
        remove_owned_job(NODE->owned_jobs, job_id);
    }
    pthread_mutex_unlock(&NODE->lock_owned);
}

/**
 * @brief Processes ANNOUNCE broadcasts from remote nodes to update the cluster directory.
 */
static void handle_node_announced(ServerContext *ctx, const char *sender_ip, const char *buffer) {
    node_data_t NODE = (node_data_t)ctx->mynode;
    
    // Inicializamos en 0 por si algún recurso no viene en el string
    unsigned port = 0, cpu = 0, ram = 0, gpu = 0;
    
    // Copiamos el buffer porque strtok modifica el string original
    char *dup = strdup(buffer);
    if (!dup) return;
    
    // 1. Extraemos "ANNOUNCE"
    char *token = strtok(dup, " "); 
    if (token != NULL && strcmp(token, "ANNOUNCE") == 0) {
        
        // 2. Extraemos el puerto
        token = strtok(NULL, " "); 
        if (token != NULL) {
            port = (unsigned)atoi(token);
            
            // 3. Iteramos sobre los recursos (pueden venir en cualquier orden)
            token = strtok(NULL, " ");
            while (token != NULL) {
                if (strncmp(token, "cpu:", 4) == 0) {
                    cpu = (unsigned)atoi(token + 4); // Leemos el número después de "cpu:"
                } 
                else if (strncmp(token, "mem:", 4) == 0 || strncmp(token, "ram:", 4) == 0) {
                    ram = (unsigned)atoi(token + 4);
                } 
                else if (strncmp(token, "gpu:", 4) == 0) {
                    gpu = (unsigned)atoi(token + 4);
                }
                
                token = strtok(NULL, " "); // Siguiente recurso
            }
            
            // 4. Actualizamos el directorio UDP de forma segura
            if (port > 0) {
                pthread_mutex_lock(&NODE->lock_known);
                update_known_node(NODE->known_nodes, sender_ip, port, cpu, ram, gpu);
                pthread_mutex_unlock(&NODE->lock_known);
            }
        }
    }
    
    free(dup);
}

/**
 * @brief Extrae los recursos locales y genera el string ANNOUNCE para UDP.
 */
static void handle_get_resources(ServerContext *ctx, out_msg_t *outbox, int *count) {
    node_data_t NODE = (node_data_t)ctx->mynode;
    unsigned cpu, gpu, ram;
    
    // 1. Protegemos la lectura física
    pthread_mutex_lock(&NODE->lock_local);
    get_local_resources(NODE->resources, &cpu, &gpu, &ram);
    pthread_mutex_unlock(&NODE->lock_local);
    
    // 2. Armamos el mensaje respetando el protocolo ANNOUNCE
    char msg[128];
    snprintf(msg, sizeof(msg), "ANNOUNCE %d cpu:%u mem:%u gpu:%u", ctx->port, cpu, ram, gpu);
    
    // 3. Lo metemos al outbox
    add_to_outbox(outbox, count, msg, -1, NULL, 0);
}


/* ========================================================================= */
/* MAIN DISPATCHER (ENTRY POINT)                                             */
/* ========================================================================= */

void resource_adapter_patch(ServerContext *ctx, char *SENDER_IP, unsigned SOCKET, const char *BUFFER, out_msg_t *outbox, int *outbox_count, JuaniAction action) {
    
    // Safety verification
    if (!ctx || !ctx->mynode) return;
    node_data_t NODE = (node_data_t)ctx->mynode;
    
    if (outbox_count != NULL) {
    *outbox_count = 0;
    }
    
    // Route based on event type
    switch (action) {
        case ACTION_GET_RESOURCES: 
            handle_get_resources(ctx, outbox, outbox_count);
            break;
        
        case ACTION_NEW_NODE_DISCOVERED:
            if (BUFFER != NULL) handle_node_announced(ctx, SENDER_IP, BUFFER);
            break;
            
        case ACTION_CHECK_DEADNODES:
            handle_check_deadnodes(ctx, outbox, outbox_count);
            break;
            
        case ACTION_DISCONNECTED:

            // 1. LOCAL RESOURCES RECLAMATION
            // If the dead socket was holding any of our physical resources, take them back.
            pthread_mutex_lock(&NODE->lock_local);
            free_all_resources_from_socket(NODE->resources, NODE->active_jobs, SOCKET);
            pthread_mutex_unlock(&NODE->lock_local);


            // 2. HEADLESS NODE RECOVERY (ORPHANED ERLANG CLEANUP)
            // Check if this dead socket was coordinating jobs (a local Erlang client).
            unsigned orphaned_jobs[50];
            pthread_mutex_lock(&NODE->lock_owned);
            unsigned orphan_count = get_jobs_by_owner_socket(NODE->owned_jobs, SOCKET, orphaned_jobs, 50);

            if (orphan_count > 0) {
                printf("[PATCHER] Local Erlang client (FD %u) disconnected. Cleaning %u orphaned jobs...\n", SOCKET, orphan_count);
                
                for (unsigned i = 0; i < orphan_count; i++) {
                    unsigned job_id = orphaned_jobs[i];
                    char *ips[MAX_JOB_RESOURCES]; unsigned ports[MAX_JOB_RESOURCES], cants[MAX_JOB_RESOURCES];
                    resource_t types[MAX_JOB_RESOURCES];
                    
                    // Extract resources that were successfully granted by remote nodes
                    unsigned granted = get_granted_resources(NODE->owned_jobs, job_id, ips, ports, types, cants, MAX_JOB_RESOURCES);
                    
                    // Dispatch RELEASE messages to free remote network resources
                    for (unsigned j = 0; j < granted; j++) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "RELEASE %u %s %u\n", job_id, (types[j]==CPU)?"cpu":(types[j]==RAM)?"mem":"gpu", cants[j]);
                        add_to_outbox(outbox, outbox_count, msg, -1, ips[j], ports[j]);
                    }
                    
                    // Purge the orphaned job from local memory
                    remove_owned_job(NODE->owned_jobs, job_id);
                }
            }
            pthread_mutex_unlock(&NODE->lock_owned);


            // 3. REMOTE NODE DEATH CLEANUP
            // If the dead socket was a remote cluster node, rollback jobs affected by its death.
            unsigned port = get_connection_port(SOCKET);

            if (port > 0 && SENDER_IP != NULL) {
                printf("[PATCHER] Instant TCP disconnect detected for %s:%u. Initiating rollback...\n", SENDER_IP, port);
                
                unsigned affected_jobs[20];
                pthread_mutex_lock(&NODE->lock_owned);
                unsigned affected_count = get_jobs_affected_by_dead_node(NODE->owned_jobs, SENDER_IP, port, affected_jobs, 20);
                
                for (unsigned j = 0; j < affected_count; j++) {
                    unsigned job_id = affected_jobs[j];
                    
                    
                    char *ips[MAX_JOB_RESOURCES]; unsigned ports[MAX_JOB_RESOURCES], cants[MAX_JOB_RESOURCES];
                    resource_t types[MAX_JOB_RESOURCES];
                    unsigned granted = get_granted_resources(NODE->owned_jobs, job_id, ips, ports, types, cants, MAX_JOB_RESOURCES);
                    unsigned owner_socket = get_job_owner_socket(NODE->owned_jobs, job_id);
                    
                    for(unsigned k = 0; k < granted; k++) {

                        // dont send realease to a dead nodes
                        if (strcmp(ips[k], SENDER_IP) == 0 && ports[k] == port) {
                            continue; 
                        }

                        char msg[128];
                        snprintf(msg, sizeof(msg), "RELEASE %u %s %u\n", job_id, (types[k]==CPU)?"cpu":(types[k]==RAM)?"mem":"gpu", cants[k]);
                        add_to_outbox(outbox, outbox_count, msg, -1, ips[k], ports[k]);
                    }
                    
                    char msg_deny[64];
                    snprintf(msg_deny, sizeof(msg_deny), "JOB_DENIED %u\n", job_id);
                    add_to_outbox(outbox, outbox_count, msg_deny, owner_socket, NULL, 0);
                    
                    remove_owned_job(NODE->owned_jobs, job_id);
                }
                pthread_mutex_unlock(&NODE->lock_owned);
            }

            break;
            
        case ACTION_RESPOND:
            if (BUFFER == NULL) break;
            
            if      (strncmp(BUFFER, "GET_NODES", 9)    == 0) handle_get_nodes_response(ctx, SOCKET, outbox, outbox_count);
            else if (strncmp(BUFFER, "JOB_REQUEST", 11) == 0) handle_job_request(ctx, SOCKET, BUFFER, outbox, outbox_count);
            else if (strncmp(BUFFER, "RESERVE", 7)      == 0) handle_reserve(ctx, SOCKET, BUFFER, outbox, outbox_count);
            else if (strncmp(BUFFER, "GRANTED", 7)      == 0) handle_granted(ctx, SENDER_IP, get_connection_port(SOCKET), SOCKET, BUFFER, outbox, outbox_count);
            else if (strncmp(BUFFER, "DENIED", 6)       == 0) handle_denied(ctx, BUFFER, outbox, outbox_count);
            else if (strncmp(BUFFER, "RELEASE", 7)      == 0) handle_release(ctx, SOCKET, BUFFER, outbox, outbox_count);
            else if (strncmp(BUFFER, "JOB_RELEASE", 11) == 0) handle_job_release(ctx, BUFFER, outbox, outbox_count);
            break;
            
        default:
            break;
    }
}
