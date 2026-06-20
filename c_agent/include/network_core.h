#ifndef NETWORK_CORE_H
#define NETWORK_CORE_H

#include <sys/epoll.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <stdint.h>
#include <sys/types.h>  
#include <stddef.h>

#define UDP_PORT 12529
#define SOCKET_ERROR -1
#define MAX_EVENTS 10
#define MAX_FDS 1024
#define BUFFER_SIZE 256

typedef struct {
    int port;
    const char* lan_ip;
    int epollfd;
    int tcp_public_fd;
    int erlang_tcp_fd;
    int udp_fd;
    int tcp_timerfd;
    int udp_timerfd;
} ServerContext;

typedef struct {
    int is_active;                      
    char ip[16];                        
    char pending_message[BUFFER_SIZE];  
} ConnectionState;

extern ConnectionState active_connections[MAX_FDS];

/**
 * @brief Creates a non blocking TCP listening socket bound to 
 * the specified port and ip address. 
 * * Reference: man 2 socket, man 2 bind, man 2 listen, man 2 fcntl.
 * This function creates an IPv4 (AF_INET), TCP (SOCK_STREAM) socket, 
 * sets the SO_REUSEADDR option to prevent "Address already in use" errors, 
 * binds it to INADDR_ANY, and marks it as a passive socket that will be used 
 * to accept incoming connection requests using listen(). Then appending 
 * O_NONBLOCK to the socket flags ensures that operations like accept() or recv()
 * return with EAGAIN or EWOULDBLOCK instead of suspending the execution of the thread
 * * @param port The port number to bind the listening socket.
 * @return The file descriptor referencing the socket, or -1 on error.
 */
int create_tcp_listener(const char* ip_address, int port);

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
int accept_tcp_connection(int server_fd, char* client_ip_buffer);

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
 * @brief Safely reads data from a non-blocking TCP socket.
 * * Reference: man 2 recv.
 * This function reads incoming data from a connected client socket 
 * into the provided buffer. Since the socket is non-blocking, it handles 
 * reading available bytes without suspending thread execution.
 * * @param sockfd The file descriptor of the connected client socket.
 * @param buffer The character array where the read data will be stored.
 * @param buffer_size The maximum capacity of the provided buffer.
 * @return The number of bytes read, 0 if the client gracefully closed the connection, 
 * or -1 on error (errno should be checked for EAGAIN/EWOULDBLOCK).
 */
ssize_t receive_tcp_message(int sockfd, char* buffer, size_t buffer_size);
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
int process_discovery_datagram(int udpsockfd, char* buffer, const int buffer_size, char* out_sender_ip);
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
/**
 * @brief Removes a file descriptor from the epoll monitoring list.
 * * Reference: man 2 epoll_ctl.
 * This function performs the EPOLL_CTL_DEL control operation. It tells 
 * the kernel to stop monitoring the target_fd. This function should generally 
 * be called before close(target_fd) when a client disconnects to prevent 
 * resource leaks or phantom events in the epoll loop.
 * * @param epoll_fd The file descriptor of the central epoll instance.
 * @param target_fd The file descriptor of the socket to be removed.
 * @return 0 on success, or -1 on error.
 */
int remove_from_epoll_interest_list(int epoll_fd, int target_fd);

/**
 * @brief Initializes the server context, sockets, and timers.
 * * Creates the epoll instance, sets up the TCP public listener, the local 
 * Erlang listener, and the UDP broadcast socket. It also arms the initial 
 * timers for delayed TCP activation and periodic UDP broadcasting.
 * * @param ctx Pointer to the ServerContext struct to be initialized.
 * @param argc Argument count from main.
 * @param argv Argument vector from main containing port and LAN IP.
 */
void init_server(ServerContext* ctx, int argc, char *argv[]);

/**
 * @brief Handles the expiration of the initial TCP timer.
 * * Consumes the timer file descriptor, removes it from epoll, and activates
 * the TCP listening sockets so the server begins accepting connections.
 * * @param ctx Pointer to the ServerContext containing the server state.
 */
void handle_tcp_timer_expiration(ServerContext* ctx);

/**
 * @brief Handles the expiration of the periodic UDP timer.
 * * Broadcasts the node's available resources to the local network using
 * the ANNOUNCE protocol.
 * * @param ctx Pointer to the ServerContext containing the server state.
 */
void handle_udp_timer_expiration(ServerContext* ctx);

/**
 * @brief Processes incoming UDP discovery datagrams.
 * * Reads the datagram, extracts the sender's IP, and ignores self-echoes.
 * Passes valid datagrams to the resource manager (Juani) to update the network map.
 * * @param ctx Pointer to the ServerContext containing the server state.
 */
void handle_incoming_discovery(ServerContext* ctx);

/**
 * @brief Accepts a new incoming TCP connection from the network or Erlang.
 * * Accepts the connection, makes the socket non-blocking, registers the 
 * client's IP in the active_connections array, and adds it to epoll for reading.
 * * @param ctx Pointer to the ServerContext containing the server state.
 * @param server_fd The listening socket file descriptor that triggered the event.
 */
void handle_new_tcp_connection(ServerContext* ctx, int server_fd);

/**
 * @brief Reads and processes an incoming message from a connected TCP client.
 * * Reads the buffer and passes it to the business logic module (Juani). 
 * Depending on the returned Action Code, it either replies synchronously, 
 * connects to a new node asynchronously, or remains silent. Handles graceful 
 * and abrupt disconnections.
 * * @param ctx Pointer to the ServerContext containing the server state.
 * @param curr_fd The file descriptor of the client who sent the data.
 */
void handle_client_message(ServerContext* ctx, int curr_fd);

/**
 * @brief Completes an asynchronous outgoing TCP connection (EPOLLOUT).
 * * Verifies if the non-blocking connect() was successful using getsockopt.
 * If successful, it dispatches the pending reservation message and switches
 * the socket's epoll interest from EPOLLOUT back to EPOLLIN.
 * * @param ctx Pointer to the ServerContext containing the server state.
 * @param curr_fd The file descriptor of the newly connected outgoing socket.
 */
void handle_async_connection_success(ServerContext* ctx, int curr_fd);

#endif