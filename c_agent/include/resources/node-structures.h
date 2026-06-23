#ifndef __RESOURCE_MANAGING_H__
#define __RESOURCE_MANAGING_H__

typedef enum { CPU = 0, GPU = 1, RAM = 2, RES_NUM = 3 } resource_t;

typedef struct node_data_t_ * node_data_t;

node_data_t node_init   (unsigned cpu, unsigned gpu, unsigned ram);

void        node_dest   (node_data_t node);

void        known_nodes_del_inactive_nodes(node_data_t node, char ** ARR_ID, unsigned * ARR_PORT, unsigned SIZE); //acá

int         chk_job_request (node_data_t NODE, char * OUT, unsigned OUT_SIZE, unsigned *out_socket);

unsigned    get_job_data(node_data_t NODE, unsigned job_id, char * arr_ip[], resource_t arr_type[], unsigned quantity[], const unsigned size);

void        get_local_resources(node_data_t NODE, unsigned * cpu_quantity, unsigned * gpu_quantity, unsigned * mem_quantity);

// given a node. saves announced new node data in known nodes
void command_announce       (node_data_t NODE, char * ip, unsigned port, unsigned ram, unsigned cpu, unsigned gpu);

// given a node, returns a parsed buffer with known nodes data
void command_get_nodes      (node_data_t NODE, char * OUT_BUFFER, unsigned OUT_SIZE);

// given a node and a job request, it saves in node structure all job's petitions
void command_job_request    (node_data_t NODE, unsigned job_id, char ** ARR_IP, unsigned * ARR_PORTS, resource_t * ARR_TYPE, unsigned * ARR_QUANT, unsigned size);

//
void command_job_release    (node_data_t NODE, unsigned job_id);

//
void command_release(node_data_t NODE, char * NODE_IP, unsigned job_id, resource_t resource, unsigned quantity);

//
void command_denied(node_data_t NODE, unsigned job_id);

//
void command_granted(node_data_t NODE, unsigned job_id, char * NODE_IP, unsigned NODE_PORT);

//
void command_reserve(node_data_t NODE, char * NODE_IP, unsigned SOCKET, unsigned job_id, resource_t type, unsigned amount);

#endif