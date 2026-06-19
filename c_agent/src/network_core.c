#include "../include/network_core.h"
#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#define LISTEN_BACKLOG 10

int create_tcp_listener(const char* ip_address, int port){
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addrlen = sizeof(server_addr);
    // 1. Create socket, 
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

int connect_to_tcp_node(const char* target_ip, int target_port) {
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

int accept_tcp_connection(int server_fd, char* client_ip_buffer) {
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

    // make socket non blocking
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    // 4. return client fd
    return client_fd;
}
// the same for all tcp messages using an already connected socket
int send_tcp_message(int sockfd, const char* message) {
    // Don't generate a SIGPIPE signal if the peer on a tcp socket has closed the connection
    int bytes = send(sockfd, message, strlen(message), MSG_NOSIGNAL);
    return bytes;
}

ssize_t receive_tcp_message(int sockfd, char* buffer, size_t buffer_size){
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

int create_udp_listener_broadcaster(int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    // 1. create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)return -1;
    
    // 2. config socket
    int optval = 1; // 1 = Activado
    
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

ssize_t broadcast_announce(int sockfd, int targetport, const char *message){
    struct sockaddr_in destaddr;
    // 1. config
    memset(&destaddr, 0, sizeof(destaddr));
    destaddr.sin_family = AF_INET;
    destaddr.sin_addr.s_addr = INADDR_BROADCAST; // todas las computadoras conectadas a la red local
    destaddr.sin_port = htons(targetport);
    // 2. send
    return sendto(sockfd, message, strlen(message), 0,(struct sockaddr*)&destaddr, sizeof(destaddr));
}

int process_discovery_datagram(int udpsockfd, char* buffer, const int BUFFER_SIZE){
    struct sockaddr senderaddr;
    socklen_t addr_len = sizeof(senderaddr);
    ssize_t answer = recvfrom(udpsockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*) &senderaddr, &addr_len);
    
    if(answer == -1) return -1; // error

    return 0;

}

int add_to_epoll_interest_list(int epoll_fd, int target_fd, uint32_t events){
    struct epoll_event event;
    event.data.fd = target_fd;
    event.events = events;
    // add target fd to epoll interest list 
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, target_fd, &event) == -1) return -1;

    return 0;
}

int remove_from_epoll_interest_list(int epoll_fd, int target_fd){
    struct epoll_event event;
    event.data.fd = target_fd;
    return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, target_fd, &event);
}