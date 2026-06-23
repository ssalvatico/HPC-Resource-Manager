#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "queues.h"
#include "node-structures.h"
#include "tablahash.h"

#define ERROR   0
#define WAIT    1
#define SOLVED  2

#define SIZE    20  
#define TIMEOUT 15

typedef enum { FALSE = 0, TRUE = 1, STATES = 2 }        state_t;

typedef enum {
    CMD_RESERVE     = 0, // done
    CMD_RELEASE     = 1, // done
    CMD_ANNOUNCE    = 2, // done
    CMD_GET_NODES   = 3, // done
    CMD_JOB_REQUEST = 4, // done
    CMD_JOB_RELEASE = 5, // done
    CMD_GRANTED     = 6, // done
    CMD_DENIED      = 7, // done
    CMD_INVALID     = 8  // default case
} command_t;

//********************************************* */
// ELEMENTS FOR DATA STRUCTURES

typedef struct{
    char *      node_ip;
    unsigned    job_id;
    unsigned    resource_quantity;
    resource_t  resource_type;
}elem_active_job_t;

typedef struct{
    char *      node_ip;
    unsigned    node_port;
    unsigned    avail[RES_NUM];
    time_t      time_stamp;
}elem_known_nodes_t;

typedef struct{
    char *      node_ip;
    unsigned    job_id;
    unsigned    socket;
    unsigned    resource_quantity;
    resource_t  resource_type;
}elem_job_request_t;

//********************************************* */
// DATA STRUCTURES

typedef struct{
    unsigned    total[RES_NUM];
    unsigned    avail[RES_NUM];
    queue_t     request_queue[RES_NUM];     // queue of elem_job_request_t
}local_resources_t;

typedef TablaHash active_jobs_t;            // hash table of elem_active_job_t
typedef TablaHash known_nodes_t;            // hash table of elem_known_nodes_t
typedef TablaHash own_jobs_t;               // hash table of elem_own_job_t

//********************************************* */
// OWNED JOBS STRUCTURES

typedef struct{
    char *      node_ip;
    unsigned    node_port;
    unsigned    quantity;
    state_t     state;
}elem_petition_t;

typedef TablaHash petitions_t;

typedef struct{
    unsigned    job_id;
    petitions_t resource_petitions[RES_NUM];    
}elem_owned_job_t;

typedef TablaHash owned_jobs_t;


//********************************************* */
// NODE DATA STRUCTURE

typedef struct node_data_t_{
    active_jobs_t       active_jobs;
    known_nodes_t       known_nodes;
    owned_jobs_t        owned_jobs;
    local_resources_t * resources;
}node_data_t_;

typedef struct node_data_t_ * node_data_t;

/********************************************** */
// FUNCTION FOR DUPLICATING STRING

char * strduplicate(const char *s){
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if(p) memcpy(p, s, len);
    return p;
}
/********************************************** */
// MANAGING ELEMENTS FUNCTIONS - JOB REQUESTS

static void     dest_job_request    (elem_job_request_t * elem)                         { free(elem -> node_ip) ;free(elem); }
static void *   copy_job_request    (elem_job_request_t * elem)                         { return elem; }
static int      comp_job_request    (elem_job_request_t * a, elem_job_request_t * b)    { return (a -> job_id != b -> job_id); }

static elem_job_request_t * create_job_request( char * ip, unsigned id, unsigned socket, unsigned quantity, resource_t type){
    elem_job_request_t * out = malloc(sizeof(elem_job_request_t));

    if(out == NULL) { return NULL; }

    out -> job_id               = id;
    out -> node_ip              = strduplicate(ip);
    out -> socket               = socket;
    out -> resource_quantity    = quantity;
    out -> resource_type        = type;

    return out;
}

/********************************************** */
// MANAGING ELEMENTS FUNCTIONS - ACTIVE JOBS

static void     dest_active_job     (elem_active_job_t * elem)                          { free(elem -> node_ip); free(elem); }
static void *   copy_active_job     (elem_active_job_t * elem)                          { return elem; }
static int      comp_active_job     (elem_active_job_t * a, elem_active_job_t * b)      { return (a -> job_id != b -> job_id) || strcmp(a -> node_ip, b -> node_ip); }
static unsigned hash_active_job     (elem_active_job_t * elem)                          {
    unsigned hash = 5381;
    char * ip = elem->node_ip;
    while (*ip)
        hash = ((hash << 5) + hash) + (unsigned char)*ip++;
    return hash ^ elem->job_id;
}

