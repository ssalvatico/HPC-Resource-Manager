#ifndef __RESOURCE_MANAGING_H__
#define __RESOURCE_MANAGING_H__

typedef enum { CPU = 0, GPU = 1, RAM = 2, RES_NUM = 3 } resource_t;

typedef struct node_data_t_ * node_data_t;

// Initializes a node with the given local resources.
// Returns a new node_data_t on success, NULL on failure.
node_data_t node_init   (unsigned cpu, unsigned gpu, unsigned ram);

// Frees all memory associated with the node.
void        node_dest   (node_data_t node);

// Removes inactive nodes (no ANNOUNCE received in TIMEOUT seconds) from known_nodes.
// Writes the IPs and ports of removed nodes into ARR_ID and ARR_PORT (up to SIZE entries).
// Returns the number of removed nodes.
unsigned    known_nodes_del_inactive_nodes(node_data_t node, char ** ARR_ID, unsigned * ARR_PORT, unsigned SIZE); //acá

// Checks if any enqueued job request can now be satisfied with available resources.
// If so, dequeues it, grants it, writes "GRANTED <job_id>\n" into OUT, and sets *out_socket.
// Returns the granted job_id, or -1 if nothing was dequeued.
int         chk_job_request (node_data_t NODE, char * OUT, unsigned OUT_SIZE, unsigned *out_socket);

// Retrieves petition data for a given job_id from owned_jobs.
// Writes up to size entries into arr_ip, arr_type and quantity.
// Returns the number of entries written.
unsigned    get_job_data(node_data_t NODE, unsigned job_id, char * arr_ip[], resource_t arr_type[], unsigned quantity[], const unsigned size);

// Writes the current available local resources into the provided pointers.
// Any pointer may be NULL, in which case that resource is skipped.
void        get_local_resources(node_data_t NODE, unsigned * cpu_quantity, unsigned * gpu_quantity, unsigned * mem_quantity);

// Registers a node announcement in known_nodes with the given ip, port and resources.
void command_announce       (node_data_t NODE, char * ip, unsigned port, unsigned ram, unsigned cpu, unsigned gpu);

// Writes a "NODES ..." string into OUT_BUFFER with all known nodes and their resources.
void command_get_nodes      (node_data_t NODE, char * OUT_BUFFER, unsigned OUT_SIZE);

// Registers a new owned job with its resource petitions.
// ARR_IP, ARR_PORTS, ARR_TYPE and ARR_QUANT are parallel arrays of length size,
// each entry describing one petition: target node (ip, port), resource type and amount.
void command_job_request    (node_data_t NODE, unsigned job_id, char ** ARR_IP, unsigned * ARR_PORTS, resource_t * ARR_TYPE, unsigned * ARR_QUANT, unsigned size);

// Removes a job from owned_jobs. Used both for JOB_RELEASE and JOB_DENIED.
void command_job_release_and_denied    (node_data_t NODE, unsigned job_id);

// Releases local resources held by a job. Used when RELEASE is received from a remote node.
void command_release(node_data_t NODE, char * NODE_IP, unsigned job_id, resource_t resource, unsigned quantity);

// Removes a job from owned_jobs upon receiving DENIED from a remote node.
void command_denied(node_data_t NODE, unsigned job_id);

// Marks the petition toward (NODE_IP, NODE_PORT) as granted for the given job.
// Returns 1 if all petitions for that job are now granted (job fully allocated), 0 otherwise.
unsigned command_granted(node_data_t NODE, unsigned job_id, char * NODE_IP, unsigned NODE_PORT);

// Attempts to reserve local resources for a job from NODE_IP via SOCKET.
// If resources are available, grants immediately and returns 1.
// If not, enqueues the request and returns 0.
unsigned command_reserve(node_data_t NODE, char * NODE_IP, unsigned SOCKET, unsigned job_id, resource_t type, unsigned amount);

#endif