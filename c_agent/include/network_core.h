#ifndef NETWORK_CORE_H
#define NETWORK_CORE_H

#include <sys/epoll.h>
#include <stdint.h>

/**
 * @brief Creates an endpoint for communication and binds it to a specified port.
 * * Reference: man 2 socket, man 2 bind, man 2 listen.
 * This function creates an IPv4 (AF_INET), TCP (SOCK_STREAM) socket, 
 * sets the SO_REUSEADDR option to prevent "Address already in use" errors, 
 * binds it to INADDR_ANY, and marks it as a passive socket that will be used 
 * to accept incoming connection requests using listen().
 * * @param port The port number to bind the listening socket.
 * @return The file descriptor referencing the socket, or -1 on error.
 */
int create_tcp_listener(int port);

/**
 * @brief Manipulates the file descriptor to enable non-blocking I/O.
 * * Reference: man 2 fcntl.
 * This function retrieves the current file status flags using F_GETFL 
 * and appends the O_NONBLOCK flag using F_SETFL. This ensures that operations 
 * like accept() or recv() return immediately with EAGAIN or EWOULDBLOCK 
 * instead of suspending the execution of the thread.
 * * @param sockfd The file descriptor of the socket to modify.
 * @return 0 on success, or -1 on error.
 */
int make_socket_non_blocking(int sockfd);

/**
 * @brief Adds a file descriptor to the epoll interest list.
 * * Reference: man 2 epoll_ctl.
 * This function performs the EPOLL_CTL_ADD control operation on the 
 * epoll instance. It tells the kernel to monitor the target_fd for 
 * the events specified in the event mask (e.g., EPOLLIN).
 * * @param epoll_fd The file descriptor of the epoll instance.
 * @param target_fd The file descriptor of the socket to be monitored.
 * @param events The bit mask specifying the events of interest.
 * @return 0 on success, or -1 on error.
 */
int add_to_epoll_interest_list(int epoll_fd, int target_fd, uint32_t events);

#endif