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

/**
 * @brief Frees all memory associated with a node data structure.
 *
 * Destroys all internal hash tables and the local resource pool,
 * including all pending FIFO request queues for each resource type.
 *
 * @param node Pointer to the node_data_t structure to destroy.
 * @return Void.
 */
void node_dest(node_data_t node);

/**
 * @brief Processes an incoming protocol message and updates internal state.
 *
 * Central dispatch function. Parses the command in BUFFER and executes
 * the corresponding logic: registering nodes (ANNOUNCE), managing resource
 * reservations and releases (RESERVE, RELEASE), handling job lifecycle
 * (JOB_REQUEST, JOB_RELEASE), and processing inter-agent responses
 * (GRANTED, DENIED). Writes a response string to OUT when applicable.
 *
 * @param NODE   Pointer to the local node data structure.
 * @param NODE_IP IP address of the sender.
 * @param SOCKET File descriptor of the connection (used as job key).
 * @param BUFFER Null-terminated string containing the incoming protocol message.
 * @param OUT    Buffer where the response message will be written, if any.
 * @param OUT_SIZE Maximum size of the OUT buffer.
 * @return SOLVED (2) on success, WAIT (1) if queued, ERROR (0) on failure.
 */
unsigned master_function(node_data_t NODE, char * NODE_IP, unsigned SOCKET, const char * BUFFER, char * OUT, unsigned OUT_SIZE);

/**
 * @brief Removes nodes that have not sent an ANNOUNCE within the timeout period.
 *
 * Iterates over the known nodes table and eliminates any entry whose
 * last_seen timestamp exceeds TIMEOUT seconds. Called periodically by
 * the garbage collector timer.
 *
 * @param node Pointer to the local node data structure.
 * @return Void.
 */
void known_nodes_del_inactive_nodes(node_data_t node);

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

/**
 * @brief Looks up the TCP port of a known node by its IP address.
 *
 * Searches the known nodes hash table for an entry matching the given IP
 * and returns its registered port. Used by the adapter to route outgoing
 * RESERVE and RELEASE messages to the correct remote agent.
 *
 * @param NODE Pointer to the local node data structure.
 * @param ip   The IP address string to look up.
 * @return The port number of the matching node, or 0 if not found.
 */
unsigned get_node_port(node_data_t NODE, const char * ip);

/**
 * @brief Removes all pending resource requests associated with a given socket.
 *
 * Iterates over the FIFO request queues for all resource types and removes
 * any entry whose socket field matches the given value. Called when a
 * connection drops unexpectedly to prevent orphaned requests from blocking
 * resources indefinitely.
 *
 * @param NODE   Pointer to the local node data structure.
 * @param socket The file descriptor of the disconnected connection.
 * @return Void.
 */
void release_jobs_by_socket(node_data_t NODE, unsigned socket);

#endif