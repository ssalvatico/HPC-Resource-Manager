#include "../../include/resources/node_structures.h" // Ajusta la ruta a la ubicación real
#include <stdlib.h>
#include <stdio.h>

/* ========================================================================= */
/* INITIALIZATION                                                            */
/* ========================================================================= */

node_data_t init_node(unsigned cpu, unsigned gpu, unsigned ram) {
    
    // 1. Allocate memory for the main container
    node_data_t node = malloc(sizeof(struct node_data_t_));
    if (node == NULL) {
        perror("[ERROR] Failed to allocate node_data_t container");
        return NULL;
    }

    // Initialaze mutex for server/client
    pthread_mutex_init(&node->lock_local, NULL);
    pthread_mutex_init(&node->lock_known, NULL);
    pthread_mutex_init(&node->lock_owned, NULL);

    // 2. Initialize the physical local resource manager
    node->resources = create_local_resource(cpu, gpu, ram);
    if (node->resources == NULL) {
        perror("[ERROR] Failed to initialize local_resources_t");
        free(node);
        return NULL;
    }

    // 3. Initialize the three core hash tables
    node->active_jobs = create_active_jobs_table();
    node->known_nodes = create_known_nodes();
    node->owned_jobs  = create_owned_jobs();

    // 4. Safety Check: Ensure no memory allocation failures occurred during hash table creation
    if (node->active_jobs == NULL || node->known_nodes == NULL || node->owned_jobs == NULL) {
        perror("[ERROR] Failed to initialize one or more core Hash Tables. Rolling back...");
        
        // We leverage our own destructor to clean up whichever modules WERE successfully created
        dest_node(node); 
        return NULL;
    }

    printf("[INITIALIZATION] Node structures successfully created.\n");
    printf("                 -> Hardware limits set to: CPU: %u | GPU: %u | RAM: %u\n", cpu, gpu, ram);

    
    return node;
}


/* ========================================================================= */
/* DESTRUCTION (GRACEFUL SHUTDOWN)                                           */
/* ========================================================================= */

void dest_node(node_data_t node) {
    // Prevent segfaults if a NULL pointer is passed
    if (node == NULL) return;

    // 1. Destroy each sub-module using its official destructor
    // The if-checks guarantee that we don't attempt to free modules that failed to initialize
    
    if (node->resources) {
        delete_local_resource(node->resources);
    }
    
    if (node->active_jobs) {
        tablahash_destruir(node->active_jobs);
    }
    
    if (node->known_nodes) {
        delete_known_nodes(node->known_nodes);
    }
    
    if (node->owned_jobs) {
        delete_owned_jobs(node->owned_jobs);
    }

    // 2. Destroy server/client locks
    pthread_mutex_destroy(&node->lock_local);
    pthread_mutex_destroy(&node->lock_known);
    pthread_mutex_destroy(&node->lock_owned);

    // 3. Finally, free the main container shell
    free(node);
    
    printf("[SHUTDOWN] Node structures and all sub-modules successfully destroyed. No leaks remaining.\n");
}