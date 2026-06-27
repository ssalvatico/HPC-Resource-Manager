#ifndef RESOURCE_ADAPTER_H
#define RESOURCE_ADAPTER_H

#include "server_types.h"
#include "event_handler.h"

#define MAX_TRACKED_JOBS 100
#define MAX_NODES_PER_JOB 20


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
 * * @param SENDER_IP The IPv4 address of the node that triggered the event.
 * * @param SOCKET The file descriptor of the connection (or abstract ID).
 * * @param BUFFER The raw, null-terminated string payload received from the network.
 * * @param outbox Array where outgoing protocol messages will be crafted.
 * * @param outbox_count Pointer to store the number of messages generated in the outbox.
 * * @param action The specific network event trigger (e.g., ACTION_RESPOND, ACTION_DISCONNECTED).
 * @return Void.
 */
void resource_adapter_patch(ServerContext* ctx, char * SENDER_IP, unsigned SOCKET, const char * BUFFER, out_msg_t * outbox, int * outbox_count, JuaniAction action);


#endif // RESOURCE_ADAPTER_H