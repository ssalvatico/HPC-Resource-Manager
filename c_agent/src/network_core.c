#include "../include/network_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h> 
#include <netinet/in.h>

#define LISTEN_BACKLOG 10

// por ahora es bloqueante
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

// estudiar y ver que onda

int make_socket_non_blocking(int sockfd){
    int flags = fcntl(sockfd, F_GETFL, 0);
    if(flags == -1) return -1;

    flags |= O_NONBLOCK;
    if(fcntl(sockfd, F_SETFL, flags) == -1) return -1;

    return 0;
}

int add_to_epoll_interest_list(int epoll_fd, int target_fd, uint32_t events){
    struct epoll_event event;
    event.data.fd = target_fd;
    event.events = events;

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, target_fd, &event) == -1) return -1;

    return 0;
}