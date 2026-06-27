#ifndef SYS_SOCKETS_H
#define SYS_SOCKETS_H

#include "server_types.h"
#include <fcntl.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define LISTEN_BACKLOG 10


/**
 * @brief Creates a dual-purpose UDP socket for listening and broadcasting.
 * * Reference: man 2 socket, man 2 bind, man 7 udp, man 7 socket.
 * This function creates an IPv4 (AF_INET), UDP (SOCK_DGRAM) socket. 
 * It sets the SO_REUSEADDR option to allow immediate port reuse. 
 * Crucially, it also enables the SO_BROADCAST option, allowing this exact 
 * same file descriptor to receive broadcast messages (e.g., ANNOUNCE) 
 * from the entire subnet. It binds the socket to all available network
 * interfaces (INADDR_ANY).
 * * @param port The port number to bind the UDP socket.
 * @return The file descriptor referencing the socket, or -1 on error.
 */
int create_udp_listener_broadcaster(const int port);

/**
 * @brief Processes an incoming discovery datagram and extracts 
 * the sender's IP address.
 * * Reference: man 2 recvfrom, man 3 inet_ntoa.
 * This function is invoked when the epoll loop detects incoming data on the 
 * UDP socket. It uses recvfrom() to read the payload into the provided buffer 
 * and extracts the sender's IP address, copying it to out_sender_ip. 
 * The buffer is null-terminated to prevent string manipulation bugs.
 * * @param udpsockfd The file descriptor of the UDP socket.
 * * @param buffer The buffer where the received datagram payload will be stored.
 * * @param buffer_size The maximum size of the buffer.
 * * @param out_sender_ip A pre-allocated string buffer to output the sender's IPv4 address.
 * @return 0 on success, or -1 on error.
 */
int process_discovery_datagram(const int udpsockfd, char* buffer, const int buffer_size, char* out_sender_ip);

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
ssize_t broadcast_announce(int sockfd, const int targetport, const char *message);

/**
 * @brief Creates a non-blocking TCP listening socket bound to 
 * the specified port and IP address. 
 * * Reference: man 2 socket, man 2 bind, man 2 listen, man 2 fcntl.
 * This function creates an IPv4 (AF_INET), TCP (SOCK_STREAM) socket, 
 * sets the SO_REUSEADDR option to prevent "Address already in use" errors, 
 * and binds it to the specifically provided IP address. It marks it as a passive 
 * socket that will be used to accept incoming connection requests using listen(). 
 * Appending O_NONBLOCK to the socket flags ensures that operations like accept() 
 * or recv() return with EAGAIN or EWOULDBLOCK instead of suspending the thread.
 * * @param ip_address The string representation of the IPv4 address to bind to.
 * * @param port The port number to bind the listening socket.
 * @return The file descriptor referencing the socket, or -1 on error.
 */
int create_tcp_listener(const char* ip_address,const int port);

/**
 * @brief Accepts a new incoming TCP connection and extracts the client's IP.
 * * Reference: man 2 accept, man 3 inet_ntoa.
 * This function is called when the epoll loop detects an EPOLLIN event 
 * on the listening TCP socket. It accepts the connection and retrieves 
 * the client's IPv4 address as a string. The newly created client socket 
 * should then be made non-blocking and added to the epoll interest list.
 * * @param server_fd The file descriptor of the listening TCP socket.
 * @param client_ip_buffer A buffer where the client's IP address will be stored 
 * (must be at least INET_ADDRSTRLEN bytes long).
 * @return The file descriptor of the new client socket, or -1 on error.
 */
int accept_tcp_connection(const int server_fd, char* client_ip_buffer);

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
int connect_to_tcp_node(const char* target_ip,const int target_port);

/**
 * @brief Sends a null-terminated string message through an open TCP socket.
 * * Reference: man 2 send.
 * Must know the receiver fd.
 * * @param sockfd The open file descriptor
 * @param message The protocol message to send 
 * @return The number of bytes sent, or -1 on error.
 */
int send_tcp_message(const int sockfd, const char* message);

/**
 * @brief Safely reads data from a non-blocking TCP socket.
 * * Reference: man 2 recv.
 * This function reads incoming data from a connected client socket 
 * into the provided buffer. Since the socket is non-blocking, it handles 
 * reading available bytes without suspending thread execution.
 * * @param sockfd The file descriptor of the connected client socket.
 * @param buffer The character array where the read data will be stored.
 * @param buffer_size The maximum capacity of the provided buffer.
 * @return The number of bytes read, 0 if the client gracefully closed the connection, 
 * -2 if there is no new data (EAGAIN/EWOULDBLOCK), or -1 on actual error.
 */
ssize_t receive_tcp_message(const int sockfd, char* buffer,const size_t buffer_size);

#endif
