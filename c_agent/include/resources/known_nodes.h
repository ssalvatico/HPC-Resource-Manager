#ifndef KNOWN_NODES_H
#define KNOWN_NODES_H

#include "tablahash.h"
#include "common_types.h"

#define ANNOUNCE_TIMEOUT 15

/**
 * @brief Hash table representing the active nodes in the cluster (Yellow Pages).
 */
typedef TablaHash known_nodes_t;

/* ========================================================================= */
/* CREATION AND DESTRUCTION                                                  */
/* ========================================================================= */

/**
 * @brief Initializes the directory of known nodes.
 * @return A new empty hash table.
 */
known_nodes_t create_known_nodes();

/**
 * @brief Frees the memory allocated for the known nodes directory.
 * @param nodes Pointer to the hash table to destroy.
 */
void delete_known_nodes(known_nodes_t nodes);

/* ========================================================================= */
/* DIRECTORY UPDATES                                                         */
/* ========================================================================= */

/**
 * @brief Inserts a new node or updates the timestamp and resources of an existing one.
 * * [INTEGRATION]
 * Where to call: Inside the Patcher when parsing a CMD_ANNOUNCE.
 * * @param nodes Hash table managing the known nodes.
 * @param ip The IP address from the announcement.
 * @param port The TCP port from the announcement.
 * @param cpu Available CPUs announced.
 * @param ram Available RAM announced.
 * @param gpu Available GPUs announced.
 */
void update_known_node(known_nodes_t nodes, const char * ip, unsigned port, unsigned cpu, unsigned ram, unsigned gpu);

/* ========================================================================= */
/* DATA EXTRACTION                                                           */
/* ========================================================================= */

/**
 * @brief Builds the payload string containing all active nodes to send to Erlang.
 * * [INTEGRATION]
 * Where to call: Inside the Patcher when responding to a CMD_GET_NODES.
 * * @param nodes Hash table managing the known nodes.
 * @param buffer The string buffer where the payload will be written.
 * @param max_size The maximum capacity of the buffer.
 */
void get_known_nodes_payload(known_nodes_t nodes, char * buffer, unsigned max_size);

/* ========================================================================= */
/* GARBAGE COLLECTION                                                        */
/* ========================================================================= */

/**
 * @brief Scans the directory and removes any node that hasn't announced itself 
 * within the ANNOUNCE_TIMEOUT window (15 seconds).
 * * [INTEGRATION]
 * Where to call: Inside the Patcher during an ACTION_CHECK_DEADNODES event.
 * * @param nodes Hash table managing the known nodes.
 * @param dead_ips Array to populate with the IPs of the removed nodes.
 * @param dead_ports Array to populate with the ports of the removed nodes.
 * @param max_size The maximum capacity of the provided arrays.
 * @return The number of dead nodes found and removed.
 */
unsigned remove_inactive_nodes(known_nodes_t nodes, char * dead_ips[], unsigned dead_ports[], unsigned max_size);

#endif