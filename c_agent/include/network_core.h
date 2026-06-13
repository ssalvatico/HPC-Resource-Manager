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
 * @brief Actively connects to a remote TCP server.
 * * Reference: man 2 socket, man 2 connect.
 * This function acts as a TCP client. It creates a new socket and initiates 
 * a connection to the specified target IP and port. Once connected, this 
 * socket should be made non-blocking and added to the epoll interest list 
 * to wait for the remote node's responses asynchronously.
 * * @param target_ip The IPv4 address of the remote node (e.g., "192.168.1.15").
 * @param target_port The port where the remote node is listening.
 * @return The new file descriptor of the active connection, or -1 on error.
 */
int connect_to_tcp_node(const char* target_ip, int target_port);

/**
 * @brief Sends a null-terminated string message through an open TCP socket.
 * * Reference: man 2 send.
 * This function is universal: it is used BOTH to reply to connected clients 
 * (sending GRANTED/DENIED) AND to send requests to other nodes (RESERVE) 
 * once the connection is established.
 * * @param sockfd The open file descriptor (either from accept() or connect()).
 * @param message The protocol message to send (e.g., "GRANTED 1001\n").
 * @return The number of bytes sent, or -1 on error.
 */
int send_tcp_message(int sockfd, const char* message);

/**
 * @brief Creates a dual-purpose UDP socket for listening and broadcasting.
 * * Reference: man 2 socket, man 2 bind, man 7 udp, man 7 socket.
 * This function creates an IPv4 (AF_INET), UDP (SOCK_DGRAM) socket. 
 * It sets the SO_REUSEADDR option to allow immediate port reuse. 
 * Crucially, it also enables the SO_BROADCAST option, allowing this exact 
 * same file descriptor to transmit broadcast messages (e.g., ANNOUNCE) 
 * to the entire subnet. Finally, it binds the socket to all available 
 * network interfaces (INADDR_ANY).
 * * @param port The port number to bind the UDP socket.
 * @return The file descriptor referencing the socket, or -1 on error.
 */
int create_udp_listener_broadcaster(int port);

/**
 * @brief Transmits a UDP broadcast message to the entire local network.
 * * Reference: man 2 sendto, man 7 socket.
 * This function uses a UDP socket to send a datagram to the IPv4 broadcast 
 * address (INADDR_BROADCAST / 255.255.255.255). The underlying socket MUST have 
 * the SO_BROADCAST option enabled via setsockopt() before calling this function, 
 * otherwise the kernel will reject the transmission with an EACCES error.
 * * @param sockfd      A valid UDP socket file descriptor with SO_BROADCAST enabled.
 * @param targetport  The destination port where other agents are listening for UDP.
 * @param message      The null-terminated string to be broadcasted (e.g., "ANNOUNCE").
 * @return The number of bytes sent on success, or -1 on error.
 */
ssize_t broadcast_announce(int sockfd, int targetport, const char *message);
// ssize devuelve es justamente eso o los bytes enviados o -1

/**
 * @brief Processes an incoming discovery datagram and updates the live nodes table.
 * * Reference: man 2 recvfrom, man 3 inet_ntoa.
 * This function is invoked when the epoll loop detects incoming data on the 
 * UDP socket. It uses recvfrom() to read the payload and extract the sender's 
 * IP address. It implements echo-filtering by comparing the incoming payload 
 * against its own node ID. Valid nodes are added or updated in the internal registry.
 * * @param udp_sockfd The file descriptor of the UDP socket.
 * @param my_node_id The string identifier of this node (used to ignore self-echoes).
 * @return 0 on success, 1 if the message was a self-echo (ignored), or -1 on error.
 */
int process_discovery_datagram(int udpsockfd, const char* mynodeid);


/**
 * @brief Adds a file descriptor to the epoll interest list.
 * * Reference: man 2 epoll_ctl.
 * This function performs the EPOLL_CTL_ADD control operation on the 
 * epoll instance. It tells the kernel to monitor the target_fd for 
 * the events specified in the event mask (e.g., EPOLLIN).
 * * @param epollfd The file descriptor of the epoll instance.
 * @param targetfd The file descriptor of the socket to be monitored.
 * @param events The bit mask specifying the events of interest.
 * @return 0 on success, or -1 on error.
 */
int add_to_epoll_interest_list(int epollfd, int targetfd, uint32_t events);

#endif