static elem_active_job_t * create_elem_active_job( unsigned id, char * ip, unsigned quantity, resource_t type){
    elem_active_job_t * out = malloc(sizeof(elem_active_job_t));
    
    if(out == NULL) { return NULL; }

    out -> job_id               = id; 
    out -> node_ip              = strduplicate(ip);
    out -> resource_quantity    = quantity;
    out -> resource_type        = type;
    
    return out;
}

/********************************************** */
// MANAGING ELEMENTS FUNCTIONS - KNOWN NODES

static void     dest_known_node     (elem_known_nodes_t * elem)                         { free(elem -> node_ip) ; free(elem); }
static void *   copy_known_node     (elem_known_nodes_t * elem)                         { return elem; }
static int      comp_known_node     (elem_known_nodes_t * a, elem_known_nodes_t * b)    { return strcmp(a -> node_ip, b -> node_ip) != 0 || a->node_port != b -> node_port ; }
static unsigned hash_known_node     (elem_known_nodes_t * elem)                         {
    unsigned hash = 5381;
    char * ip = elem->node_ip;
    while (*ip)
        hash = ((hash << 5) + hash) + (unsigned char)*ip++;
    return hash ^ elem->node_port;
}

static elem_known_nodes_t * create_elem_known_node( char * ip, unsigned port, unsigned cpu, unsigned ram, unsigned gpu ){
    elem_known_nodes_t * out = malloc(sizeof(elem_known_nodes_t));
    
    if(out == NULL) { return NULL; }

    out -> node_ip      = strduplicate(ip);
    out -> node_port    = port;
    out -> avail[CPU]   = cpu;
    out -> avail[RAM]   = ram;
    out -> avail[GPU]   = gpu;
    out -> time_stamp   = time(NULL);
    return out;
}

/********************************************** */
// MANAGING ELEMENTS FUNCTIONS - OWNED JOBS

static void     dest_rec_petition   (elem_petition_t * elem)                            { free(elem -> node_ip); free(elem); }
static void *   copy_rec_petition   (elem_petition_t * elem)                            { return elem; }
static int      comp_rec_petition   (elem_petition_t * a, elem_petition_t * b)          { return strcmp(a -> node_ip, b -> node_ip) != 0 || (a -> node_port != b -> node_port); }
static unsigned hash_rec_petition   (elem_petition_t * elem)                            {
    unsigned hash = 5381;
    char * ip = elem->node_ip;
    while (*ip)
        hash = ((hash << 5) + hash) + (unsigned char)*ip++;
    return hash ^ elem->node_port;
}

static elem_petition_t *    create_elem_petition( char * ip, unsigned port, unsigned quantity ){
    elem_petition_t * out = malloc(sizeof(elem_petition_t));

    if(out == NULL) { return NULL; }

    out -> node_ip      = strduplicate(ip);
    out -> node_port    = port;
    out -> quantity     = quantity;
    out -> state        = FALSE;

    return out;
}

static petitions_t          create_petitions(){
    return tablahash_crear(11, (FuncionCopiadora)copy_rec_petition, (FuncionComparadora)comp_rec_petition, (FuncionDestructora)dest_rec_petition, (FuncionHash)hash_rec_petition);
}

static void                 delete_petitions(petitions_t petition){
    tablahash_destruir(petition);
}

static elem_owned_job_t *   create_owned_job(unsigned job_id){
    elem_owned_job_t * out = malloc(sizeof(elem_owned_job_t));

    if(out == NULL) { return NULL; }

    out -> job_id = job_id;
    for( int type = 0 ; type < RES_NUM ; type++ ) {
        out -> resource_petitions[type] = create_petitions() ; 
        if (out -> resource_petitions[type] == NULL){
            for(int i = 0 ; i < type ; i++){
                delete_petitions(out -> resource_petitions[i]);
            }

            free(out);
            return NULL;
        }
    }

    return out;
}

static void     dest_owned_job      (elem_owned_job_t * elem)                           {
    if(elem == NULL) { return; }
    
    for( int type = 0 ; type < RES_NUM ; type++ ) { delete_petitions(elem -> resource_petitions[type]); }

    free(elem);
}
static void *   copy_owned_job      (elem_owned_job_t * elem)                           { return elem; }
static int      comp_owned_job      (elem_owned_job_t * a, elem_owned_job_t * b)        { return a -> job_id != b -> job_id; } 
static unsigned hash_owned_job      (elem_owned_job_t * elem)                           { return elem -> job_id; }

