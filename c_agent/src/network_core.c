#include "../include/network_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h> 
#include <netinet/in.h>

#define LISTEN_BACKLOG 10

// por ahora es bloqueante y atiende a cualquier ip que se quiera conectar al puerto especificado
int create_tcp_listener(int port){
    int sockfd;
    struct sockaddr_in server_addr;
    
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
    server_addr.sin_addr.s_addr = INADDR_ANY;   // addr to listen

    // 3. bind socket 
    if(bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr))){
        close(sockfd);
        return -1;
    }

    // 4. Set socket to listen 
    if(listen(sockfd, LISTEN_BACKLOG)){
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// estudiar y ver que onda porque sirve para socket listener tcp y udp 

int make_socket_non_blocking(int sockfd){
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

int create_udp_listener(int port){
    int sockfd;

    // 1. Create socket 
    struct sockaddr_in server_addr;
    sockfd = socket(AF_INET, SOCK_DGRAM,0);
    if(sockfd == -1)return -1;

    // 2. Config socket

    // config to reuse address
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        close(sockfd);
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 3. bind socket
    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

// PARA DEFINIR EL BROADCASTER USAR EL MISMO SOCKET QUE EL DE ESCUCHA UDP