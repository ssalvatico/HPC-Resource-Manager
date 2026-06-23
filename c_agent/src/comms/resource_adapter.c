#include "../../include/resources/node-structures.h"
#include "../../include/comms/server_types.h"
#include "../../include/comms/event_handler.h"
#include "../../include/comms/resource_adapter.h"
#include <string.h>
#include <stdio.h>

#include <pthread.h>

// La memoria estatica del adaptador de red
static job_record_t active_jobs_registry[MAX_TRACKED_JOBS] = {0};

// ==========================================
// LOCKS GLOBALES
// ==========================================
pthread_mutex_t juani_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t registry_rwlock = PTHREAD_RWLOCK_INITIALIZER; // Lock para la tabla de red

// ==========================================
// ADAPTADOR DE RECURSOS PRINCIPAL
// ==========================================
void resource_adapter_patch(ServerContext* ctx, node_data_t NODE, char * SENDER_IP, unsigned SOCKET, const char * BUFFER, out_msg_t * outbox, int * outbox_count, JuaniAction action) {
    char juani_out[BUFFER_SIZE] = {0};
    if (outbox_count) *outbox_count = 0;

    // ----------------------------------------------------------------------
    // 1. TIMERS AND UDP DISCOVERY
    // ----------------------------------------------------------------------
    if (action == ACTION_GET_RESOURCES) {
        unsigned cpu, gpu, ram;
        pthread_mutex_lock(&juani_mutex); 
        get_local_resources(NODE, &cpu, &gpu, &ram);
        pthread_mutex_unlock(&juani_mutex);
        
        sprintf(outbox[0].message, "ANNOUNCE %d cpu:%u mem:%u gpu:%u\n", ctx->port, cpu, ram, gpu);
        *outbox_count = 1;
        return;
    }
    
    if (action == ACTION_CHECK_DEADNODES) {
        char dead_ips[50][16]; 
        unsigned dead_count = 0;

        pthread_mutex_lock(&juani_mutex);
        // dead_count = command_check_dead_nodes(NODE, dead_ips, 50); 
        pthread_mutex_unlock(&juani_mutex);

        // --- CACHÉ DE FASE 2 ---
        unsigned affected_jobs_cache[MAX_TRACKED_JOBS];
        int affected_count = 0;
        unsigned loopback_jobs[MAX_TRACKED_JOBS * MAX_NODES_PER_JOB];
        unsigned loopback_amounts[MAX_TRACKED_JOBS * MAX_NODES_PER_JOB];
        resource_t loopback_types[MAX_TRACKED_JOBS * MAX_NODES_PER_JOB];
        int loopback_count = 0;

        // FASE 1: LOCK DE RED (Solo manipular outbox y memoria de red)
        pthread_rwlock_wrlock(&registry_rwlock);
        for (unsigned d = 0; d < dead_count; d++) {
            const char* dead_ip = dead_ips[d];

            for (int i = 0; i < MAX_TRACKED_JOBS; i++) {
                if (active_jobs_registry[i].is_active) {
                    
                    int job_is_affected = 0;
                    
                    for (unsigned j = 0; j < active_jobs_registry[i].petition_count; j++) {
                        if (strcmp(active_jobs_registry[i].petitions[j].ip, dead_ip) == 0) {
                            job_is_affected = 1;
                            break;
                        }
                    }

                    if (job_is_affected) {
                        unsigned affected_job_id = active_jobs_registry[i].job_id;

                        for (unsigned j = 0; j < active_jobs_registry[i].petition_count; j++) {
                            char* ip_destino = active_jobs_registry[i].petitions[j].ip;
                            
                            if (strcmp(ip_destino, dead_ip) != 0) {
                                // === OPTIMIZACIÓN LOOPBACK (Guardado en Caché) ===
                                if (strcmp(ip_destino, ctx->lan_ip) == 0) {
                                    loopback_jobs[loopback_count] = affected_job_id;
                                    loopback_amounts[loopback_count] = active_jobs_registry[i].petitions[j].amount;
                                    loopback_types[loopback_count] = active_jobs_registry[i].petitions[j].type;
                                    loopback_count++;
                                } else {
                                    char res_str[4];
                                    if      (active_jobs_registry[i].petitions[j].type == CPU) strcpy(res_str, "cpu");
                                    else if (active_jobs_registry[i].petitions[j].type == GPU) strcpy(res_str, "gpu");
                                    else if (active_jobs_registry[i].petitions[j].type == RAM) strcpy(res_str, "mem");

                                    sprintf(outbox[*outbox_count].message, "RELEASE %u %s %u\n", affected_job_id, res_str, active_jobs_registry[i].petitions[j].amount);
                                    strcpy(outbox[*outbox_count].target_ip, ip_destino);
                                    outbox[*outbox_count].target_port = active_jobs_registry[i].petitions[j].port;
                                    outbox[*outbox_count].target_fd = -1;
                                    (*outbox_count)++;
                                }
                            }
                        }

                        // B. Avisar a Erlang
                        sprintf(outbox[*outbox_count].message, "JOB_DENIED %u\n", affected_job_id);
                        outbox[*outbox_count].target_fd = ctx->active_erlang_fd; 
                        (*outbox_count)++;

                        // C. Guardar el trabajo afectado en caché
                        affected_jobs_cache[affected_count++] = affected_job_id;

                        active_jobs_registry[i].is_active = 0;
                    }
                }
            }
        }
        pthread_rwlock_unlock(&registry_rwlock); // <-- LIBERACIÓN DEL CANDADO DE RED

        // FASE 2: LOCK DE NEGOCIO (Aplicar los cambios cacheados sin estorbar la red)
        pthread_mutex_lock(&juani_mutex);
        for (int k = 0; k < loopback_count; k++) {
            // command_release(NODE, loopback_jobs[k], loopback_amounts[k], loopback_types[k]);
        }
        for (int k = 0; k < affected_count; k++) {
            // command_job_release(NODE, affected_jobs_cache[k]);
        }
        pthread_mutex_unlock(&juani_mutex);

        return;
    }
    
    if (action == ACTION_DISCONNECTED) {
        const char* dead_ip = SENDER_IP; 

        // --- CACHÉ DE FASE 2 ---
        unsigned affected_jobs_cache[MAX_TRACKED_JOBS];
        int affected_count = 0;
        unsigned loopback_jobs[MAX_TRACKED_JOBS * MAX_NODES_PER_JOB];
        unsigned loopback_amounts[MAX_TRACKED_JOBS * MAX_NODES_PER_JOB];
        resource_t loopback_types[MAX_TRACKED_JOBS * MAX_NODES_PER_JOB];
        int loopback_count = 0;

        // FASE 1: LOCK DE RED 
        pthread_rwlock_wrlock(&registry_rwlock);
        for (int i = 0; i < MAX_TRACKED_JOBS; i++) {
            if (active_jobs_registry[i].is_active) {
                
                int job_is_affected = 0;
                
                for (unsigned j = 0; j < active_jobs_registry[i].petition_count; j++) {
                    if (strcmp(active_jobs_registry[i].petitions[j].ip, dead_ip) == 0) {
                        job_is_affected = 1;
                        break;
                    }
                }

                if (job_is_affected) {
                    unsigned affected_job_id = active_jobs_registry[i].job_id;

                    for (unsigned j = 0; j < active_jobs_registry[i].petition_count; j++) {
                        char* ip_destino = active_jobs_registry[i].petitions[j].ip;
                        
                        if (strcmp(ip_destino, dead_ip) != 0) {
                            // === OPTIMIZACIÓN LOOPBACK (Guardado en Caché) ===
                            if (strcmp(ip_destino, ctx->lan_ip) == 0) {
                                loopback_jobs[loopback_count] = affected_job_id;
                                loopback_amounts[loopback_count] = active_jobs_registry[i].petitions[j].amount;
                                loopback_types[loopback_count] = active_jobs_registry[i].petitions[j].type;
                                loopback_count++;
                            } else {
                                char res_str[4];
                                if      (active_jobs_registry[i].petitions[j].type == CPU) strcpy(res_str, "cpu");
                                else if (active_jobs_registry[i].petitions[j].type == GPU) strcpy(res_str, "gpu");
                                else if (active_jobs_registry[i].petitions[j].type == RAM) strcpy(res_str, "mem");

                                sprintf(outbox[*outbox_count].message, "RELEASE %u %s %u\n", affected_job_id, res_str, active_jobs_registry[i].petitions[j].amount);
                                strcpy(outbox[*outbox_count].target_ip, ip_destino);
                                outbox[*outbox_count].target_port = active_jobs_registry[i].petitions[j].port;
                                outbox[*outbox_count].target_fd = -1;
                                (*outbox_count)++;
                            }
                        }
                    }

                    // B. Avisar a Erlang
                    sprintf(outbox[*outbox_count].message, "JOB_DENIED %u\n", affected_job_id);
                    outbox[*outbox_count].target_fd = ctx->active_erlang_fd; 
                    (*outbox_count)++;

                    // C. Guardar el trabajo afectado en caché
                    affected_jobs_cache[affected_count++] = affected_job_id;

                    active_jobs_registry[i].is_active = 0;
                }
            }
        }
        pthread_rwlock_unlock(&registry_rwlock); // <-- LIBERACIÓN DEL CANDADO DE RED

        // FASE 2: LOCK DE NEGOCIO 
        pthread_mutex_lock(&juani_mutex);
        for (int k = 0; k < loopback_count; k++) {
            // command_release(NODE, loopback_jobs[k], loopback_amounts[k], loopback_types[k]);
        }
        for (int k = 0; k < affected_count; k++) {
            // command_job_release(NODE, affected_jobs_cache[k]);
        }
        pthread_mutex_unlock(&juani_mutex);

        return;
    }
    
    if (action == ACTION_NEW_NODE_DISCOVERED) {
        unsigned discovered_port, cpu, gpu, ram;
        if(parse_announce(BUFFER, &discovered_port, &cpu, &ram, &gpu)){
            //command_announce(NODE, SENDER_IP, discovered_port, cpu, ram, gpu);
        }
        return;
    }

    // ----------------------------------------------------------------------
    // 2. EVENTOS TCP Y PRE-PARSING DE MENSAJES (Erlang y otros Nodos)
    // ----------------------------------------------------------------------
    
    if (action == ACTION_RESPOND && BUFFER != NULL) {

        // --- CASO 0: GET_NODES ---
        if (strncmp(BUFFER, "GET_NODES", 9) == 0) {
            pthread_mutex_lock(&juani_mutex);
            // command_get_nodes(NODE, juani_out, BUFFER_SIZE); 
            pthread_mutex_unlock(&juani_mutex);
            
            if (strlen(juani_out) > 0) {
                strcpy(outbox[0].message, juani_out);
                outbox[0].target_fd = SOCKET;
                *outbox_count = 1;
            }
            return;
        }

        // --- CASO 1: RESERVE ---
        else if (strncmp(BUFFER, "RESERVE", 7) == 0) {
            unsigned job_id, quantity;
            resource_t type;
            
            if (parse_reserve_release(BUFFER, &job_id, &type, &quantity)) {
                pthread_mutex_lock(&juani_mutex);
                // unsigned granted = command_reserve(NODE, SENDER_IP, SOCKET, job_id, type, quantity);
                pthread_mutex_unlock(&juani_mutex);
                
                unsigned granted = 1; // Dummy
                
                if (granted) {
                    sprintf(outbox[0].message, "GRANTED %u\n", job_id);
                    outbox[0].target_fd = SOCKET; 
                    *outbox_count = 1;
                }
            }
            return;
        }

        // --- CASO 2: RELEASE ---
        else if (strncmp(BUFFER, "RELEASE", 7) == 0) {
            unsigned job_id, quantity;
            resource_t type;
            
            if (parse_reserve_release(BUFFER, &job_id, &type, &quantity)) {
                pthread_mutex_lock(&juani_mutex);
                // command_release(NODE, SENDER_IP, job_id, type, quantity);
                pthread_mutex_unlock(&juani_mutex);
            }
            return;
        }

        // --- CASO 3: JOB_REQUEST ---
        else if (strncmp(BUFFER, "JOB_REQUEST", 11) == 0) {
            unsigned job_id;
            parsed_petition_t petitions[MAX_NODES_PER_JOB];
            unsigned petition_count = 0;
            
            if (parse_job_request(BUFFER, &job_id, petitions, &petition_count, MAX_NODES_PER_JOB)) {
                
                char* ips_ptrs[MAX_NODES_PER_JOB]; 
                unsigned ports[MAX_NODES_PER_JOB];
                resource_t types[MAX_NODES_PER_JOB];
                unsigned amounts[MAX_NODES_PER_JOB];
        
                for(unsigned i = 0; i < petition_count; i++) {
                    ips_ptrs[i] = petitions[i].ip; 
                    ports[i]    = petitions[i].port;
                    types[i]    = petitions[i].type;
                    amounts[i]  = petitions[i].amount;          
                }

                pthread_mutex_lock(&juani_mutex);
                // command_job_request(NODE, job_id, ips_ptrs, ports, types, amounts, petition_count);
                pthread_mutex_unlock(&juani_mutex);

                // Esta función tiene su propio rwlock adentro
                register_job(job_id, petitions, petition_count);

                for (unsigned i = 0; i < petition_count; i++) {
                    char res_str[4];
                    if      (petitions[i].type == CPU) strcpy(res_str, "cpu");
                    else if (petitions[i].type == GPU) strcpy(res_str, "gpu");
                    else if (petitions[i].type == RAM) strcpy(res_str, "mem");

                    sprintf(outbox[i].message, "RESERVE %u %s %u\n", job_id, res_str, petitions[i].amount);
                    strcpy(outbox[i].target_ip, petitions[i].ip);
                    outbox[i].target_port = petitions[i].port;
                    outbox[i].target_fd = -1;
                }
                *outbox_count = petition_count;
            }
            return;
        }

        // --- CASO 4: GRANTED ---
        else if (strncmp(BUFFER, "GRANTED", 7) == 0) {
            unsigned job_id;
            
            if (parse_single_id_cmd(BUFFER, &job_id)) {
                pthread_mutex_lock(&juani_mutex);
                // unsigned job_fully_granted = command_granted(NODE, job_id, SENDER_IP, puerto); 
                pthread_mutex_unlock(&juani_mutex);
                
                int job_fully_granted = 0; // Dummy

                if (job_fully_granted) {
                    sprintf(outbox[0].message, "JOB_GRANTED %u\n", job_id);
                    outbox[0].target_fd = SOCKET; 
                    *outbox_count = 1;
                }
            }
            return;
        }

        // --- CASO 5: JOB_RELEASE o DENIED ---
        else if (strncmp(BUFFER, "JOB_RELEASE", 11) == 0 || strncmp(BUFFER, "DENIED", 6) == 0) {
            unsigned job_id;
            
            if (parse_single_id_cmd(BUFFER, &job_id)) {
                
                // --- CACHE DE FASE 2 ---
                unsigned loopback_amounts[MAX_NODES_PER_JOB];
                resource_t loopback_types[MAX_NODES_PER_JOB];
                int loopback_count = 0;

                // FASE 1: LOCK DE ESCRITURA 
                pthread_rwlock_wrlock(&registry_rwlock);
                for (int i = 0; i < MAX_TRACKED_JOBS; i++) {
                    if (active_jobs_registry[i].is_active && active_jobs_registry[i].job_id == job_id) {
                        
                        for (unsigned j = 0; j < active_jobs_registry[i].petition_count; j++) {
                            char* ip_destino = active_jobs_registry[i].petitions[j].ip;
                            
                            if (strcmp(ip_destino, SENDER_IP) != 0) {
                                // === OPTIMIZACIÓN LOOPBACK (Guardado en Caché) ===
                                if (strcmp(ip_destino, ctx->lan_ip) == 0) {
                                    loopback_amounts[loopback_count] = active_jobs_registry[i].petitions[j].amount;
                                    loopback_types[loopback_count] = active_jobs_registry[i].petitions[j].type;
                                    loopback_count++;
                                } else {
                                    char res_str[4];
                                    if      (active_jobs_registry[i].petitions[j].type == CPU) strcpy(res_str, "cpu");
                                    else if (active_jobs_registry[i].petitions[j].type == GPU) strcpy(res_str, "gpu");
                                    else if (active_jobs_registry[i].petitions[j].type == RAM) strcpy(res_str, "mem");

                                    sprintf(outbox[*outbox_count].message, "RELEASE %u %s %u\n", job_id, res_str, active_jobs_registry[i].petitions[j].amount);
                                    strcpy(outbox[*outbox_count].target_ip, ip_destino);
                                    outbox[*outbox_count].target_port = active_jobs_registry[i].petitions[j].port;
                                    outbox[*outbox_count].target_fd = -1;
                                    (*outbox_count)++;
                                }
                            }
                        }
                        active_jobs_registry[i].is_active = 0;
                        break; 
                    }
                }
                pthread_rwlock_unlock(&registry_rwlock); // <-- LIBERACIÓN DEL CANDADO DE RED

                if (strncmp(BUFFER, "DENIED", 6) == 0) {
                    sprintf(outbox[*outbox_count].message, "JOB_DENIED %u\n", job_id);
                    outbox[*outbox_count].target_fd = ctx->active_erlang_fd; 
                    (*outbox_count)++;
                }

                // FASE 2: LOCK DE NEGOCIO 
                pthread_mutex_lock(&juani_mutex);
                for (int k = 0; k < loopback_count; k++) {
                    // command_release(NODE, job_id, loopback_amounts[k], loopback_types[k]);
                }

                if (strncmp(BUFFER, "JOB_RELEASE", 11) == 0) {
                    // command_job_release(NODE, job_id);
                } else {
                    // command_denied(NODE, job_id);
                }
                pthread_mutex_unlock(&juani_mutex);
            }
            return;
        }
    }
}

