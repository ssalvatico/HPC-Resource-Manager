#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#define MAX_EVENTS 10
#include "server_types.h"
#include <stdint.h>
#include <pthread.h>

typedef struct {
    char ip[16];
    char message[BUFFER_SIZE];
} AsyncPending;

/**
 * @brief Handles the expiration of the TCP startup timer.
 * * Reference: man 2 read, man 7 epoll.
 * This function clears the timer expiration count, removes the timer FD from 
 * the epoll interest list, and safely closes it. It then activates the main 
 * TCP communication channels (public and local Erlang) by registering them 
 * with epoll to start accepting incoming connections.
 * * @param ctx Pointer to the global ServerContext structure.
 * @return Void.
 */
void handle_tcp_timer_expiration(ServerContext* ctx);

/**
 * @brief Handles the expiration of the UDP broadcast timer.
 * * Reference: man 2 read.
 * This function clears the timer expiration count, retrieves the local resources 
 * formatted as an ANNOUNCE string via the master function, and broadcasts 
 * this state to the entire local network to maintain discovery.
 * * @param ctx Pointer to the global ServerContext structure.
 * @return Void.
 */
void handle_udp_timer_expiration(ServerContext* ctx);

/**
 * @brief Processes incoming UDP discovery datagrams from other nodes.
 * * Reference: man 2 recvfrom.
 * This function reads incoming datagrams. It implements self-echo filtering 
 * by checking if the sender's IP matches the local LAN IP. If the message 
 * is from a new peer, it delegates the payload to the master function to 
 * register the newly discovered node.
 * * @param ctx Pointer to the global ServerContext structure.
 * @return Void.
 */
void handle_incoming_discovery(ServerContext* ctx);

/**
 * @brief Accepts and registers a new incoming TCP connection.
 * * Reference: man 2 accept, man 7 epoll.
 * This function accepts a pending connection on the listening socket. 
 * It determines if the connection is from the public network or the local 
 * Erlang scheduler for logging purposes. It then adds the new client FD 
 * to epoll using EPOLLIN and EPOLLONESHOT to ensure thread safety during reads.
 * * @param ctx Pointer to the global ServerContext structure.
 * * @param server_fd The listening file descriptor (public or erlang) that triggered the event.
 * @return Void.
 */
void handle_new_tcp_connection(ServerContext* ctx, int server_fd);

/**
 * @brief Reads and processes incoming data from an established TCP connection.
 * * Reference: man 2 recv.
 * This function retrieves the message and passes it to the master logic. 
 * If the client gracefully or unexpectedly disconnects (recv returns 0 or an error), 
 * it triggers the DISCONNECTED action, cleans up the FD, and removes it from epoll.
 * * @param ctx Pointer to the global ServerContext structure.
 * * @param curr_fd The file descriptor of the connected client.
 * @return 1 if the connection is kept alive, 0 if the client was disconnected.
 */
int handle_client_message(ServerContext* ctx, int curr_fd);

/**
 * @brief Finalizes an asynchronous non-blocking TCP connection attempt.
 * * Reference: man 2 getsockopt, man 3 pthread_rwlock_rdlock.
 * Triggered by an EPOLLOUT event after a connect() call. It verifies the 
 * connection status via SO_ERROR. If successful, it securely flushes any 
 * pending messages stored in the async_pending table using rwlocks, and 
 * modifies the epoll event to listen for incoming responses (EPOLLIN | EPOLLONESHOT). 
 * If it fails, it safely aborts and cleans up the pending queue.
 * * @param ctx Pointer to the global ServerContext structure.
 * * @param curr_fd The file descriptor of the socket that finished connecting.
 * @return Void.
 */
void handle_connection_success(ServerContext* ctx, int curr_fd);

/**
 * @brief Triggers the periodic garbage collection for dead nodes.
 * * Reference: man 2 read.
 * Clears the GC timer and invokes the master function to scan the node 
 * registry for unresponsive peers. Dispatches any resulting timeout 
 * or disconnection messages through the outbox.
 * * @param ctx Pointer to the global ServerContext structure.
 * @return Void.
 */
void handle_gc_timer_expiration(ServerContext* ctx);

/**
 * @brief Dispatches a batch of outgoing network messages.
 * * Reference: man 2 send, man 2 connect.
 * Iterates through the master function's outbox. If a valid target_fd exists, 
 * it sends the message directly. If not, it initiates a new non-blocking 
 * TCP connection, safely enqueues the message in the async_pending table 
 * using write-locks, and registers the new FD with EPOLLIN | EPOLLOUT.
 * * @param ctx Pointer to the global ServerContext structure.
 * * @param outbox The array containing the messages and target metadata.
 * * @param outbox_count The number of valid messages in the outbox.
 * @return Void.
 */
void send_outbox(ServerContext* ctx, out_msg_t* outbox, int outbox_count);

/**
 * @brief Retrieves the IP address of the remote peer connected to a socket.
 * * Reference: man 2 getpeername, man 3 inet_ntoa.
 * Extracts the remote IPv4 address associated with an active socket and 
 * copies it as a null-terminated string into the provided buffer.
 * * @param fd The active file descriptor.
 * * @param ip_buffer A pre-allocated string buffer to hold the IPv4 address (min 16 chars).
 * @return Void.
 */
void get_ip_from_fd(int fd, char* ip_buffer);

#endif