static owned_jobs_t         create_owned_jobs(){
    return tablahash_crear(11, (FuncionCopiadora)copy_owned_job, (FuncionComparadora)comp_owned_job, (FuncionDestructora)dest_owned_job, (FuncionHash)hash_owned_job);
}

/********************************************** */
// LOCAL RESOURCES MANAGING 

static local_resources_t * create_local_resource( unsigned cpu,  unsigned gpu,  unsigned ram){
    local_resources_t * out = malloc(sizeof(local_resources_t));
    
    if (out == NULL)
        return NULL;
    
    out -> total[CPU] = cpu;
    out -> total[GPU] = gpu;
    out -> total[RAM] = ram;
    
    for( unsigned t = 0 ; t < RES_NUM ; t++) { out -> avail[t] = out -> total[t]; out -> request_queue[t] = queue_create(); }

    return out;
}

static void delete_local_resource(local_resources_t * resources){
    if (resources == NULL)
        return;
    
    for (unsigned idx = 0 ; idx < RES_NUM ; idx++) { queue_destroy(resources -> request_queue[idx], (FuncionDestructora)dest_job_request); }

    free(resources);
}

/********************************************** */
// JOB REQUEST MANAGING

static unsigned new_job_request (local_resources_t * resources, active_jobs_t active_jobs, char * ip, unsigned id, unsigned socket, unsigned quantity, resource_t type){
    if(resources -> avail[type] < quantity){
        elem_job_request_t * new = create_job_request(ip, id, socket, quantity, type);
        if(new != NULL)
            resources -> request_queue[type] = queue_add(resources -> request_queue[type], new, (FuncionCopiadora)copy_job_request);
        return 0;
    }
    
    resources -> avail[type] -= quantity;
    tablahash_insertar(active_jobs, create_elem_active_job(id, ip, quantity, type));

    return id;
}

int chk_job_request (node_data_t NODE, char * OUT, unsigned OUT_SIZE, unsigned *out_socket){
    queue_t                 act         = NULL;
    elem_job_request_t  *   act_elem    = NULL;
    int out = -1;    
    
    local_resources_t * resources   = NODE -> resources;
    active_jobs_t       active_jobs = NODE -> active_jobs;

    
    resource_t init = rand() % RES_NUM;
    resource_t type;

    for(resource_t idx = 0; idx < RES_NUM && out == -1 ; idx++){
        
        type = (init + idx) % RES_NUM;
        act = resources -> request_queue[type];
        do{ 
            if(queue_empty(act)) { break; }

            act_elem = act -> data;

            if(resources -> avail[type] < act_elem -> resource_quantity) { act = act -> next; continue; }

            resources -> avail[type] -= act_elem -> resource_quantity;

            out = act_elem -> job_id;

            if (out_socket != NULL)
            *out_socket = act_elem -> socket;

            tablahash_insertar(active_jobs, create_elem_active_job(act_elem -> job_id, act_elem -> node_ip, act_elem ->resource_quantity, type));

            resources -> request_queue[type] = queue_delete(resources -> request_queue[type], act_elem, (FuncionDestructora)dest_job_request, (FuncionComparadora)comp_job_request);

            break;
        }while(act != NULL);
    }

    snprintf(OUT, OUT_SIZE, "GRANTED %d\n", out);

    return out;
}

/********************************************** */
// JOB REQUEST MANAGING

static void del_active_job (local_resources_t * resources, active_jobs_t jobs, char * ip, unsigned job_id){
    
    elem_active_job_t * dummy = create_elem_active_job(job_id, ip, 0, 0);
    if( dummy == NULL) return;

    elem_active_job_t * job_actual = tablahash_buscar(jobs, dummy);

    if(job_actual == NULL) { dest_active_job(dummy) ; return ;}

    resources->avail[job_actual -> resource_type] += job_actual -> resource_quantity;

    tablahash_eliminar(jobs, dummy);

    dest_active_job(dummy);
}

/********************************************** */
// KNOWN NODES MANAGING

static unsigned cont = 0;
static elem_known_nodes_t ** nodes_to_eliminate;

static void     known_nodes_del_inactive        (elem_known_nodes_t * node){
    if (difftime(time(NULL), node->time_stamp) > TIMEOUT) nodes_to_eliminate[cont++] = node;
}

