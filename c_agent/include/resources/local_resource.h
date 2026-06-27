#ifndef LOCAL_RESOURCES_H
#define LOCAL_RESOURCES_H

#include "tablahash.h"
#include "queues.h"
#include "common_types.h"

/**
 * @brief Hash table representing the jobs currently holding local resources.
 */
typedef TablaHash active_jobs_t;        

/**
 * @brief Central structure managing the physical inventory of the node.
 * * Tracks the absolute total capacity, the currently available capacity, 
 * and the FIFO waiting queues for each resource type.
 */
typedef struct{
    unsigned    total[RES_NUM];         /**< Maximum physical capacity per resource type */
    unsigned    avail[RES_NUM];         /**< Currently available capacity per resource type */
    queue_t     request_queue[RES_NUM]; /**< FIFO queues for pending requests (elem_job_request_t) */
} local_resources_t;


/* ========================================================================= */
/* CREATION AND DESTRUCTION                                                  */
/* ========================================================================= */

/**
 * @brief Initializes the local resource manager with physical hardware limits.
 * @param cpu Total CPU cores available.
 * @param gpu Total GPUs available.
 * @param ram Total RAM available (e.g., in MB).
 * @return Pointer to the allocated local_resources_t structure.
 */
local_resources_t * create_local_resource(unsigned cpu, unsigned gpu, unsigned ram);

/**
 * @brief Frees the memory allocated for the local resource manager and its queues.
 * @param resources Pointer to the local_resources_t structure to destroy.
 */
void delete_local_resource(local_resources_t * resources);

/**
 * @brief Creates the hash table used to track resources currently granted to remote nodes.
 * @return A new empty hash table.
 */
active_jobs_t create_active_jobs_table();


/* ========================================================================= */
/* RESERVATION AND QUEUE MANAGEMENT                                          */
/* ========================================================================= */

/**
 * @brief Attempts to allocate resources for a new incoming job request.
 * * If resources are available, they are immediately granted and tracked.
 * If not enough resources are available, the request is pushed to the waiting queue.
 * If the requested quantity exceeds the absolute total capacity, it is denied.
 * * @param resources Pointer to the local resource manager.
 * @param active_jobs Hash table tracking currently granted resources.
 * @param id The unique identifier of the job.
 * @param socket The file descriptor (socket) of the remote node requesting the resource.
 * @param quantity Amount of the resource requested.
 * @param type The type of resource requested (CPU, GPU, or RAM).
 * @return -1 if DENIED (exceeds total capacity), 0 if WAIT (queued), or 1 if GRANTED.
 */
int new_job_request(local_resources_t * resources, active_jobs_t active_jobs, unsigned id, unsigned socket, unsigned quantity, resource_t type);

/**
 * @brief Checks the waiting queues to see if any pending requests can now be fulfilled.
 * * This function should be called immediately after resources are freed. It iterates 
 * through the queues and grants resources to the first eligible request.
 * * @param resources Pointer to the local resource manager.
 * @param active_jobs Hash table tracking currently granted resources.
 * @param OUT Buffer to store the formatted network message (e.g., "GRANTED 100\n").
 * @param OUT_SIZE Maximum size of the OUT buffer.
 * @param out_socket Pointer to store the socket of the node that was just granted the resources.
 * @return The job_id that was granted resources, or -1 if no requests could be fulfilled.
 */
int chk_job_request(local_resources_t * resources, active_jobs_t active_jobs, char * OUT, unsigned OUT_SIZE, unsigned *out_socket);


/* ========================================================================= */
/* RESOURCE RELEASE AND CLEANUP                                              */
/* ========================================================================= */

/**
 * @brief Frees previously granted resources for a specific job and returns them to the available pool.
 * @param resources Pointer to the local resource manager.
 * @param jobs Hash table containing the active jobs.
 * @param job_id The identifier of the job releasing the resources.
 * @param socket The file descriptor (socket) of the remote node releasing the resources.
 */
void del_active_job(local_resources_t * resources, active_jobs_t jobs, unsigned job_id, unsigned socket);

/**
 * @brief Removes a specific pending request from the waiting queues without granting it.
 * @param resources Pointer to the local resource manager.
 * @param job_id The identifier of the job to cancel.
 * @param socket The file descriptor (socket) of the remote node cancelling the request.
 */
void del_pending_job_requests(local_resources_t * resources, unsigned job_id, unsigned socket);

/**
 * @brief Emergency cleanup: Reclaims all granted resources and deletes all queued requests 
 * originating from a specific socket. Used when a remote node abruptly disconnects.
 * @param resources Pointer to the local resource manager.
 * @param jobs Hash table containing the active jobs.
 * @param socket The file descriptor of the disconnected node.
 */
void free_all_resources_from_socket(local_resources_t * resources, active_jobs_t jobs, unsigned socket);


/* ========================================================================= */
/* QUERIES                                                                   */
/* ========================================================================= */

/**
 * @brief Retrieves the current available quantities of all local resources.
 * @param resources Pointer to the local resource manager.
 * @param cpu_quantity Pointer to store the available CPU count.
 * @param gpu_quantity Pointer to store the available GPU count.
 * @param mem_quantity Pointer to store the available RAM count.
 */
void get_local_resources(local_resources_t * resources, unsigned * cpu_quantity, unsigned * gpu_quantity, unsigned * mem_quantity);

#endif