#ifndef __RESOURCE_MANAGING_H__
#define __RESOURCE_MANAGING_H__

typedef enum { CPU = 0, GPU = 1, RAM = 2, RES_NUM = 3 } resource_t;

typedef struct node_data_t_ * node_data_t;

node_data_t node_init   (unsigned cpu, unsigned gpu, unsigned ram);

void        node_dest   (node_data_t node);

unsigned    master_function(node_data_t NODE, char * NODE_IP, unsigned SOCKET, const char * BUFFER, char * OUT, unsigned OUT_SIZE);

void        known_nodes_del_inactive_nodes(node_data_t node);

int         chk_job_request (node_data_t NODE, char * OUT, unsigned OUT_SIZE);

unsigned    get_job_data(node_data_t NODE, unsigned job_id, char * arr_ip[], resource_t arr_type[], unsigned quantity[], const unsigned size);

void        get_local_resources(node_data_t NODE, unsigned * cpu_quantity, unsigned * gpu_quantity, unsigned * mem_quantity);

unsigned get_node_port(node_data_t NODE, const char * ip);

#endif