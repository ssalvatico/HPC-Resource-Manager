#include "../../include/resources/known_nodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========================================================================= */
/* INTERNAL STRUCTURES                                                       */
/* ========================================================================= */

/**
 * @brief Represents a remote node discovered via UDP broadcast.
 * Instances of this structure are stored in the 'known_nodes_t' hash table.
 * It acts as a directory entry (Yellow Pages) for the cluster.
 */
typedef struct {
    char * node_ip;             /**< IP address of the remote node */
    unsigned node_port;         /**< TCP listening port of the remote node */
    unsigned avail[RES_NUM];    /**< Last reported available resources (CPU, GPU, RAM) */
    time_t time_stamp;          /**< Timestamp of the last received ANNOUNCE message */
} elem_known_nodes_t;


/* ========================================================================= */
/* HASH TABLE INTERNAL HELPERS                                               */
/* ========================================================================= */

static char * strduplicate(const char *s){
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if(p) memcpy(p, s, len);
    return p;
}

static void dest_known_node (elem_known_nodes_t * elem) { 
    free(elem->node_ip); 
    free(elem); 
}

static void * copy_known_node (elem_known_nodes_t * elem) { return elem; }

// Equality is based strictly on the combination of IP and Port.
static int comp_known_node (elem_known_nodes_t * a, elem_known_nodes_t * b) { 
    return strcmp(a->node_ip, b->node_ip) != 0 || a->node_port != b->node_port; 
}

static unsigned hash_known_node (elem_known_nodes_t * elem) {
    unsigned hash = 5381;
    char * ip = elem->node_ip;
    while (*ip) {
        hash = ((hash << 5) + hash) + (unsigned char)*ip++;
    }
    hash = ((hash << 5) + hash) + elem->node_port;
    return hash;
}

// Internal constructor for creating a new known node entry
static elem_known_nodes_t * create_elem_known_node(const char * ip, unsigned port, unsigned cpu, unsigned ram, unsigned gpu){
    elem_known_nodes_t * out = malloc(sizeof(elem_known_nodes_t));
    if(out == NULL) return NULL;

    out->node_ip = strduplicate(ip);
    out->node_port = port;
    out->avail[CPU] = cpu;
    out->avail[RAM] = ram;
    out->avail[GPU] = gpu;
    
    // Automatically tags the entry with the exact time of creation/update
    out->time_stamp = time(NULL);
    
    return out;
}


/* ========================================================================= */
/* INITIALIZATION & DESTRUCTION                                              */
/* ========================================================================= */

/**
 * [INTEGRATION] 
 * Where to call: Should be called ONLY ONCE during node initialization (e.g., inside 'node_init').
 */
known_nodes_t create_known_nodes() {
    return tablahash_crear(11, (FuncionCopiadora)copy_known_node, (FuncionComparadora)comp_known_node, (FuncionDestructora)dest_known_node, (FuncionHash)hash_known_node);
}

/**
 * [INTEGRATION] 
 * Where to call: Should be called during graceful shutdown (e.g., inside 'node_dest')
 * to safely free the directory memory.
 */
void delete_known_nodes(known_nodes_t nodes) { 
    tablahash_destruir(nodes); 
}


/* ========================================================================= */
/* DIRECTORY UPDATES                                                         */
/* ========================================================================= */

/**
 * [INTEGRATION] 
 * Where to call: Inside the Patcher (resource_adapter.c) when handling an ACTION_NEW_NODE_DISCOVERED
 * or when parsing a CMD_ANNOUNCE.
 */
void update_known_node(known_nodes_t nodes, const char * ip, unsigned port, unsigned cpu, unsigned ram, unsigned gpu) { 
    // tablahash_insertar automatically replaces the old element if it already exists
    // (thanks to our comp_known_node function matching the IP and Port).
    // This perfectly updates the time_stamp to the current time.
    tablahash_insertar(nodes, create_elem_known_node(ip, port, cpu, ram, gpu)); 
}


/* ========================================================================= */
/* DATA EXTRACTION (GET_NODES PAYLOAD BUILDER)                               */
/* ========================================================================= */

/**
 * @brief Temporary context passed through a void pointer during hash table traversal.
 * Used exclusively by 'get_known_nodes_payload' to safely build the text payload without global variables.
 */
typedef struct {
    char * buffer;                  /**< The target string buffer */
    unsigned max_size;              /**< The maximum capacity of the buffer */
    unsigned current_offset;        /**< The current write head position in the buffer */
} payload_ctx_t;