// =====================================================
// PARSING 

int parse_reserve_release(const char* buffer, unsigned* job_id, resource_t* type, unsigned* quantity) {
    char cmd[32];
    char res_str[32];
    
    if (sscanf(buffer, "%31s %u %31s %u", cmd, job_id, res_str, quantity) == 4) {
        if      (strcmp(res_str, "cpu") == 0) *type = CPU;
        else if (strcmp(res_str, "gpu") == 0) *type = GPU;
        else if (strcmp(res_str, "mem") == 0) *type = RAM;
        else return 0;
        return 1;
    }
    return 0;
}

int parse_announce(const char* buffer, unsigned* port, unsigned* cpu, unsigned* ram, unsigned* gpu) {
    if (sscanf(buffer, "ANNOUNCE %u cpu:%u mem:%u gpu:%u", port, cpu, ram, gpu) == 4) {
        return 1;
    }
    return 0;
}

int parse_job_request(const char* buffer, unsigned* job_id, parsed_petition_t* petitions, unsigned* petition_count, unsigned max_petitions) {
    char cmd[32];
    
    if (sscanf(buffer, "%31s %u", cmd, job_id) != 2) return 0;
    if (strcmp(cmd, "JOB_REQUEST") != 0) return 0;
    
    *petition_count = 0;
    
    const char *p = strchr(buffer, '@');
    while (p != NULL && *petition_count < max_petitions) {
        char ip[16];
        unsigned port;
        char res_str[32];
        unsigned amount;
        
        if (sscanf(p, "@%15[^:]:%u:%31[^:]:%u", ip, &port, res_str, &amount) == 4) {
            strcpy(petitions[*petition_count].ip, ip);
            petitions[*petition_count].port = port;
            petitions[*petition_count].amount = amount;
            
            if      (strcmp(res_str, "cpu") == 0) petitions[*petition_count].type = CPU;
            else if (strcmp(res_str, "gpu") == 0) petitions[*petition_count].type = GPU;
            else if (strcmp(res_str, "mem") == 0) petitions[*petition_count].type = RAM;
            
            (*petition_count)++;
        }
        p = strchr(p + 1, '@');
    }
    
    return (*petition_count > 0) ? 1 : 0;
}

int parse_single_id_cmd(const char* buffer, unsigned* job_id) {
    char cmd[32];
    if (sscanf(buffer, "%31s %u", cmd, job_id) == 2) {
        return 1;
    }
    return 0;
}

// ==========================================================
// AUX FUNCTIONS

void register_job(unsigned job_id, parsed_petition_t* petitions, unsigned count) {
    // LOCK DE ESCRITURA: Encontramos un espacio libre y escribimos en él
    pthread_rwlock_wrlock(&registry_rwlock);
    for (int i = 0; i < MAX_TRACKED_JOBS; i++) {
        if (!active_jobs_registry[i].is_active) {
            active_jobs_registry[i].job_id = job_id;
            active_jobs_registry[i].petition_count = (count < MAX_NODES_PER_JOB) ? count : MAX_NODES_PER_JOB;
            
            for (unsigned j = 0; j < active_jobs_registry[i].petition_count; j++) {
                active_jobs_registry[i].petitions[j] = petitions[j];
            }
            
            active_jobs_registry[i].is_active = 1;
            break;
        }
    }
    pthread_rwlock_unlock(&registry_rwlock);
}