#ifndef RESOURCE_ADAPTER_H
#define RESOURCE_ADAPTER_H

#include "server_types.h"
#include "event_handler.h"
#include "../resources/node-structures.h"

#define MAX_TRACKED_JOBS 100
#define MAX_NODES_PER_JOB 20

/**
 * @brief Structure to hold parsed data from a single resource petition.
 * * Used primarily to extract and store routing information (IP and port)
 * alongside the requested resources from a complex JOB_REQUEST string.
 */
typedef struct {
    char ip[16];
    unsigned port;
    resource_t type;
    unsigned amount;
} parsed_petition_t;

/**
 * @brief Structure to hold a full job record.
 *
 * This structure stores the complete state of a single job, including its
 * unique identifier, active status, and the list of resource petitions
 * that were parsed from a JOB_REQUEST message. It is intended to be used
 * as the primary data container for tracking jobs within the system.
 */
typedef struct {
    unsigned job_id;
    int is_active;
    unsigned petition_count;
    parsed_petition_t petitions[MAX_NODES_PER_JOB];
} job_record_t;

/**
 * @brief Main adapter for routing network events to local resource logic.
 * * Reference: C-Agent Network Architecture, Two-Phase Locking Pattern.
 * This function acts as the bridge between the asynchronous network layer (epoll)
 * and the synchronous business logic (Juani's node structures). It parses incoming
 * buffers, executes the corresponding state updates, handles node disconnections
 * by resolving affected jobs, and populates the outbox with response messages.
 * It strictly uses a Two-Phase Locking approach to avoid deadlocks between the
 * network routing table and the local resource mutex.
 * * @param ctx Pointer to the global ServerContext structure.
 * * @param NODE The local node data structure containing resource states.
 * * @param SENDER_IP The IPv4 address of the node that triggered the event.
 * * @param SOCKET The file descriptor of the connection (or abstract ID).
 * * @param BUFFER The raw, null-terminated string payload received from the network.
 * * @param outbox Array where outgoing protocol messages will be crafted.
 * * @param outbox_count Pointer to store the number of messages generated in the outbox.
 * * @param action The specific network event trigger (e.g., ACTION_RESPOND, ACTION_DISCONNECTED).
 * @return Void.
 */
void resource_adapter_patch(ServerContext* ctx, node_data_t NODE, char * SENDER_IP, unsigned SOCKET, const char * BUFFER, out_msg_t * outbox, int * outbox_count, JuaniAction action);

/**
 * @brief Parses a RESERVE or RELEASE protocol command.
 * * Reference: C-Agent Protocol Definitions.
 * Extracts the job ID, resource type, and quantity from a string formatted
 * as "COMMAND <job_id> <resource> <quantity>". Maps the resource string
 * ("cpu", "gpu", "mem") to the internal resource_t enum.
 * * @param buffer The raw string payload containing the command.
 * * @param job_id Pointer to store the extracted job identifier.
 * * @param type Pointer to store the extracted resource type enum.
 * * @param quantity Pointer to store the extracted resource amount.
 * @return 1 if parsing is completely successful and valid, 0 otherwise.
 */
int parse_reserve_release(const char* buffer, unsigned* job_id, resource_t* type, unsigned* quantity);

/**
 * @brief Parses an ANNOUNCE UDP datagram.
 * * Reference: C-Agent UDP Discovery Protocol.
 * Extracts the listening port and available local resources of a newly
 * discovered peer from a string formatted as
 * "ANNOUNCE <port> cpu:<x> mem:<y> gpu:<z>".
 * * @param buffer The raw UDP datagram payload.
 * * @param port Pointer to store the node's TCP listening port.
 * * @param cpu Pointer to store the available CPU quantity.
 * * @param ram Pointer to store the available RAM quantity.
 * * @param gpu Pointer to store the available GPU quantity.
 * @return 1 if all 4 components are successfully extracted, 0 otherwise.
 */
int parse_announce(const char* buffer, unsigned* port, unsigned* cpu, unsigned* ram, unsigned* gpu);

/**
 * @brief Parses a complex JOB_REQUEST containing multiple node petitions.
 * * Reference: Erlang-C Agent Communication Protocol.
 * Iterates over a command string to extract the master job ID and an arbitrary
 * number of individual resource petitions separated by the '@' character.
 * The expected format is "JOB_REQUEST <job_id> @ip:port:res:amount ...".
 * * @param buffer The raw string payload from Erlang.
 * * @param job_id Pointer to store the master job identifier.
 * * @param petitions Array of structures to store the extracted petitions.
 * * @param petition_count Pointer to output the total number of successfully parsed petitions.
 * * @param max_petitions The maximum capacity of the petitions array to prevent buffer overflows.
 * @return 1 if the command is valid and at least one petition is extracted, 0 otherwise.
 */
int parse_job_request(const char* buffer, unsigned* job_id, parsed_petition_t* petitions, unsigned* petition_count, unsigned max_petitions);

/**
 * @brief Parses single-argument commands consisting only of an ID.
 * * Reference: C-Agent Protocol Definitions.
 * Extracts the job ID for lightweight state-change commands like
 * GRANTED, DENIED, or JOB_RELEASE (e.g., "GRANTED 1234").
 * * @param buffer The raw string payload containing the command.
 * * @param job_id Pointer to store the extracted job identifier.
 * @return 1 if parsing is successful, 0 otherwise.
 */
int parse_single_id_cmd(const char* buffer, unsigned* job_id);

/**
 * @brief Registers a newly parsed job into the local network routing table.
 * * Reference: pthread_rwlock_wrlock.
 * Safely inserts an array of parsed petitions into the active_jobs_registry
 * using a write-lock. This table is later used to perform atomic loopbacks
 * and mass-release broadcasts when a participating node disconnects or dies.
 * * @param job_id The unique identifier of the job being registered.
 * * @param petitions The array of parsed petitions detailing participating nodes.
 * * @param count The number of valid petitions in the array.
 * @return Void.
 */
void register_job(unsigned job_id, parsed_petition_t* petitions, unsigned count);

#endif // RESOURCE_ADAPTER_H