// Visitor function executed for each active node in the hash table
static void append_node_to_payload(void * dato, void * extra) {
    elem_known_nodes_t * node = (elem_known_nodes_t *)dato;
    payload_ctx_t * ctx = (payload_ctx_t *)extra;

    // Abort if the buffer is full to prevent overflow
    if (ctx->current_offset >= ctx->max_size) return;

    // Write the node's data strictly following the required protocol format
    int written = snprintf(
        ctx->buffer + ctx->current_offset,
        ctx->max_size - ctx->current_offset,
        "%s:%u:cpu:%u:mem:%u:gpu:%u;",
        node->node_ip, node->node_port,
        node->avail[CPU], node->avail[RAM], node->avail[GPU]
    );

    // Advance the write head if successful
    if (written > 0 && written < (int)(ctx->max_size - ctx->current_offset)) {
        ctx->current_offset += written;
    }
}

/**
 * [INTEGRATION] 
 * Where to call: Inside the Patcher (resource_adapter.c) when responding to a CMD_GET_NODES from Erlang.
 */
void get_known_nodes_payload(known_nodes_t nodes, char * buffer, unsigned max_size) {
    payload_ctx_t ctx;
    ctx.buffer = buffer;
    ctx.max_size = max_size;
    
    // Start the payload with the standard protocol header
    ctx.current_offset = snprintf(ctx.buffer, ctx.max_size, "NODES ");

    // Append all nodes dynamically using the visitor pattern
    tablahash_visitar_extra(nodes, append_node_to_payload, &ctx);

    // Finalize the string: Replace the trailing ';' of the last node with '\n'
    if (ctx.current_offset > 6 && ctx.current_offset < max_size) {
        ctx.buffer[ctx.current_offset - 1] = '\n';
        ctx.buffer[ctx.current_offset] = '\0';
    }
}


/* ========================================================================= */
/* GARBAGE COLLECTION (DEAD NODE DETECTOR)                                   */
/* ========================================================================= */

/**
 * @brief Temporary context passed through a void pointer during hash table traversal.
 * Used exclusively by 'remove_inactive_nodes' to safely collect dead nodes.
 */
typedef struct {
    char ** dead_ips;                   /**< Array to collect the IPs of dead nodes */
    unsigned * dead_ports;              /**< Array to collect the ports of dead nodes */
    elem_known_nodes_t ** to_delete;    /**< Array of pointers for internal deletion */
    unsigned count;                     /**< Number of dead nodes found */
    unsigned max;                       /**< Maximum capacity of the arrays */
    time_t now;                         /**< Current system time captured once to avoid drift */
} gc_ctx_t;

// Visitor function executed for each node to evaluate its heartbeat
static void check_inactive_node(void * dato, void * extra) {
    elem_known_nodes_t * node = (elem_known_nodes_t *)dato;
    gc_ctx_t * ctx = (gc_ctx_t *)extra;

    if (ctx->count >= ctx->max) return;

    // If the node has been silent for longer than the ANNOUNCE_TIMEOUT
    if (difftime(ctx->now, node->time_stamp) > ANNOUNCE_TIMEOUT) {
        // We MUST duplicate the IP here, because tablahash_eliminar will free 'node->node_ip'
        ctx->dead_ips[ctx->count] = strduplicate(node->node_ip);
        ctx->dead_ports[ctx->count] = node->node_port;
        
        // Mark for deletion
        ctx->to_delete[ctx->count] = node;
        ctx->count++;
    }
}

/**
 * [INTEGRATION] 
 * Where to call: Inside the Patcher (resource_adapter.c) periodically during an 
 * ACTION_CHECK_DEADNODES event. The returned arrays of dead_ips and dead_ports 
 * MUST be passed directly to 'get_jobs_affected_by_dead_node' to clean up orphaned jobs.
 */
unsigned remove_inactive_nodes(known_nodes_t nodes, char * dead_ips[], unsigned dead_ports[], unsigned max_size) {
    unsigned current_elems = tablahash_nelems(nodes);
    if (current_elems == 0) return 0;

    gc_ctx_t ctx = {dead_ips, dead_ports, NULL, 0, max_size, time(NULL)};
    ctx.to_delete = malloc(sizeof(elem_known_nodes_t *) * current_elems);
    
    if (ctx.to_delete == NULL) return 0;

    // Traverse the directory looking for expired timestamps
    tablahash_visitar_extra(nodes, check_inactive_node, &ctx);

    // Remove the dead nodes from the hash table
    for (unsigned i = 0; i < ctx.count; i++) {
        tablahash_eliminar(nodes, ctx.to_delete[i]);
    }

    free(ctx.to_delete);
    return ctx.count; // Number of nodes that were permanently deleted
}