void            known_nodes_del_inactive_nodes  (node_data_t node){
    known_nodes_t nodes = node -> known_nodes;
    
    cont = 0;
    unsigned size = tablahash_nelems(nodes);

    if(size == 0)
        return;

    nodes_to_eliminate = malloc(sizeof(elem_known_nodes_t *) * size);
    
    if(nodes_to_eliminate == NULL)
        return;

    tablahash_visitar(nodes, (FuncionVisitante)known_nodes_del_inactive);

    for(unsigned idx = 0 ; idx < cont ; idx++) { tablahash_eliminar(nodes, nodes_to_eliminate[idx]); }

    free(nodes_to_eliminate);
}

static char *   append_out_buffer;
static unsigned append_out_size;
static unsigned append_out_disp;

static void append_node(elem_known_nodes_t * node){
    if(append_out_size <= append_out_disp)
        return;

    int n = snprintf(
        append_out_buffer + append_out_disp,
        append_out_size   - append_out_disp,
        "%s:%u:cpu:%u:mem:%u:gpu:%u;",
        node->node_ip,
        node->node_port,
        node->avail[CPU],
        node->avail[RAM],
        node->avail[GPU]
    );

    if(n > 0)
        append_out_disp += n;
}


static unsigned granted_job_cond;

static void update_granted_job_cond(elem_petition_t * petition){
    granted_job_cond = granted_job_cond && (petition -> state == TRUE); 
}

static int check_job_granted(own_jobs_t jobs, unsigned job_id){
    granted_job_cond = TRUE;
        
    elem_owned_job_t * dummy = create_owned_job(job_id);
    elem_owned_job_t * act = tablahash_buscar(jobs, dummy);
    dest_owned_job(dummy);
    if (act == NULL)
        return FALSE;
    
    for( unsigned type = 0 ; type < RES_NUM && granted_job_cond; type++){
        tablahash_visitar(act -> resource_petitions[type], (FuncionVisitante)update_granted_job_cond);
    }

    return granted_job_cond;
}


/********************************************** */
// CONTROL FUNCTIONS

static char **      ip_out;
static resource_t * type_out;
static unsigned *   quantity_out;
static unsigned     out_cont;
static unsigned     out_size; 
static resource_t   act_type;

node_data_t node_init   (unsigned cpu, unsigned gpu, unsigned ram){
    node_data_t node = malloc(sizeof(node_data_t_));

    node -> active_jobs = tablahash_crear(11, (FuncionCopiadora)copy_active_job, (FuncionComparadora)comp_active_job, (FuncionDestructora)dest_active_job, (FuncionHash)hash_active_job);
    node -> known_nodes = tablahash_crear(11, (FuncionCopiadora)copy_known_node, (FuncionComparadora)comp_known_node, (FuncionDestructora)dest_known_node, (FuncionHash)hash_known_node);
    node -> owned_jobs  = create_owned_jobs();
    node -> resources   = create_local_resource(cpu, gpu, ram);
    
    return node;
}

void        node_dest   (node_data_t node){
    local_resources_t * resources   = node -> resources;
    active_jobs_t       active_jobs = node -> active_jobs;
    known_nodes_t       known_nodes = node -> known_nodes;
    owned_jobs_t        owned_jobs  = node -> owned_jobs;

    delete_local_resource(resources);
    tablahash_destruir(active_jobs);
    tablahash_destruir(known_nodes);
    tablahash_destruir(owned_jobs);

    free(node);
}

void get_petition_data(elem_petition_t * act){
    quantity_out[out_cont] = act -> quantity;
    type_out    [out_cont] = act_type;
    ip_out      [out_cont] = act -> node_ip;
    
    out_cont++;
}

unsigned get_job_data(node_data_t NODE, unsigned job_id, char * arr_ip[], resource_t arr_type[], unsigned quantity[], const unsigned size){
    out_cont = 0;
    out_size = size;

    ip_out          = arr_ip;
    type_out        = arr_type;
    quantity_out    = quantity;

    elem_owned_job_t * dummy    = create_owned_job(job_id);
    elem_owned_job_t * job      = tablahash_buscar(NODE -> owned_jobs, dummy);

    dest_owned_job(dummy);

    if (job == NULL) return 0;

    petitions_t act_petitions;    

    for ( unsigned type = 0 ; type < RES_NUM && out_cont < out_size; type++){
        act_type = type;
        act_petitions = job -> resource_petitions[type];
        tablahash_visitar(act_petitions, (FuncionVisitante)get_petition_data);
    }

    return out_cont;
}

