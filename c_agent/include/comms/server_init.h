#ifndef SERVER_INIT_H
#define SERVER_INIT_H

#include <sys/timerfd.h>
#include <unistd.h>
#include "server_types.h"

/**
 * @brief Initializes the server context, sockets, and event timers.
 * * Reference: man 2 epoll_create1, man 2 timerfd_create, man 2 timerfd_settime.
 * This function bootstraps the server by parsing command-line arguments 
 * for the port and LAN IP address. It initializes a central epoll instance 
 * and sets up the primary communication sockets: a public TCP listener, 
 * a local TCP listener for Erlang, and a UDP socket for broadcasting. 
 * Additionally, it configures three non-blocking timer file descriptors 
 * (for TCP events, UDP announcements, and Garbage Collection) and registers 
 * them, along with the UDP socket, into the epoll interest list.
 * * @param ctx Pointer to the ServerContext structure to be populated.
 * * @param argc The number of command-line arguments.
 * * @param argv The array of command-line argument strings (expects [1]=port, [2]=IP).
 * @return Void. The program exits (EXIT_FAILURE) if arguments are invalid.
 */
void init_server(ServerContext* ctx, int argc, char *argv[]);

#endif