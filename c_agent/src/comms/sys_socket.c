#include "../../include/comms/sys_sockets.h"
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>

int create_udp_listener_broadcaster(const int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    // 1. create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)return -1;
    
    // 2. config socket
    int optval = 1; 
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        close(sockfd);
        return -1;
    }
    // allow broadcast
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
        close(sockfd);
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // 4. bind socket
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        close(sockfd);
        return -1;
    }
    
    // make socket non blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    return sockfd;
}

int process_discovery_datagram(const int udpsockfd, char* buffer, const int buffer_size, char* out_sender_ip) {
    struct sockaddr_in senderaddr; 
    socklen_t addr_len = sizeof(senderaddr);
    
    ssize_t answer = recvfrom(udpsockfd, buffer, buffer_size - 1, 0, (struct sockaddr*) &senderaddr, &addr_len);
    
    if(answer == -1) return -1; // error
    
    buffer[answer] = '\0'; // evita bugs
    if (out_sender_ip != NULL) {
        strcpy(out_sender_ip, inet_ntoa(senderaddr.sin_addr));
    }
    
    return 0;
}

ssize_t broadcast_announce(const int sockfd, const int targetport, const char *message){
    struct sockaddr_in destaddr;
    // 1. config
    memset(&destaddr, 0, sizeof(destaddr));
    destaddr.sin_family = AF_INET;
    destaddr.sin_addr.s_addr = INADDR_BROADCAST; // todas las computadoras conectadas a la red local
    destaddr.sin_port = htons(targetport);
    // 2. send
    return sendto(sockfd, message, strlen(message), 0,(struct sockaddr*)&destaddr, sizeof(destaddr));
}

int create_tcp_listener(const char* ip_address,const int port){
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addrlen = sizeof(server_addr);
    // 1. Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) return-1;
    
    // 2. Config socket
    
    // set socket options to reuse address and port for testing purposes
    int optval = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))){
        close(sockfd);
        return -1;
    }
    
    server_addr.sin_family = AF_INET;           // IPV4           
    server_addr.sin_port = htons(port);         // host to network short (port)
    server_addr.sin_addr.s_addr = inet_addr(ip_address);   // addr to listen
    
    // 3. bind socket 
    if(bind(sockfd, (struct sockaddr*) &server_addr, addrlen)){
        close(sockfd);
        return -1;
    }
    
    // 4. Set socket to listen 
    if(listen(sockfd, LISTEN_BACKLOG)){
        close(sockfd);
        return -1;
    }
    
    // MAKE SOCKET NON BLOCKING
    // get current flags
    int flags = fcntl(sockfd, F_GETFL, 0);

    if(flags == -1) {
        close(sockfd);
        return -1;
    };

    // set non-blocking flag
    flags |= O_NONBLOCK;
    if(fcntl(sockfd, F_SETFL, flags) == -1) {
        close(sockfd);
        return -1;
    };

    return sockfd;
}

int accept_tcp_connection(const int server_fd, char* client_ip_buffer) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // 1. accept
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd == -1) {
        return -1; 
    }

    // 2. write twh buffer
    if (client_ip_buffer != NULL) {
        // bytes to string
        char* ip_string = inet_ntoa(client_addr.sin_addr);
        strcpy(client_ip_buffer, ip_string);
    }
    // ACA PUEDO GUARDAS LOS FILE DESCRIPTORS
    // make socket non blocking
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    // 4. return client fd
    return client_fd;
}

int connect_to_tcp_node(const char* target_ip, const int target_port) {
    int sockfd;
    struct sockaddr_in remote_addr;
    
    // 1. create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return -1;
    }

    // make socket non blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // 2. config socket
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(target_port);
    remote_addr.sin_addr.s_addr = inet_addr(target_ip);
    

    // 3. connect socket puede interrumpirse el connect?
    if (connect(sockfd, (struct sockaddr*)&remote_addr, sizeof(remote_addr)) < 0) {
        // non blocking socket return -1 and set errno to EINPROGRESS socket still connectig
        if(errno != EINPROGRESS){
            close(sockfd);
            return -1;
        }
        return sockfd; // socket still connecting
    }
    
    return sockfd;
}

int send_tcp_message(const int sockfd, const char* message) {
    // Don't generate a SIGPIPE signal if the peer on a tcp socket has closed the connection
    int bytes = send(sockfd, message, strlen(message), MSG_NOSIGNAL);
    return bytes;
}

ssize_t receive_tcp_message(const int sockfd, char* buffer,const size_t buffer_size){
    ssize_t bytes = recv(sockfd, buffer, buffer_size - 1, 0); // Dejamos 1 byte para el \0
    if (bytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2; // No new data return to epoll, still waiting please
        }
        return -1; 
    }
    if (bytes == 0 ) return 0; 
    
    buffer[bytes] = '\0'; 
    return bytes;
}