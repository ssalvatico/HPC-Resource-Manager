#ifndef __RESOURCE_MANAGING_H__
#define __RESOURCE_MANAGING_H__

typedef enum { CPU = 0, GPU = 1, RAM = 2, RES_NUM = 3 } resource_t;

typedef struct node_data_t_ * node_data_t;

/**
 * @brief Allocates and initializes a new node data structure.
 *
 * Creates the internal hash tables for active jobs, known nodes, and
 * owned jobs. Also initializes the local resource pool with the given
 * CPU, GPU, and RAM capacities and their corresponding FIFO request queues.
 *
 * @param cpu Total CPU units available on this node.
 * @param gpu Total GPU units available on this node.
 * @param ram Total RAM units (MB) available on this node.
 * @return A pointer to the newly allocated node_data_t structure.
 */
node_data_t node_init(unsigned cpu, unsigned gpu, unsigned ram);

void        node_dest   (node_data_t node);

unsigned    master_function(node_data_t NODE, char * NODE_IP, unsigned SOCKET, const char * BUFFER, char * OUT, unsigned OUT_SIZE);

void        known_nodes_del_inactive_nodes(node_data_t node);

/**
 * @brief Checks if any queued resource request can now be fulfilled.
 *
 * Scans the FIFO request queues for each resource type. If a pending
 * request can be satisfied with currently available resources, it grants
 * it, updates the active jobs table, and writes a GRANTED message to OUT.
 *
 * @param NODE       Pointer to the local node data structure.
 * @param OUT        Buffer where the GRANTED message will be written.
 * @param OUT_SIZE   Maximum size of the OUT buffer.
 * @param out_socket Output parameter set to the socket of the requester
 *                   whose request was fulfilled, so the caller can route
 *                   the response to the correct file descriptor.
 * @return The job_id that was granted, or -1 if no request could be fulfilled.
 */
int chk_job_request(node_data_t NODE, char * OUT, unsigned OUT_SIZE, unsigned *out_socket);

/**
 * @brief Retrieves the list of resources associated with a given job.
 *
 * Looks up an owned job by its ID and populates the provided arrays with
 * the IP, resource type, and quantity for each resource petition. Used
 * by the adapter to build RESERVE and RELEASE messages for remote nodes.
 *
 * @param NODE      Pointer to the local node data structure.
 * @param job_id    The unique identifier of the job to look up.
 * @param arr_ip    Output array of IP address strings, one per resource.
 * @param arr_type  Output array of resource types (CPU, GPU, RAM).
 * @param quantity  Output array of requested quantities.
 * @param size      Maximum number of entries the output arrays can hold.
 * @return The number of resource petitions found for the given job.
 */
unsigned get_job_data(node_data_t NODE, unsigned job_id, char * arr_ip[], resource_t arr_type[], unsigned quantity[], const unsigned size);

/**
 * @brief Reads the currently available local resource quantities.
 *
 * Copies the available (not total) units of each resource type into
 * the provided output pointers. Any pointer may be NULL to skip that
 * resource. Used to build ANNOUNCE broadcast messages.
 *
 * @param NODE          Pointer to the local node data structure.
 * @param cpu_quantity  Output pointer for available CPU units.
 * @param gpu_quantity  Output pointer for available GPU units.
 * @param mem_quantity  Output pointer for available RAM units.
 * @return Void.
 */
void get_local_resources(node_data_t NODE, unsigned * cpu_quantity, unsigned * gpu_quantity, unsigned * mem_quantity);

unsigned    get_node_port(node_data_t NODE, const char * ip);

void        release_jobs_by_socket(node_data_t NODE, unsigned socket);

#endif