void get_local_resources(node_data_t NODE, unsigned * cpu_quantity, unsigned * gpu_quantity, unsigned * mem_quantity){
    if(cpu_quantity) { * cpu_quantity = (NODE -> resources -> avail)[CPU]; }    
    if(gpu_quantity) { * gpu_quantity = (NODE -> resources -> avail)[GPU]; }
    if(mem_quantity) { * mem_quantity = (NODE -> resources -> avail)[RAM]; }    
}

// ==================================================================
// COMMAND FUNCTIONS

void command_announce   (node_data_t NODE, char * ip, unsigned port, unsigned ram, unsigned cpu, unsigned gpu) { tablahash_insertar(NODE -> known_nodes, create_elem_known_node(ip, port, cpu, ram, gpu)); }

void command_get_nodes  (node_data_t NODE, char * OUT_BUFFER, unsigned OUT_SIZE){

    append_out_buffer = OUT_BUFFER;
    append_out_size   = OUT_SIZE;

    append_out_disp     = snprintf(append_out_buffer, append_out_size, "NODES ");

    tablahash_visitar(NODE -> known_nodes, (FuncionVisitante)append_node);

    if(append_out_disp > 6 && append_out_disp < OUT_SIZE){
        append_out_buffer[append_out_disp - 1]  = '\n';
        append_out_buffer[append_out_disp]      = '\0';
    }
} 

void command_job_request(node_data_t NODE, unsigned job_id, char ** ARR_IP, unsigned * ARR_PORTS, resource_t * ARR_TYPE, unsigned * ARR_QUANT, unsigned size){
    elem_petition_t *   new_petition;
    elem_owned_job_t *  new_job = create_owned_job(job_id);

    for ( unsigned idx = 0 ; idx < size ; idx++){
        new_petition = create_elem_petition(ARR_IP[idx], ARR_PORTS[idx], ARR_QUANT[idx]);        
        tablahash_insertar(new_job -> resource_petitions[ARR_TYPE[idx]], new_petition);
    }

    tablahash_insertar(NODE -> owned_jobs, new_job);
}

void command_job_release(node_data_t NODE, unsigned job_id){
    elem_owned_job_t * dummy = create_owned_job(job_id);
    
    elem_owned_job_t * data  = tablahash_buscar(NODE -> owned_jobs, dummy);
    
    if(!data){ dest_owned_job(dummy); return ;}                 
    
    tablahash_eliminar(NODE -> owned_jobs, dummy);
    
    dest_owned_job(dummy);
}

void command_release    (node_data_t NODE, char * NODE_IP, unsigned job_id, resource_t resource, unsigned quantity){
    del_active_job(NODE -> resources, NODE -> active_jobs, NODE_IP, job_id);   
}

void command_denied     (node_data_t NODE, unsigned job_id){
    elem_owned_job_t * dummy = create_owned_job(job_id);
    if(!dummy)  return;

    tablahash_eliminar(NODE -> owned_jobs, dummy);
    dest_owned_job(dummy);    
}

void command_granted    (node_data_t NODE, unsigned job_id, char *  NODE_IP, unsigned NODE_PORT){ // preguntar lucio
    elem_owned_job_t * dummy1 = create_owned_job(job_id);
    if(!dummy1) return; 
    
    elem_owned_job_t * job = tablahash_buscar(NODE -> owned_jobs, dummy1);
    dest_owned_job(dummy1);
    if(!job)    return;

    elem_petition_t * dummy2 = create_elem_petition(NODE_IP, NODE_PORT, 0);
    if(!dummy2) return;

    unsigned cond = 0;
    elem_petition_t * act;

    for (unsigned type = 0 ; type < RES_NUM ; type++ ){
        act = tablahash_buscar((job -> resource_petitions)[type], dummy2);
        if(act == NULL)
            continue;

        act -> state = TRUE;
        cond = 1;
    }

    dest_rec_petition(dummy2);
    
    if(!cond) return;
    
    if(check_job_granted(NODE -> owned_jobs, job_id))  
        snprintf(OUT, OUT_SIZE, "JOB_GRANTED %u", job_id);    
}

void command_reserve    (node_data_t NODE, char * NODE_IP, unsigned SOCKET, unsigned job_id, resource_t type, unsigned amount){ // preguntar lucio
    new_job_request(NODE -> resources, NODE -> active_jobs, NODE_IP, job_id, SOCKET, amount, type);
    snprintf(OUT, OUT_SIZE, "GRANTED %u\n", out);
}