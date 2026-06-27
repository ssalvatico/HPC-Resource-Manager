#ifndef NODE_STRUCTURES_H
#define NODE_STRUCTURES_H

#include "local_resource.h"
#include "owned_jobs.h"
#include "known_nodes.h"
#include <pthread.h>
/**
 * @brief Global state container for the node.
 * Unifies the physical resource manager, the network directory (Yellow Pages), 
 * and the Erlang jobs tracker into a single cohesive structure.
 */
typedef struct node_data_t_{
    pthread_mutex_t     lock_local;     /**< Lock for local resources and active jobs management */
    active_jobs_t       active_jobs;    /**< Hash table tracking local resources currently granted to remote nodes */
    local_resources_t * resources;      /**< Physical inventory manager and waiting queues of this machine */

    pthread_mutex_t     lock_known;     /**< Lock for known nodes management */
    known_nodes_t       known_nodes;    /**< Hash table tracking remote nodes discovered via UDP (Yellow Pages) */
    
    pthread_mutex_t     lock_owned;     /**< Lock for owned jobs management */
    owned_jobs_t        owned_jobs;     /**< Hash table tracking jobs initiated by our local Erlang scheduler */
} node_data_t_;

/**
 * @brief Opaque pointer to the global state container.
 * Passed around to the Patcher (resource_adapter) and Event Handlers to provide 
 * unified access to all node functionalities.
 */
typedef struct node_data_t_ * node_data_t;


/* ========================================================================= */
/* INITIALIZATION AND DESTRUCTION                                            */
/* ========================================================================= */

/**
 * @brief Allocates and initializes the global node structure and all its sub-modules.
 * * [INTEGRATION]
 * Where to call: Inside your main.c or server initialization block, right before 
 * starting the epoll event loop.
 * * @param cpu Total physical CPUs available on this machine.
 * @param gpu Total physical GPUs available on this machine.
 * @param ram Total physical RAM available on this machine.
 * @return A pointer to the fully initialized node_data_t, or NULL on memory allocation failure.
 */
node_data_t init_node(unsigned cpu, unsigned gpu, unsigned ram);

/**
 * @brief Safely destroys the global node structure and prevents memory leaks 
 * by invoking the specific destructors of all its sub-modules.
 * * [INTEGRATION]
 * Where to call: Inside your graceful shutdown routine (e.g., when catching SIGINT/Ctrl+C 
 * in the main thread) right before exiting the program.
 * * @param node Pointer to the global state container to destroy.
 */
void dest_node(node_data_t node);

#endif