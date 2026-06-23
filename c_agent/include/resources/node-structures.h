#ifndef __RESOURCE_MANAGING_H__
#define __RESOURCE_MANAGING_H__

typedef enum { CPU = 0, GPU = 1, RAM = 2, RES_NUM = 3 } resource_t;

typedef struct node_data_t_ * node_data_t;

node_data_t node_init   (unsigned cpu, unsigned gpu, unsigned ram);

void        node_dest   (node_data_t node);

unsigned    master_function(node_data_t NODE, char * NODE_IP, unsigned NODE_PORT, unsigned SOCKET, const char * BUFFER, char * OUT, unsigned OUT_SIZE);

void        known_nodes_del_inactive_nodes(node_data_t node);

int         chk_job_request (node_data_t NODE, char * OUT, unsigned OUT_SIZE, unsigned *out_socket);

unsigned    get_job_data(node_data_t NODE, unsigned job_id, char * arr_ip[], unsigned arr_port[], resource_t arr_type[], unsigned quantity[], const unsigned size);

unsigned    get_granted_job_data(node_data_t NODE, unsigned job_id, char * arr_ip[], unsigned arr_port[], resource_t arr_type[], unsigned quantity[], const unsigned size);

unsigned    get_next_job_data(node_data_t NODE, unsigned job_id, char ** ip, unsigned * port, resource_t * type, unsigned * quantity);

unsigned    get_job_owner_socket(node_data_t NODE, unsigned job_id);

unsigned    collect_timed_out_jobs(node_data_t NODE, unsigned job_ids[], unsigned owner_sockets[], const unsigned size);

void        remove_owned_job(node_data_t NODE, unsigned job_id);

void        get_local_resources(node_data_t NODE, unsigned * cpu_quantity, unsigned * gpu_quantity, unsigned * mem_quantity);

#endif
